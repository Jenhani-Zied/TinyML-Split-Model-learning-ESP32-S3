import numpy as np
import time
import platform
from tflite_runtime.interpreter import Interpreter, load_delegate
from PIL import Image
test_dog = [

]
# === Equivalent to 'extern const uint8_t mobilenet_esp32_tflite[];' ===
MODEL_PATH = "mobilenet_esp32_s3.tflite"  # Path to your quantized model

# === Load test image as NumPy array like `test_cat[]` ===
def load_test_image_array():
    start_time = time.perf_counter()
    img = Image.open("bergerall.jpg").convert("RGB").resize((224, 224))
    data = np.asarray(img, dtype=np.uint8)  # Already in [0,255]
    load_time = (time.perf_counter() - start_time) * 1000
    print(f"Image load + preprocess time: {load_time:.2f} ms")
    return data

# === Print memory info (ESP32 style) ===
def log_memory_info():
    import os, psutil
    process = psutil.Process(os.getpid())
    print(f"Platform: {platform.system()} | Memory Used: {process.memory_info().rss / 1024:.2f} KB")

# === Mimic ESP32 logic ===
def run_mobilenet_inference():
    print("\nStarting MobileNet Inference (Python equivalent)")
    log_memory_info()

    # Load the model
    start_time = time.perf_counter()
    interpreter = Interpreter(model_path=MODEL_PATH)
    load_model_time = (time.perf_counter() - start_time) * 1000
    print(f"Model load time: {load_model_time:.2f} ms")

    start_time = time.perf_counter()
    interpreter.allocate_tensors()
    allocate_time = (time.perf_counter() - start_time) * 1000
    print(f"Tensor allocation time: {allocate_time:.2f} ms")
    # Get model input/output
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    print("Input Quant:", input_details[0]['quantization'])
    print("Output Quant:", output_details[0]['quantization'])
    input_shape = input_details[0]['shape']
    input_dtype = input_details[0]['dtype']
    output_dtype = output_details[0]['dtype']
    input_scale, input_zero_point = input_details[0]['quantization']
    output_scale, output_zero_point = output_details[0]['quantization']


    print(f"Input shape: {input_shape}, dtype: {input_dtype}")
    assert input_shape[1] == 224 and input_shape[2] == 224 and input_shape[3] == 3, "Model input shape mismatch!"

    # === Load test image (like copying test_cat to input tensor) ===
    input_data = load_test_image_array()
    input_data = np.expand_dims(input_data, axis=0)
    #input_data = np.array(test_dog, dtype=np.uint8).reshape((1, 224, 224, 3))
    assert input_data.dtype == np.uint8, "Input must be raw uint8 data!"
    
    interpreter.set_tensor(input_details[0]['index'], input_data)

    # === Run inference and time it ===
    print("Running inference...")
    start_time = time.time()
    start_timex = time.perf_counter()
    interpreter.invoke()
    inference_time = (time.perf_counter() - start_timex) * 1000
    duration_ms = (time.time() - start_time) * 1000
    print(f"Inference completed in {duration_ms:.2f} ms")
    print(f"Inference time: {inference_time:.2f} ms")

    # === Get and dequantize output ===
    start_time = time.perf_counter()
    output_data = interpreter.get_tensor(output_details[0]['index'])[0]
    assert output_data.dtype == np.uint8
    topk_time = (time.perf_counter() - start_time) * 1000
    print(f"Output read time: {topk_time:.2f} ms")
    #if output_dtype != np.float32:
        #output_data = output_scale * (output_data.astype(np.float32) - output_zero_point)

    # === Top-5 predictions (like ESP32 sorting) ===
    # Top-5 prediction like ESP32
    top5 = sorted(
        [(i, int(score)) for i, score in enumerate(output_data)],
        key=lambda x: x[1], reverse=True
    )[:5]

    print("\nTop 5 predictions (raw uint8 scores):")
    for rank, (cls, score) in enumerate(top5, start=1):
        print(f"{rank}: Class {cls} (Score: {score})")

    log_memory_info()

# === Run main ===
if __name__ == "__main__":
    run_mobilenet_inference()
