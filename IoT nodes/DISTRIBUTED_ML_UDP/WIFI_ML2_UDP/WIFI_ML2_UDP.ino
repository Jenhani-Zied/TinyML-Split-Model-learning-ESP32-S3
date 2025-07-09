#include <Arduino.h>
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "Second_Part_Model.h"
#include <WiFi.h>
#include <WiFiUdp.h>

// WiFi and UDP settings
const char* ssid = "iPhone";
const char* password = "abcd1234";
const int udpPort = 4210;
WiFiUDP udp;

// Model parameters
int buffer_index = 0;
bool buffer_mml = false;
bool startload = true;

constexpr int kTensorArenaSize2 = 1000 * 1024;
uint8_t* tensor_arena2 = nullptr;
float msg_ml = 0;
tflite::AllOpsResolver resolver;
#define BUFFER_SIZE 7*7*112
uint8_t intermediate_buffer[BUFFER_SIZE];
size_t bufferIndex = 0;

// Model instances
const tflite::Model* model_part2 = nullptr;
tflite::MicroInterpreter* interpreter_part2 = nullptr;

// Timing variables

uint8_t Predict_Buffer[10];

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
  uint32_t start_time = micros();
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
  IPAddress senderIP = udp.remoteIP();
  udp.beginPacket(senderIP, udpPort);
  udp.print(response);
  udp.endPacket();
  
  Serial.println("\n--- Prediction Results ---");
  Serial.println(response);
  Serial.println("Results sent back to sender");
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


void WiFi_Setup_send_data() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  udp.begin(udpPort);
}

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("Receiver ready.");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  udp.begin(udpPort);
  while (!Serial);
  //WiFi_Setup_send_data();
  ML_CONFIGURATION();
}

void loop() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    if (startload) {
      startload = false;
      Serial.println("\n--- Starting Data Reception ---");
    }
    
    // Print received packet info
    Serial.printf("Received packet of size %d from %s\n", 
                 packetSize, udp.remoteIP().toString().c_str());
    
    // Read the packet directly into our buffer
    int bytesRead = udp.read(&intermediate_buffer[bufferIndex], packetSize);
    
    // Print first 10 bytes of received data
    Serial.print("Data [");
    for(int i = 0; i < min(10, bytesRead); i++) {
      Serial.printf("%02X ", intermediate_buffer[bufferIndex + i]);
    }
    Serial.println("...]");
    
    bufferIndex += bytesRead;
    Serial.printf("Total bytes received: %d/%d\n", bufferIndex, BUFFER_SIZE);
    
    if (bufferIndex >= BUFFER_SIZE) {
      buffer_mml = true;
      bufferIndex = 0; // Reset for next transmission
      Serial.println("--- Complete Data Received ---");
      
      // Print verification of complete received data
      Serial.println("\nVerifying received data:");
      Serial.print("First 10 bytes: [");
      for(int i = 0; i < 10; i++) {
        Serial.printf("%02X ", intermediate_buffer[i]);
      }
      Serial.println("]");
      
      Serial.print("Last 10 bytes: [");
      for(int i = BUFFER_SIZE-10; i < BUFFER_SIZE; i++) {
        Serial.printf("%02X ", intermediate_buffer[i]);
      }
      Serial.println("]");
    }
  }

  if(buffer_mml) {
    Serial.println("\n--- Processing Received Data ---");
    ML_SPLIT_FUNCTION();
    buffer_mml = false;
  
  }
}
