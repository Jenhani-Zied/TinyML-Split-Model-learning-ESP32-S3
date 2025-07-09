#include <Arduino.h>
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "Second_Part_Model.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>

// WiFi settings
const char* ssid = "iPhone";
const char* password = "abcd1234";
const int tcpPort = 4210;
WiFiServer server(tcpPort);
WiFiClient client;

// Model parameters
#define BUFFER_SIZE (7*7*112)
uint8_t intermediate_buffer[BUFFER_SIZE];
size_t bufferIndex = 0;
bool buffer_mml = false;
bool startload = true;

// Timing variables

// Model instances
const tflite::Model* model_part2 = nullptr;
tflite::MicroInterpreter* interpreter_part2 = nullptr;
tflite::AllOpsResolver resolver;
constexpr int kTensorArenaSize2 = 1000 * 1024;
uint8_t* tensor_arena2 = nullptr;


void ML_CONFIGURATION() {
  if (!psramFound()) {
    Serial.println("Error: PSRAM not found!");
    while (1);
  }

  tensor_arena2 = (uint8_t*)heap_caps_malloc(kTensorArenaSize2, MALLOC_CAP_SPIRAM);
  if (!tensor_arena2) {
    Serial.println("Failed to allocate tensor arena!");
    while (1);
  }

  model_part2 = tflite::GetModel(Second_Part_Model_quant_tflite);
  if (model_part2->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model Part 2 schema mismatch!");
    while(1);
  }
  
 
  interpreter_part2 = new tflite::MicroInterpreter(model_part2, resolver, tensor_arena2, kTensorArenaSize2);

  if (interpreter_part2->AllocateTensors() != kTfLiteOk) {
    Serial.println("Tensor allocation failed!");
    while (1);
  }
 
}

void ML_SPLIT_FUNCTION() {
  TfLiteTensor* input_part2 = interpreter_part2->input(0);
  
  if (input_part2->dims->data[1] != 7 || input_part2->dims->data[2] != 7 || 
      input_part2->dims->data[3] != 112) {
    Serial.println("Unexpected input shape!");
    while(1);
  }
   
  memcpy(input_part2->data.uint8, intermediate_buffer, BUFFER_SIZE);
  

  if (interpreter_part2->Invoke() != kTfLiteOk) {
    Serial.println("Inference failed");
    while(1);
  }
 

  TfLiteTensor* output_part2 = interpreter_part2->output(0);
  
  struct Prediction { uint8_t index; uint8_t score; };
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

  // Send predictions back
  String response = "Predictions: ";
  for (int i = 0; i < 5; i++) {
    response += "Class ";
    response += top5[i].index;
    response += " (";
    response += top5[i].score;
    response += "%)";
    if (i < 4) response += ", ";
  }
  
  client.println(response);
  Serial.println("\n--- Prediction Results ---");
  Serial.println(response);
}

void setup() {
  Serial.begin(115200);
  Serial.println("First Test");
  // Set up WiFi in AP mode
  WiFi.softAP(ssid, password);
  Serial.println("Receiver ready");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  
  server.begin();
  ML_CONFIGURATION();
}

void loop() {
  if (!client || !client.connected()) {
    client = server.available();
    if (client) {
      Serial.println("New client connected");
      bufferIndex = 0;
      startload = true;
    }
  } else {
    while (client.available()) {
      if (startload) {
        startload = false;
        Serial.println("\n--- Receiving Data ---");
      }
      
      size_t bytesAvailable = client.available();
      size_t bytesToRead = min(bytesAvailable, BUFFER_SIZE - bufferIndex);
      
      int bytesRead = client.read(&intermediate_buffer[bufferIndex], bytesToRead);
      
      Serial.printf("Received %d bytes (Total: %d/%d)\n", bytesRead, bufferIndex+bytesRead, BUFFER_SIZE);
      bufferIndex += bytesRead;
      
      if (bufferIndex >= BUFFER_SIZE) {
        buffer_mml = true;
        Serial.println("--- Complete Data Received ---");
        
        // Verify data
        Serial.print("First 10 bytes: ");
        for(int i=0; i<10; i++) Serial.printf("%02X ", intermediate_buffer[i]);
        Serial.println();
      }
    }

    if(buffer_mml) {
      Serial.println("--- Processing Data ---");
      ML_SPLIT_FUNCTION();
      buffer_mml = false;
      Serial.println("..Finish Test..");
    }
  }
}
