import tensorflow as tf
import numpy as np
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.applications.mobilenet_v2 import preprocess_input
from PIL import Image
import os

def create_model():
    """Create MobileNetV2 model and save layer info to a file."""
    model = MobileNetV2(
        weights='imagenet',
        input_shape=(224, 224, 3),
        include_top=True,
        alpha=0.35
    )

    # File path to save the output
    return model


def representative_dataset():
    """Improved representative dataset using real images"""
    # Replace with your actual image directory
    image_dir = "photos"  # Directory with 100+ diverse images
    image_paths = [os.path.join(image_dir, f) for f in os.listdir(image_dir)][:100]
    
    for image_path in image_paths:
        try:
            img = Image.open(image_path).resize((224, 224)).convert("RGB")
            img_array = np.array(img, dtype=np.float32)
            
            # Use the same preprocessing as during training
            img_array = preprocess_input(img_array)  # Normalizes to [-1, 1]
            
            yield [img_array[np.newaxis, ...]]  # Add batch dimension
        except Exception as e:
            print(f"Skipping {image_path}: {str(e)}")
            continue

def convert_to_tflite():
    """Convert full model to quantized TFLite with proper settings"""
    model = create_model()
    
    # Convert to TFLite with quantization
    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.uint8
    converter.inference_output_type = tf.uint8
    
    # Add experimental options for better quantization
    converter.experimental_new_quantizer = True
    converter.experimental_enable_resource_variables = True
    
    try:
        tflite_model = converter.convert()
        
        # Save the model
        output_path = 'mobilenet_esp32_s3.tflite'
        with open(output_path, 'wb') as f:
            f.write(tflite_model)
        
        print(f"Quantized TFLite model saved to {output_path}")
        
        # Verify the quantization
        verify_quantization(output_path)
        
    except Exception as e:
        print(f"Conversion failed: {str(e)}")
        # Fallback to dynamic range quantization if full integer fails
        print("Attempting dynamic range quantization...")
        converter.inference_input_type = tf.float32
        converter.inference_output_type = tf.float32
        tflite_model = converter.convert()
        with open('mobilenetV2_dynamic_quant.tflite', 'wb') as f:
            f.write(tflite_model)
        print("Dynamic range quantized model saved")

def verify_quantization(model_path):
    """Verify the quantization parameters of the converted model"""
    interpreter = tf.lite.Interpreter(model_path=model_path)
    interpreter.allocate_tensors()
    
    input_details = interpreter.get_input_details()[0]
    output_details = interpreter.get_output_details()[0]
    
    print("\nQuantization Verification:")
    print(f"Input details: dtype={input_details['dtype']}, "
          f"scale={input_details['quantization_parameters']['scales'][0]:.5f}, "
          f"zero_point={input_details['quantization_parameters']['zero_points'][0]}")
    
    print(f"Output details: dtype={output_details['dtype']}, "
          f"scale={output_details['quantization_parameters']['scales'][0]:.5f}, "
          f"zero_point={output_details['quantization_parameters']['zero_points'][0]}")
    
    # Check if quantization is applied properly
    if (input_details['dtype'] == np.uint8 and 
        output_details['dtype'] == np.uint8):
        print("✅ Full integer quantization successful")
    else:
        print("⚠️ Quantization may not be fully applied")

if __name__ == "__main__":
    convert_to_tflite()