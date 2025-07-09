#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "iPhone";
const char* password = "abcd1234";

const char* receiverIP = "192.168.4.1";
const int receiverPort = 4210;
const int localPort = 4211;
WiFiUDP udp;

const int CHUNK_SIZE = 1460;
const int TOTAL_SIZE = 150528;
const int NUM_CHUNKS = (TOTAL_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE;

uint8_t* packet;  // PSRAM allocation for large packets
uint8_t* recvPacket;  // PSRAM allocation for receive buffer

// Statistics
float minRtt = 999999.0;
float maxRtt = 0.0;
float sumRtt = 0.0;

int receivedCount = 0;
int chunkLen = 0;

void preparePacket(uint32_t sequence) {
  memcpy(packet, &sequence, 4);  // Copy sequence number into the first 4 bytes
  for (int i = 4; i < CHUNK_SIZE; i++) {
    packet[i] = 'A' + (i % 26);  // Fill the remaining packet with dummy data
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("\nConnected to receiver AP");
  udp.begin(localPort);

  // Allocate buffers in PSRAM
  packet = (uint8_t*)ps_malloc(CHUNK_SIZE);
  recvPacket = (uint8_t*)ps_malloc(CHUNK_SIZE);
  if (!packet || !recvPacket) {
    Serial.println("PSRAM allocation failed!");
    while (1);
  }

  preparePacket(0);
}

void loop() {
  minRtt = 999999.0;
  maxRtt = 0.0;
  sumRtt = 0.0;
  receivedCount = 0;

  for (int chunk = 0; chunk < NUM_CHUNKS; chunk++) {
    chunkLen = (chunk < NUM_CHUNKS - 1) ? CHUNK_SIZE : (TOTAL_SIZE - chunk * CHUNK_SIZE);

    unsigned long sendTime = micros();
    udp.beginPacket(receiverIP, receiverPort);
    udp.write(packet, chunkLen);
    udp.endPacket();

    unsigned long timeout = micros() + 200000;  // 200 ms timeout
    bool received = false;
    while (micros() < timeout) {
      int packetSize = udp.parsePacket();
      if (packetSize > 0) {
        int len = udp.read(recvPacket, CHUNK_SIZE);
        if (memcmp(packet, recvPacket, len) == 0) {
          unsigned long rtt = micros() - sendTime;
          float rttHalf = rtt / 2000.0;
          if (rttHalf < minRtt) minRtt = rttHalf;
          if (rttHalf > maxRtt) maxRtt = rttHalf;
          sumRtt += rttHalf;

          Serial.printf("Chunk %d: RTT/2=%.3f ms (pure transmission), Length %d\n", chunk, rttHalf, chunkLen);
          received = true;
          receivedCount++;
          break;
        }
      }
    }

    if (!received) {
      Serial.printf("Chunk %d -> TIMEOUT\n", chunk);
    }
  }

  if (receivedCount > 0) {
    Serial.printf("\nStatistics:\n");
    Serial.printf("  Min RTT/2:    %.3f ms\n", minRtt);
    Serial.printf("  Max RTT/2:    %.3f ms\n", maxRtt);
    Serial.printf("  Avg RTT/2:    %.3f ms\n", sumRtt);
    Serial.printf("  Success rate: %d/%d (%.1f%%)\n", receivedCount, NUM_CHUNKS, (receivedCount * 100.0) / NUM_CHUNKS);
    Serial.printf("  RSSI: %d dBm\n", WiFi.RSSI());
  } else {
    Serial.println("No chunks received successfully!\n");
  }

  delay(2000);
}
