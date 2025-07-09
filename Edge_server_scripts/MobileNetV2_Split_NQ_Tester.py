import tensorflow as tf
from tensorflow.keras.models import load_model
from tensorflow.keras.applications.mobilenet_v2 import preprocess_input
import numpy as np
from PIL import Image

# === Load both split models ===
feature_model = load_model("First_Part_Model.h5")
classifier_model = load_model("Second_Part_Model.h5")

# === Load and preprocess the image ===
def preprocess_image(image_path):
    img = Image.open(image_path).convert("RGB").resize((224, 224))
    img_array = np.array(img).astype(np.float32)
    img_array = preprocess_input(img_array) # Use same preprocessing as full model
    return np.expand_dims(img_array, axis=0)


image_path = "bergerall.jpg"
input_image = preprocess_image(image_path)

# === Run inference ===
intermediate_output = feature_model.predict(input_image)
final_output = classifier_model.predict(intermediate_output)
print("Shape:", intermediate_output.shape)
print(intermediate_output)


# === Print Top 5 Predictions ===
top_k = 5  # Change this to print more predictions
top_indices = final_output[0].argsort()[-top_k:][::-1]  # Indices of top k classes

print(f"✅ Top {top_k} Predicted Classes and Confidence Scores:")
for rank, idx in enumerate(top_indices):
    confidence = final_output[0][idx]
    print(f"{rank+1}. Class {idx}, Confidence: {confidence:.4f}")
