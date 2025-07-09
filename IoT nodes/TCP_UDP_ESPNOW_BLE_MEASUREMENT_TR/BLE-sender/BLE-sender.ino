#include <BLEDevice.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_SEND_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_RECEIVE_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a9"

BLERemoteCharacteristic* pSendChar = nullptr;
BLERemoteCharacteristic* pReceiveChar = nullptr;
BLEClient* pClient = nullptr;
class MyServerCallbacks: public BLEServerCallbacks {
    void onMTUChange(uint16_t MTU) {  // Removed the second parameter
        Serial.printf("Negotiated MTU: %d\n", MTU);
    }
};
bool connected = false;
const int CHUNK_SIZE = 512;
const int TOTAL_SIZE = 5488;  // Change to 5488 or others
const int NUM_CHUNKS = (TOTAL_SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE;
//uint8_t* dataBuffer;
//uint8_t* chunkBuffer;
uint8_t dataBuffer[TOTAL_SIZE];
uint8_t chunkBuffer[CHUNK_SIZE];
int totallengh = 0 ;
unsigned long sendTime = 0;
unsigned long rttSum = 0;
int successCount = 0;
int currentChunk = 0;
bool ackReceived = false;

void notifyCallback(BLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
  // Verify the echoed data matches the sent chunk
  bool match = true;
  int offset = currentChunk * CHUNK_SIZE;  // Find the offset of the current chunk
  int chunkLen = min(CHUNK_SIZE, TOTAL_SIZE - offset);

  if (length != chunkLen) {
    Serial.printf("Data length mismatch! Sent %d, received %d\n", chunkLen, length);
    match = false;
  } else {
    if (memcmp(pData, chunkBuffer, chunkLen) != 0) {
      Serial.println("Data content mismatch!");
      match = false;
    }
  }

  if (match) {
    unsigned long rtt = micros() - sendTime;
    Serial.printf("Chunk %d/%d: RTT/2 = %.3f ms (verified)\n", currentChunk, NUM_CHUNKS, rtt / 2000.0);
    rttSum += rtt / 2;
    successCount++;
    ackReceived = true;
  } else {
    Serial.printf("Chunk %d/%d: Data verification FAILED!\n", currentChunk, NUM_CHUNKS);
  }
}

void connectToServer() {
  BLEScan* scan = BLEDevice::getScan();
  BLEScanResults* pResults = scan->start(5);
  for (int i = 0; i < pResults->getCount(); i++) {
    BLEAdvertisedDevice device = pResults->getDevice(i);
    if (device.haveServiceUUID() && device.getServiceUUID().toString() == SERVICE_UUID) {
      Serial.println("Found server, connecting...");
      pClient = BLEDevice::createClient();
      if (pClient->connect(&device)) {
        Serial.println("Connected!");
        pClient->setMTU(517);  // Request specific MTU 
    // Wait a moment for MTU negotiation
        delay(100);  
        BLERemoteService* pService = pClient->getService(SERVICE_UUID);
        pSendChar = pService->getCharacteristic(CHAR_SEND_UUID);
        pReceiveChar = pService->getCharacteristic(CHAR_RECEIVE_UUID);
        if (pSendChar->canNotify()) pSendChar->registerForNotify(notifyCallback);
        connected = true;
      }
      break;
    }
  }
}


void setup() {
  Serial.begin(115200);
  BLEDevice::init("BLE_Sender");
  BLEDevice::setMTU(517); 
  Serial.print("MTU set to: ");
  Serial.println(BLEDevice::getMTU());

  connectToServer();
  /*if (!psramFound()) {
    Serial.println("PSRAM not found!");
    while (true);
  }
  dataBuffer = (uint8_t*) ps_malloc(TOTAL_SIZE);
  chunkBuffer = (uint8_t*) ps_malloc(CHUNK_SIZE);

  if (!dataBuffer || !chunkBuffer) {
    Serial.println("PSRAM allocation failed!");
    while (true);
  }*/
  // Fill test data
  for (int i = 0; i < TOTAL_SIZE; i++) dataBuffer[i] = i % 256;
}

void loop() {
  if (connected && currentChunk < NUM_CHUNKS) {
    int offset = currentChunk * CHUNK_SIZE;
    int chunkLen = min(CHUNK_SIZE, TOTAL_SIZE - offset);
    memcpy(chunkBuffer, dataBuffer + offset, chunkLen);

    ackReceived = false;
    sendTime = micros();
    pReceiveChar->writeValue(chunkBuffer, chunkLen);
    //Serial.printf("Sent %d bytes in Chunk %d\n", chunkLen, currentChunk + 1);  // <-- New print

    unsigned long timeout = millis() + 1000;
    while (!ackReceived && millis() < timeout) ;//delay(5);

    if (!ackReceived) Serial.printf("Chunk %d TIMEOUT\n", currentChunk + 1);
    totallengh += chunkLen ; 
    currentChunk++;
    //delay(10);  // Pacing
  }

  if (currentChunk >= NUM_CHUNKS) {
    Serial.printf("\nCompleted %d/%d chunks\n", successCount, NUM_CHUNKS);
    if (successCount > 0) {Serial.printf("Avg RTT/2: %.3f ms\n", (float)rttSum / 1000.0);
    Serial.printf("TOTAL Sent %d bytes\n", totallengh);  // <-- New print}
    totallengh = 0 ;
    }
    // Reset for next test after 2 seconds
    delay(3000);
    currentChunk = 0;
    successCount = 0;
    rttSum = 0;
  }
}
