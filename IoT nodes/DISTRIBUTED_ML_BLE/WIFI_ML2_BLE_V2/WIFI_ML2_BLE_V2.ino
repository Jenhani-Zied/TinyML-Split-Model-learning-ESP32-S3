#include <Arduino.h>
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "Second_Part_Model.h"
#include <BLEDevice.h>
#include <BLEServer.h>


//BLE-CODE

int buffer_index = 0;
bool buffer_mml = false;
bool startload = true;
float Delay_Calculation = 0;
float Start_Delay_Calculation = 0 ;
constexpr int kTensorArenaSize2 = 1000 * 1024;
uint8_t* tensor_arena2 = nullptr;
float msg_ml = 0;
tflite::AllOpsResolver resolver;
#define BUFFER_SIZE 7*7*112
uint8_t intermediate_buffer[BUFFER_SIZE];
size_t bufferIndex = 0;
float BLESetup_Delay = 0 ;
// Model instances
const tflite::Model* model_part2 = nullptr;
tflite::MicroInterpreter* interpreter_part2 = nullptr;

// Timing variables 

uint8_t Predict_Buffer[10];

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
  /*void onWrite(BLECharacteristic* pChar) {
    String data = pChar->getValue();
    pSendChar->setValue((uint8_t*)data.c_str(), data.length());
    pSendChar->notify();
    Serial.printf("Echoed %d bytes\n", data.length());
  }*/
  void onWrite(BLECharacteristic* pChar) {
  String data = pChar->getValue();
  int len = data.length();
  if (len) {
    if (startload) {
      Start_Delay_Calculation = micros();
      startload = false;
      //Serial.println("\n--- Starting Data Reception ---");
    }
    
    // Print received packet info
    /*Serial.printf("Received packet of size %d from %s\n", 
                 len, data);*/
    
    // Read the packet directly into our buffer
    memcpy(&intermediate_buffer[bufferIndex],(const uint8_t*)data.c_str(),len);
    
    // Print first 10 bytes of received data
    /*Serial.print("Data [");
    for(int i = 0; i < min(10, len); i++) {
      Serial.printf("%02X ", intermediate_buffer[bufferIndex + i]);
    }
    Serial.println("...]");*/
    
    bufferIndex += len;
    //Serial.printf("Total bytes received: %d/%d\n", bufferIndex, BUFFER_SIZE);
    
    if (bufferIndex >= BUFFER_SIZE) {
      buffer_mml = true;
      bufferIndex = 0; // Reset for next transmission
      //Serial.println("--- Complete Data Received ---");
      
      // Print verification of complete received data
      //Serial.println("\nVerifying received data:");
      //Serial.print("First 10 bytes: [");
      /*for(int i = 0; i < 10; i++) {
        Serial.printf("%02X ", intermediate_buffer[i]);
      }*/
      //Serial.println("]");
      
      /*Serial.print("Last 10 bytes: [");
      for(int i = BUFFER_SIZE-10; i < BUFFER_SIZE; i++) {
        Serial.printf("%02X ", intermediate_buffer[i]);
      }*/
      //Serial.println("]");
    }
  }
  }
};

void logMemoryInfo() {
  Serial.print("Free Heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" | Min Free Heap: ");
  Serial.print(ESP.getMinFreeHeap());
  
  if (psramFound()) {
    Serial.print(" | Free PSRAM: ");
    Serial.print(ESP.getFreePsram());
    Serial.print(" | Min Free PSRAM: ");
    Serial.println(ESP.getMinFreePsram());
  } else {
    Serial.println(" | PSRAM not available");
  }
}

void ML_CONFIGURATION() {
  if (!psramFound()) {
    Serial.println("Error: PSRAM not found! Enable PSRAM in Arduino IDE: Tools > PSRAM > Enabled");
    while (1);
  }

  tensor_arena2 = (uint8_t*)heap_caps_malloc(kTensorArenaSize2, MALLOC_CAP_SPIRAM);
  if (!tensor_arena2) {
    Serial.println("Failed to allocate tensor arena in PSRAM!");
    while (1);
  }

  model_part2 = tflite::GetModel(Second_Part_Model_quant_tflite);
  if (model_part2->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model Part 2 schema mismatch!");
    while(1);
  }
  
  interpreter_part2 = new tflite::MicroInterpreter(
    model_part2, resolver, tensor_arena2, kTensorArenaSize2);

  if (interpreter_part2->AllocateTensors() != kTfLiteOk) {
    Serial.print("Tensor allocation failed! Requested: ");
    Serial.print(interpreter_part2->arena_used_bytes());
    Serial.print(" bytes, Available: ");
    Serial.print(kTensorArenaSize2);
    Serial.println(" bytes");
    while (1);
  }
}

void ML_SPLIT_FUNCTION() {
  TfLiteTensor* input_part2 = interpreter_part2->input(0);
  
  if (input_part2->dims->data[1] != 7 || 
      input_part2->dims->data[2] != 7 || 
      input_part2->dims->data[3] != 112) {
    Serial.println("Unexpected input shape for Part 2!");
    while(1);
  }
   
  memcpy(interpreter_part2->input(0)->data.uint8, intermediate_buffer, 5488 * sizeof(uint8_t));
  
  if (interpreter_part2->Invoke() != kTfLiteOk) {
    Serial.println("Part 2 inference failed");
    while(1);
  }

  TfLiteTensor* output_part2 = interpreter_part2->output(0);
  
  struct Prediction {
    uint8_t index;
    uint8_t score;
  };
  Prediction top5[5] = {{0,0}};

  for (int i = 0; i < output_part2->dims->data[1]; i++) {
    uint8_t score = output_part2->data.uint8[i];
    if (score > top5[4].score) {
      top5[4] = {static_cast<uint8_t>(i), score};
      for (int j = 4; j > 0 && top5[j].score > top5[j-1].score; j--) {
        Prediction temp = top5[j-1];
        top5[j-1] = top5[j];
        top5[j] = temp;
      }
    }
  }
  
  buffer_mml = false;
  // Prepare response message with predictions
  String response = "Predictions: ";
  for (int i = 0; i < 5; i++) {
    response += "Class ";
    response += top5[i].index;
    response += " (";
    response += top5[i].score;
    response += "%)";
    if (i < 4) response += ", ";
  }

  // Send response back to sender
  pSendChar->setValue((uint8_t*)response.c_str(), response.length());
  pSendChar->notify();
  Serial.printf("Sent %d bytes via BLE notify\n", response.length());
  //Serial.println("\n--- Prediction Results ---");
  //Serial.println(response);
  //Serial.println("Results sent back to sender");
  for (int i = 0; i < 5; i++) {
    Predict_Buffer[i * 2] = top5[i].index;
    Predict_Buffer[i * 2 + 1] = top5[i].score;
  }
  
  Serial.println("Top 5 predictions:");
  for (int i = 0; i < 5; i++) {
    Serial.printf("%d: Class %d (Score: %d)\n", 
                 i+1, top5[i].index, top5[i].score);
  }
}



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
  while (!Serial);
  //WiFi_Setup_send_data();
  
  ML_CONFIGURATION();
}

void loop() {

    if(buffer_mml) {
    
    //Serial.println("\n--- Processing Received Data ---");
    ML_SPLIT_FUNCTION();
    
   //buffer_mml = false;
 
  }
}
