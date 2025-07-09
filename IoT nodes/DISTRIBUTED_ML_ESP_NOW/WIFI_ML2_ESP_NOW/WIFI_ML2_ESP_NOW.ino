#include <Arduino.h>
#include <Chirale_TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "Second_Part_Model.h"
#include <esp_now.h>
#include <WiFi.h>

// Model parameters
#define BUFFER_SIZE (7*7*112)
uint8_t intermediate_buffer[BUFFER_SIZE];
uint8_t incomingData[250];
size_t bufferIndex = 0;
bool buffer_mml = false;
bool startload = true;

// Timing variables
uint8_t senderMac[] ={0xCC, 0xBA, 0x97, 0x14, 0x17, 0x48};
float Receive_ESPNOW_Delay = 0;
float Load_Input_Delay = 0;
float LoadModel_Delay = 0;
float Allocations_Tensors_Delay = 0;
float Prediction_Delay = 0;
float Inference_Delay = 0;
float Connection_Wifi_Delay = 0;
float Start_Receive_ESPNOW_Delay = 0;

// Model instances
const tflite::Model* model_part2 = nullptr;
tflite::MicroInterpreter* interpreter_part2 = nullptr;
tflite::AllOpsResolver resolver;
constexpr int kTensorArenaSize2 = 1000 * 1024;
uint8_t* tensor_arena2 = nullptr;

void PRINT_DELAY() {
  Serial.print("LoadINPUT_PART1_Delay in ms-->"); Serial.println(Load_Input_Delay);
  Serial.print("LoadModel_Delay in ms-->"); Serial.println(LoadModel_Delay);
  Serial.print("Allocations_Tensors_Delay in ms-->"); Serial.println(Allocations_Tensors_Delay);
  Serial.print("Inference_Delay in ms-->"); Serial.println(Inference_Delay);
  Serial.print("Prediction_Delay in ms-->"); Serial.println(Prediction_Delay);
  Serial.print("Receive_ESPNOW_Delay in ms-->"); Serial.println(Receive_ESPNOW_Delay);
  //Serial.print("Connection_Wifi_Delay in ms-->"); Serial.println(Connection_Wifi_Delay);
  
  Serial.print("Total consumption in ms -->"); 
  Serial.println(Receive_ESPNOW_Delay + Prediction_Delay + Inference_Delay + 
                Allocations_Tensors_Delay + LoadModel_Delay + Load_Input_Delay );
}

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

  uint32_t Start_LoadModel_Delay = micros();
  model_part2 = tflite::GetModel(Second_Part_Model_quant_tflite);
  if (model_part2->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model Part 2 schema mismatch!");
    while(1);
  }
  LoadModel_Delay = (micros() - Start_LoadModel_Delay)/1000.0;
  
  
  interpreter_part2 = new tflite::MicroInterpreter(model_part2, resolver, tensor_arena2, kTensorArenaSize2);
  uint32_t Start_Allocations_Tensors_Delay = micros();
  if (interpreter_part2->AllocateTensors() != kTfLiteOk) {
    Serial.println("Tensor allocation failed!");
    while (1);
  }
  Allocations_Tensors_Delay = (micros() - Start_Allocations_Tensors_Delay)/1000;
}

void ML_SPLIT_FUNCTION() {
  uint32_t Start_Load_Input_Delay = micros();
  TfLiteTensor* input_part2 = interpreter_part2->input(0);
  
  if (input_part2->dims->data[1] != 7 || input_part2->dims->data[2] != 7 || 
      input_part2->dims->data[3] != 112) {
    Serial.println("Unexpected input shape!");
    while(1);
  }
   
  memcpy(input_part2->data.uint8, intermediate_buffer, BUFFER_SIZE);
  Load_Input_Delay = (micros() - Start_Load_Input_Delay)/1000;
  
  uint32_t Start_Inference_Delay = micros();
  if (interpreter_part2->Invoke() != kTfLiteOk) {
    Serial.println("Inference failed");
    while(1);
  }
  Inference_Delay = (micros() - Start_Inference_Delay)/1000;

  uint32_t Start_Prediction_Delay = micros();
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
  
  Prediction_Delay = (micros() - Start_Prediction_Delay)/1000;
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
  
  // Get sender MAC from received data
  
  //memcpy(senderMac, esp_now_sender, 6);
  
  esp_now_send(senderMac, (uint8_t *)response.c_str(), response.length());
  Serial.println("1");
  Serial.println(response.length());
  Serial.println("\n--- Prediction Results ---");
  Serial.println(response);
}

// Callback when data is received
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (startload) {
    Start_Receive_ESPNOW_Delay = micros();
    startload = false;
    Serial.println("\n--- Receiving Data ---");
  }
  
  // Check if we have space in buffer
  if (bufferIndex + len > BUFFER_SIZE) {
    //Serial.println("Buffer overflow!");
    return;
  }
  
  memcpy(&intermediate_buffer[bufferIndex], data, len);
  bufferIndex += len;
  
  Serial.printf("Received %d bytes (Total: %d/%d)\n", len, bufferIndex, BUFFER_SIZE);
  
  if (bufferIndex >= BUFFER_SIZE) {
    
    Receive_ESPNOW_Delay = (micros() - Start_Receive_ESPNOW_Delay)/1000.0;
    buffer_mml = true;
    Serial.println("--- Complete Data Received ---");
    
    // Verify data

  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("First Test");
  uint32_t Start_Every_Thing = micros();
  // Set device as WiFi Station
  uint32_t Start_Connection_Wifi_Delay = micros();
  WiFi.mode(WIFI_STA);
  Serial.print("MAC Address: "); Serial.println(WiFi.macAddress());
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    while(1);
  }
    if (!esp_now_is_peer_exist(senderMac)) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, senderMac, 6);
    peerInfo.channel = 0;  // Use 0 for auto
    peerInfo.encrypt = false;

    esp_err_t addResult = esp_now_add_peer(&peerInfo);
    if (addResult != ESP_OK) {
      Serial.printf("Failed to add peer, error: %d\n", addResult);
    } else {
      Serial.println("Peer added successfully.");
    }
  }
  // Register callback
  Connection_Wifi_Delay = (micros() - Start_Connection_Wifi_Delay) / 1000 ;
  esp_now_register_recv_cb(OnDataRecv);
  
  
  ML_CONFIGURATION();
}

void loop() {
  if(buffer_mml) {
    Serial.println("--- Processing Data ---");
    ML_SPLIT_FUNCTION();
    buffer_mml = false;
    PRINT_DELAY();
    //Serial.print("the data input: ");
    //for(int i=0; i<5488; i++) Serial.printf("%02X ", intermediate_buffer[i]);
    //Serial.println();
    Serial.println("..Finish Test..");
  }
}