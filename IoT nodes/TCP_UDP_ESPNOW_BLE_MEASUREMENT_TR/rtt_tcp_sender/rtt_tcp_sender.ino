#include <WiFi.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";
const char* serverIP = "192.168.4.1";
const int serverPort = 4210;

WiFiClient client;

const int CHUNK_SIZE = 1460;
const int TOTAL_SIZE = 150528;  // You can increase this further now
const int NUM_CHUNKS = (TOTAL_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE;

const int PACKET_SIZE = 1460;  // Smaller size to reduce latency
uint8_t packet[PACKET_SIZE];
uint8_t buffer[CHUNK_SIZE];
//uint8_t* buffer = nullptr;  // Allocated in PSRAM

uint32_t transferID = 0;

void preparePacket(uint32_t sequence) {
  memcpy(packet, &sequence, 4);  // Copy sequence number into the first 4 bytes
  for (int i = 4; i < CHUNK_SIZE; i++) {
    packet[i] = 'A' + (i % 26);
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

  // 🧠 Allocate large buffers in PSRAM
  //packet = (uint8_t*) ps_malloc(CHUNK_SIZE);
  //buffer = (uint8_t*) ps_malloc(CHUNK_SIZE);

  if (!packet || !buffer) {
    Serial.println("PSRAM allocation failed!");
    while (true);
  }

  if (!client.connect(serverIP, serverPort)) {
    Serial.println("Failed to connect to TCP server.");
    while (true);
  }
  client.setNoDelay(true);  // Disable Nagle to reduce latency

  preparePacket(0);
  Serial.println("Connected to TCP server.");
}

void loop() {
  float minRtt = 999999.0;
  float maxRtt = 0.0;
  float sumRtt = 0.0;
  int receivedCount = 0;

  for (int chunk = 0; chunk < NUM_CHUNKS; chunk++) {
    int chunkLen = (chunk < NUM_CHUNKS - 1) ? CHUNK_SIZE : (TOTAL_SIZE - chunk * CHUNK_SIZE);
    //preparePacket(transferID);  // Optional: regenerate payload for each chunk

    unsigned long sendTime = micros();
    client.write(packet, chunkLen);
    client.flush();

    unsigned long timeout = micros() + 200000;
    int totalRead = 0;
    while (micros() < timeout && totalRead < chunkLen) {
      if (client.available()) {
        totalRead += client.read(&buffer[totalRead], chunkLen - totalRead);
      }
    }

    if (totalRead == chunkLen) {
      unsigned long rtt = micros() - sendTime;
      float oneWay = rtt / 2000.0;
      sumRtt += oneWay;
      if (oneWay < minRtt) minRtt = oneWay;
      if (oneWay > maxRtt) maxRtt = oneWay;
      receivedCount++;
      Serial.printf("Chunk %d: RTT/2 = %.3f ms\n", chunk, oneWay);
    } else {
      Serial.printf("Chunk %d: TIMEOUT (received %d/%d bytes)\n", chunk, totalRead, chunkLen);
    }

    //delay(50);
  }

  Serial.printf("\nTCP Transfer %u complete.\n", transferID);
  Serial.printf("  Min RTT/2:    %.3f ms\n", minRtt);
  Serial.printf("  Max RTT/2:    %.3f ms\n", maxRtt);
  Serial.printf("  Avg RTT/2:    %.3f ms\n", sumRtt );
  Serial.printf("  Success rate: %d/%d (%.1f%%)\n\n", receivedCount, NUM_CHUNKS, (receivedCount * 100.0) / NUM_CHUNKS);

  transferID++;
  delay(3000);
}
