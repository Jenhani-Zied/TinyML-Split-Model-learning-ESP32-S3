#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

const char* ssid = "ESP32_AP";
const char* password = "12345678";

WiFiServer server(4210);
WiFiClient client;

uint8_t* buffer;

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("Receiver ready.");
  Serial.println(WiFi.softAPIP());

  // Allocate buffer in PSRAM
  const int CHUNK_SIZE = 1460;
  uint8_t buffer[CHUNK_SIZE];
  //buffer = (uint8_t*) ps_malloc(1500);


  server.begin();
}

void loop() {
  if (!client || !client.connected()) {
    client = server.available();
    return;
  }

  if (client.available()) {
    uint8_t buffer[1200];
    int len = client.read(buffer, sizeof(buffer));

    if (len > 0) {
      // Echo back the same data
      client.write(buffer, len);
    }
  }
}
