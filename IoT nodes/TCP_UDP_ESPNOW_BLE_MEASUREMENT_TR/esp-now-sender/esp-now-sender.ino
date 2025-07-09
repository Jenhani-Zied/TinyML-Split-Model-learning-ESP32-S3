#include <WiFi.h>
#include <esp_now.h>

#define CHUNK_SIZE 250
#define TOTAL_SIZE 5488
#define NUM_CHUNKS ((TOTAL_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE)

uint8_t receiverMAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // Replace with your receiver MAC
extern "C" {
  #include "esp_wifi.h"
}
uint8_t dataBuffer[TOTAL_SIZE];
uint8_t chunkBuffer[CHUNK_SIZE];

volatile bool ackReceived = false;
volatile int ackLen = 0;

unsigned long sendTime = 0;
unsigned long rtt = 0;

void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  //const uint8_t* mac = recv_info->src_addr;  // Get sender MAC
  if (len == ackLen && memcmp(data, dataBuffer, len) == 0) {
    rtt = micros() - sendTime;
    ackReceived = true;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true);
  }
//esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(onDataRecv);

  // Fill dataBuffer with dummy data
  for (int i = 0; i < CHUNK_SIZE; i++) {
    dataBuffer[i] = 'A' + (i % 26);
  }

  Serial.println("Sender ready!");
}

void loop() {
  float minRtt = 999999.0;
  float maxRtt = 0.0;
  float sumRtt = 0.0;
  int successCount = 0;

  for (int chunk = 0; chunk < NUM_CHUNKS; chunk++) {
    int chunkLen = (chunk < NUM_CHUNKS - 1) ? CHUNK_SIZE : (TOTAL_SIZE - chunk * CHUNK_SIZE);
    //memcpy(chunkBuffer, &dataBuffer[chunk * CHUNK_SIZE], chunkLen);

    ackReceived = false;
    ackLen = chunkLen;
    sendTime = micros();
    esp_now_send(receiverMAC, dataBuffer, chunkLen);

    unsigned long timeout = micros() + 500000;  // 500 ms
    while (!ackReceived && micros() < timeout) {
      //delayMicroseconds(10);
    }

    if (ackReceived) {
      float rttHalf = rtt / 2000.0;
      sumRtt += rttHalf;
      if (rttHalf < minRtt) minRtt = rttHalf;
      if (rttHalf > maxRtt) maxRtt = rttHalf;
      successCount++;
      Serial.printf("Chunk %d: RTT/2 = %.3f ms\n", chunk, rttHalf);
    } else {
      Serial.printf("Chunk %d: TIMEOUT\n", chunk);
    }

    //delay(10);  // Optional pacing
  }

  Serial.printf("\nFull transfer complete:\n");
  Serial.printf("  Min RTT/2:    %.3f ms\n", minRtt);
  Serial.printf("  Max RTT/2:    %.3f ms\n", maxRtt);
  Serial.printf("  Avg RTT/2:    %.3f ms\n", sumRtt );
  Serial.printf("  Success rate: %d/%d (%.1f%%)\n\n", successCount, NUM_CHUNKS, (successCount * 100.0) / NUM_CHUNKS);

  delay(3000);
}
