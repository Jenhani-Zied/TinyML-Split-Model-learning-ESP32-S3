#include <WiFi.h>
#include <esp_now.h>
bool test=false; 
//const uint8_t* mac = 0 ;
uint8_t mac[] = {0xCC, 0xBA, 0x97, 0x14, 0x17, 0x48};
void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  
/*const uint8_t* mac = recv_info->src_addr;
  Serial.printf("Received %d bytes from MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", 
    len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (!test){
          const uint8_t* mac = recv_info->src_addr;
          test = true ;
    }*/

  // Dynamically add peer if not already added
  if (!esp_now_is_peer_exist(mac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac, 6);
    peerInfo.channel = 0;  // Use 0 for auto
    peerInfo.encrypt = false;

    esp_err_t addResult = esp_now_add_peer(&peerInfo);
    if (addResult != ESP_OK) {
      Serial.printf("Failed to add peer, error: %d\n", addResult);
    } else {
      Serial.println("Peer added successfully.");
    }
  }

  // Echo back the received data
  esp_err_t result = esp_now_send(mac, data, len);
  /*if (result == ESP_OK) {
    Serial.println("Echo sent!");
  } else {
    Serial.printf("Echo failed, error: %d\n", result);
  }*/
}

void setup() {
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin();  // Ensure MAC is initialized
  delay(100);    // Give time for WiFi

  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true);
  }

  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  // Nothing to do, callback handles everything
}
