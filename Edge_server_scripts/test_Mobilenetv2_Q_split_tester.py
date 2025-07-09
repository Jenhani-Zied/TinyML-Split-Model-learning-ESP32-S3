import numpy as np
from PIL import Image
from tflite_runtime.interpreter import Interpreter
import time
# === Load TFLite models ===
start_time = time.perf_counter()
feature_interpreter = Interpreter(model_path="First_Part_Model_quant.tflite")
interpreter_creation_time1 = time.perf_counter() - start_time
start_time = time.perf_counter()
feature_interpreter.allocate_tensors()
allocation_time1 = time.perf_counter() - start_time

start_time = time.perf_counter()
classifier_interpreter = Interpreter(model_path="Second_Part_Model_quant.tflite")
interpreter_creation_time2 = time.perf_counter() - start_time

start_time = time.perf_counter()
classifier_interpreter.allocate_tensors()
allocation_time2 = time.perf_counter() - start_time
# Print interpreter + allocation times

feature_out_info = feature_interpreter.get_output_details()[0]
print("Feature output quantization:", feature_out_info['quantization'])
classifier_in_info = classifier_interpreter.get_input_details()[0]
print("Classifier input quantization:", classifier_in_info['quantization'])
# === Load and preprocess input image ===
start_time = time.perf_counter()
img = Image.open("bergerall.jpg").resize((224, 224)).convert("RGB")
input_data = np.asarray(img, dtype=np.uint8)
input_data = np.expand_dims(input_data, axis=0)  # Shape: (1, 224, 224, 3)
LoadImage_time = time.perf_counter() - start_time
# === Run first model (Feature Extractor) ===
feature_input_index = feature_interpreter.get_input_details()[0]['index']
feature_interpreter.set_tensor(feature_input_index, input_data)
start_time = time.perf_counter()
feature_interpreter.invoke()
inference_time1 = time.perf_counter() - start_time
feature_output = feature_interpreter.get_tensor(feature_interpreter.get_output_details()[0]['index'])

# === Run second model (Classifier) ===
classifier_input_index = classifier_interpreter.get_input_details()[0]['index']
classifier_interpreter.set_tensor(classifier_input_index, feature_output)
start_time = time.perf_counter()
classifier_interpreter.invoke()
inference_time2 = time.perf_counter() - start_time
classifier_output = classifier_interpreter.get_tensor(classifier_interpreter.get_output_details()[0]['index'])

# === Top-5 Predictions (raw uint8 like ESP32) ===
top5 = sorted([(i, score) for i, score in enumerate(classifier_output[0])],
              key=lambda x: x[1], reverse=True)[:5]

print("Top 5 predictions (raw uint8):")
for rank, (cls, score) in enumerate(top5, start=1):
    print(f"{rank}: Class {cls} (Score: {score})")

# === Print Inference Timings ===
print(f"Feature Interpreter creation time load model: {interpreter_creation_time1 * 1000:.3f} ms")
print(f"Feature allocate_tensors time: {allocation_time1 * 1000:.3f} ms")
print(f"Classifier Interpreter creation time load model: {interpreter_creation_time2 * 1000:.3f} ms")
print(f"Classifier allocate_tensors time: {allocation_time2 * 1000:.3f} ms")
print(f"LoadImage_time: {LoadImage_time * 1000:.3f} ms")
print(f"Feature model inference time: {inference_time1 * 1000:.3f} ms")
print(f"Classifier model inference time: {inference_time2 * 1000:.3f} ms")