#include <BLEDevice.h>
#include <BLEServer.h>

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_SEND_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_RECEIVE_UUID   "beb5483e-36e1-4688-b7f5-ea07361b26a9"

BLECharacteristic* pSendChar;
class MyServerCallbacks: public BLEServerCallbacks {
    void onMTUChange(uint16_t MTU) {  // Removed the second parameter
        Serial.printf("Negotiated MTU: %d\n", MTU);
    }
};
class ReceiveCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) {
    String data = pChar->getValue();
    pSendChar->setValue((uint8_t*)data.c_str(), data.length());
    pSendChar->notify();
    Serial.printf("Echoed %d bytes\n", data.length());
  }
};



void setup() {
  Serial.begin(115200);
  BLEDevice::init("BLE_Receiver");
  BLEDevice::setMTU(517);
  Serial.print("MTU set to: ");
  Serial.println(BLEDevice::getMTU());
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);

  pSendChar = pService->createCharacteristic(
    CHAR_SEND_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  BLECharacteristic* pReceiveChar = pService->createCharacteristic(
    CHAR_RECEIVE_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  pReceiveChar->setCallbacks(new ReceiveCallback());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Receiver ready!");
}

void loop() {}
