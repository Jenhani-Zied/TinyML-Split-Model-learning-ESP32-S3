#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "iPhone";
const char* password = "abcd1234";

WiFiUDP udp;
const int localPort = 4210;

// Allocate large buffers dynamically in PSRAM
uint8_t* recvBuffer;
uint8_t* recvBuffer2;

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("Receiver ready.");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  udp.begin(localPort);

  // Allocate buffers in PSRAM
  recvBuffer = (uint8_t*) ps_malloc(150528);
  recvBuffer2 = (uint8_t*) ps_malloc(1284);

  if (!recvBuffer || !recvBuffer2) {
    Serial.println("PSRAM allocation failed!");
    while (1);  // Halt execution
  }
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(recvBuffer, 1460);  // Read into PSRAM buffer
    if (len > 0) {
      // Immediately echo back with minimal processing
      udp.beginPacket(udp.remoteIP(), udp.remotePort());
      udp.write(recvBuffer, len);
      udp.endPacket();
    }
  }
}
