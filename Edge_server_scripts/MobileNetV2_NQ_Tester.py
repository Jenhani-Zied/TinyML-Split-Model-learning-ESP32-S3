# test_model.py
import numpy as np
import tensorflow as tf
from PIL import Image
from tensorflow.keras.applications.mobilenet_v2 import decode_predictions, preprocess_input
from tensorflow.keras.applications import MobileNetV2

def test_prediction():
    model =model =  MobileNetV2(weights='imagenet', 
                   input_shape=(224, 224, 3),  # Standard input size
                   include_top=True,alpha=0.35)  # Keep full classification layer
    # Load sample image (replace with your own)
    img = Image.open("bergerall.jpg").resize((224, 224))
    img_array = np.array(img)
    
    # Preprocess for MobileNet
    img_array = preprocess_input(img_array[np.newaxis, ...])  # Add batch dimension
    
    # Make prediction
    preds = model.predict(img_array)
    decoded = decode_predictions(preds, top=5)[0]  # Top 3 predictions
    
    print("Predictions:")
    for i, (imagenet_id, label, prob) in enumerate(decoded):
        print(f"{i+1}: {label} ({prob:.2f})")
                                                                                                           
test_prediction()  # Verify the model works        
