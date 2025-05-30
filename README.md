# touchswipe-identifier

An IoT system that identifies users based on capacitive swipe patterns using ESP32, BeagleBone Black, and a machine learning model.

## 📌 Project Overview

This project consists of three main components:

1. **ESP32** – Collects capacitive touch data from 9 copper tape strips using touch pins.
2. **BeagleBone Black (BBB)** – Acts as a middle layer for processing and running the ML model.
3. **MQTT Server** – Used for sending collected data to a remote server (instructor-owned) during the data collection phase.

## 🔧 How It Works

- Each of the 9 copper strips connected to the ESP32 measures:
  - **Capacitive value**
  - **Time spent on each strip**
- These 18 features (9 touch values + 9 durations) are sent to the BeagleBone Black.
- Initially, the BBB forwards this data to the MQTT server for storage and analysis.
- Once enough data is collected, a machine learning model is trained to recognize individual swipe patterns.
- The trained model is then deployed on the BBB.
- In the final stage, MQTT is no longer used — the BBB directly receives real-time data from the ESP32, feeds it into the model, and predicts the identity of the person based on swipe behavior.

## 🧠 Machine Learning

- Model Input: 18 features (9 touch values + 9 durations)
- Goal: Identify the user based on the unique sequence and speed of their swipe from left to right.

## 🚀 Tech Stack

- **ESP32** (Microcontroller for capacitive sensing)
- **BeagleBone Black** (Local processing and model inference)
- **MQTT** (Message queue for data communication)
- **Python** (Data processing and model training)
- **Scikit-learn / Other ML libraries** (Model implementation – TBD)

## 📁 Folder Structure

