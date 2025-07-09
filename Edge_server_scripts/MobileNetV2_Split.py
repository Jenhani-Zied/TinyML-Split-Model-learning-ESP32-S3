import tensorflow as tf
import numpy as np
from tensorflow.keras.applications import MobileNetV2
from tensorflow.keras.applications.mobilenet_v2 import preprocess_input
from PIL import Image
import os

def split_and_quantize():
    # Load original model
    full_model = MobileNetV2(weights='imagenet',
                           input_shape=(224, 224, 3),
                           include_top=True,
                           alpha=0.35)
    
    # Split at block_15_project
    split_layer_name = 'block_16_project_BN'
    split_layer = full_model.get_layer(split_layer_name)
    print("Splitting after layer:", split_layer_name)
    print("Feature extractor output shape:", split_layer.output.shape)
    
    # Part 1: Feature Extractor
    feature_extractor = tf.keras.Model(
        inputs=full_model.input,
        outputs=split_layer.output,
        name='feature_extractor'
    )
    
    # Part 2: Classifier - Fixed version
    classifier_input = tf.keras.Input(shape=split_layer.output.shape[1:])
    x = classifier_input
    
    # Dictionary to store outputs of specific layers
    layer_outputs = {split_layer_name: x}
    
    # Rebuild the model from split point
    for layer in full_model.layers[full_model.layers.index(split_layer)+1:]:
        if isinstance(layer, tf.keras.layers.Add):
            # Create a Lambda layer to handle zero tensor creation
            class AddWithFallback(tf.keras.layers.Layer):
                def call(self, inputs):
                    # Ensure we have exactly 2 inputs
                    if len(inputs) < 2:
                        shape = tf.shape(inputs[0])
                        zero_input = tf.zeros(shape, dtype=inputs[0].dtype)
                        return tf.keras.layers.Add()([inputs[0], zero_input])
                    return tf.keras.layers.Add()(inputs)
            
            # Get input layer names
            input_names = []
            for node in layer._inbound_nodes:
                for inbound_layer in getattr(node, 'inbound_layers', []):
                    input_names.append(inbound_layer.name)
            
            # Get the actual tensors to add
            inputs = []
            for name in input_names:
                if name in layer_outputs:
                    inputs.append(layer_outputs[name])
            
            # Use our custom add layer
            x = AddWithFallback()(inputs) if inputs else x
        else:
            x = layer(x)
        
        layer_outputs[layer.name] = x
    
    classifier = tf.keras.Model(
        inputs=classifier_input,
        outputs=x,
        name='classifier'
    )

    # Save float models
    feature_extractor.save("First_Part_Model.h5")
    classifier.save("Second_Part_Model.h5")
    def preprocess_image(image_path):
        img = Image.open(image_path).resize((224, 224)).convert("RGB")
        img_array = np.array(img, dtype=np.float32)
        return preprocess_input(img_array)  # MobileNetV2 standard [-1,1]
    # Representative dataset using real images
    def representative_dataset():
        # Replace with your actual image directory
        image_dir = "photos"
        image_paths = [os.path.join(image_dir, f) for f in os.listdir(image_dir)][:100]
            
        for image_path in image_paths:
                yield [preprocess_image(image_path)[np.newaxis, ...]]
    # NEW: Function to get quantization stats
    def get_quantization_stats(converter):
        interpreter = tf.lite.Interpreter(model_content=converter.convert())
        interpreter.allocate_tensors()
        output_details = interpreter.get_output_details()[0]
        return {
            'scale': output_details['quantization_parameters']['scales'][0],
            'zero_point': output_details['quantization_parameters']['zero_points'][0]
        }

    # Quantize feature extractor
    converter = tf.lite.TFLiteConverter.from_keras_model(feature_extractor)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.uint8
    converter.inference_output_type = tf.uint8
    
    # Get quantization stats
    feature_stats = get_quantization_stats(converter)
    tflite_feature_extractor = converter.convert()
    
    # Quantize classifier with matched parameters
    classifier_converter = tf.lite.TFLiteConverter.from_keras_model(classifier)
    classifier_converter.optimizations = [tf.lite.Optimize.DEFAULT]
    
    def classifier_representative_dataset():
        # Create fake intermediate outputs with correct range
        image_dir = "/home/zied/Documents/work folder/Ing/mobilenet_test/Real_test_mobileNet/photos"
        image_paths = [os.path.join(image_dir, f) for f in os.listdir(image_dir)][:100]
        
        for image_path in image_paths:
            # Get REAL intermediate features
            img = preprocess_image(image_path)[np.newaxis, ...]
            features = feature_extractor.predict(img)
            yield [features]
    
    classifier_converter.representative_dataset = classifier_representative_dataset
    classifier_converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    classifier_converter.inference_input_type = tf.uint8
    classifier_converter.inference_output_type = tf.uint8
    
    tflite_classifier = classifier_converter.convert()
    
    # Save quantized models
    with open('First_Part_Model_quant.tflite', 'wb') as f:
        f.write(tflite_feature_extractor)
    with open('Second_Part_Model_quant.tflite', 'wb') as f:
        f.write(tflite_classifier)
    
    print("Quantization parameters:")
    print(f"Feature extractor output: scale={feature_stats['scale']:.5f}, zero_point={feature_stats['zero_point']}")
    
    # Verify models
    verify_quantization('First_Part_Model_quant.tflite', 'Second_Part_Model_quant.tflite')

def verify_quantization(feature_path, classifier_path):
    """Check quantization alignment between models"""
    feature_interpreter = tf.lite.Interpreter(model_path=feature_path)
    classifier_interpreter = tf.lite.Interpreter(model_path=classifier_path)
    
    feature_interpreter.allocate_tensors()
    classifier_interpreter.allocate_tensors()
    
    # Get quantization details
    feature_out_quant = feature_interpreter.get_output_details()[0]['quantization_parameters']
    classifier_in_quant = classifier_interpreter.get_input_details()[0]['quantization_parameters']
    
    print(f"Feature extractor output: scale={feature_out_quant['scales'][0]:.5f}, zero={feature_out_quant['zero_points'][0]}")
    print(f"Classifier input: scale={classifier_in_quant['scales'][0]:.5f}, zero={classifier_in_quant['zero_points'][0]}")
    
    # Check alignment
    scale_diff = abs(feature_out_quant['scales'][0] - classifier_in_quant['scales'][0]) / feature_out_quant['scales'][0]
    zero_diff = abs(feature_out_quant['zero_points'][0] - classifier_in_quant['zero_points'][0])
    
    if scale_diff > 0.01 or zero_diff > 1:
        print("⚠️ Warning: Quantization mismatch! Try more representative data.")
    else:
        print("✅ Quantization parameters aligned properly")

# Run the conversion
split_and_quantize()