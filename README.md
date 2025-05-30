# 👋 touchswipe-identifier: AIoT Capacitive Swipe User Identification

**Project Team:**
*   [@cihanayindi](https://github.com/cihanayindi)
*   [@keremtanil](https://github.com/keremtanil)
*   [@turan1609](https://github.com/turan1609)
*   [@rizakarakaya](https://github.com/rizakarakaya)

An AIoT (Artificial Intelligence of Things) system designed to identify users based on their unique capacitive swipe patterns. This project leverages an ESP32 microcontroller for data acquisition, a BeagleBone Black for edge processing and machine learning inference, and an MQTT server for initial data aggregation, fulfilling the requirements of our "AIoT Project: Machine Learning and Sensor Networks" course.

## 📜 Project Concept

The core idea is to capture the distinct way individuals interact with a series of capacitive touch sensors. By analyzing both the electrical capacitance change and the duration of touch on each sensor strip during a swipe, we aim to create a unique "fingerprint" for each user. This data is then used to train a machine learning model for user identification.

![System Schema](images/schema.png) *(Assuming you have an image named schema.png in an 'images' folder)*

**Key Goals:**
1.  Collect real-time capacitive touch and duration data using ESP32.
2.  Transmit this data to a BeagleBone Black (BBB) via Bluetooth.
3.  Initially, forward data from BBB to a central MQTT server for backup and instructor oversight.
4.  Train a supervised machine learning model on the collected dataset.
5.  Deploy the trained model onto the BeagleBone Black for real-time, on-device inference.

## ✨ System Highlights

*   **Capacitive Sensing:** 9 copper tape strips connected to ESP32 touch pins.
*   **Dual-Feature Data:** Captures raw capacitive values and touch duration for each strip.
*   **Wireless Communication:** ESP32 to BeagleBone Black via Bluetooth.
*   **Edge ML Deployment:** BeagleBone Black hosts the ML model for local inference.
*   **Phased Data Flow:**
    *   **Collection:** ESP32 → Bluetooth → BBB → MQTT Server & Local CSV Backup.
    *   **Inference:** ESP32 → Bluetooth → BBB (ML Model) → User Prediction.
*   **Interactive Data Labeling:** Physical buttons on ESP32 for user/trial management.
*   **Visual Feedback:** RGB LED for system status and user interaction.

## ⚙️ System Architecture Simplified

1.  **Sensing (ESP32):** Reads 9 copper tapes, calculates touch duration, formats data as CSV, and sends via Bluetooth. Includes buttons and LED for interaction.
2.  **Processing (BeagleBone Black):** Receives Bluetooth data. During collection, it relays to MQTT and saves locally. Post-deployment, it runs the ML model for prediction.
3.  **Data Backup (MQTT - Initial Phase):** Instructor-managed server for secure data storage during collection.

## 🚀 Our Learning Journey & Challenges

This project was a significant learning experience, pushing us to integrate diverse hardware and software components.

**Key Milestones & Technical Hurdles:**

1.  **Hardware Prototyping & Assembly:**
    *   **ESP32 Capacitive Touch:** Started with a single copper tape to validate ESP32's touchRead functionality, then successfully scaled to 9 tapes. This involved careful wiring and soldering to ensure reliable connections.
    *   **Physical Interface:** Designed and built a user-friendly interface on a breadboard with buttons for data labeling (user/trial) and an RGB LED for intuitive feedback, which required iterating on the ESP32 code for responsiveness.

2.  **BeagleBone Black Integration:**
    *   **Initial Connectivity Issues:** A major early challenge was getting the BeagleBone Black connected to the internet. We overcame this by **installing a fresh Debian 12 OS**, which provided better driver support and network configuration tools. This was a valuable lesson in troubleshooting embedded Linux systems.
    *   **Bluetooth Communication:** Establishing a stable Bluetooth serial connection between the ESP32 and BBB (`rfcomm`) required understanding Linux Bluetooth utilities (`bluetoothctl`, `rfcomm bind`) and Python's `pyserial` library. Debugging this link involved checking MAC addresses, RFCOMM channels, and ensuring services were running correctly on the BBB.

3.  **Data Pipeline & MQTT:**
    *   **ESP32 to BBB Data Stream:** We developed robust ESP32 code to consistently format sensor readings and timing data into CSV strings for Bluetooth transmission.
    *   **BBB Data Handling:** The Python script on the BBB was engineered to:
        *   Reliably parse incoming Bluetooth data.
        *   Implement TLS-secured communication with the instructor's **MQTT broker using `paho-mqtt`**, a new technology for many of us. This included managing CA certificates and credentials.
        *   Simultaneously **log data to a local CSV file** on the BBB, providing an essential local backup and a direct source for model training.

4.  **New Technologies & Methods Encountered:**
    *   **Embedded Linux (Debian on BBB):** Gained experience with system configuration, package management, and running Python scripts in a headless environment.
    *   **Bluetooth Serial Profile (SPP):** Learned the intricacies of setting up and using SPP for wireless data transfer between microcontrollers and single-board computers.
    *   **MQTT Protocol:** Understood the publish-subscribe model and its application in IoT for messaging, including security aspects like TLS.
    *   **Edge Computing Concepts:** Applied the principle of processing data closer to the source by planning to deploy the ML model directly on the BBB.
    *   **Systematic Data Collection:** Developed a process for labeled data acquisition using the physical button interface, crucial for supervised machine learning.

**Current Stage & Next Steps:**
*   Actively collecting and labeling swipe data.
*   **Upcoming:** Data preprocessing, ML model training and evaluation, deployment on BBB, and end-to-end system testing.

## 🧠 Machine Learning Aspect

*   **Dataset:** Time-series data from 9 capacitive sensors (18 features: 9 raw values + 9 durations), labeled with User ID.
*   **Task:** Supervised multi-class classification to predict User ID.
*   **Why ML?** Swipe patterns are nuanced; simple thresholds are insufficient. ML can learn these complex, individual-specific patterns.
*   **Potential Models:** Exploring Logistic Regression, Decision Trees, Random Forest, KNN, SVM.

## 🛠️ Tech Stack

*   **Microcontroller:** ESP32 (C++/Arduino, `BluetoothSerial.h`)
*   **Edge Computer:** BeagleBone Black (Debian 12, Python 3, `paho-mqtt`, `pyserial`)
*   **Sensors:** 9x Copper Tape Strips
*   **Communication:** Bluetooth (ESP32-BBB), MQTT over TLS (BBB-Server)
*   **ML (Planned):** Python, Scikit-learn, Pandas, NumPy

## 🔧 Setup & Usage Summary

**1. ESP32:**
   *   Wire sensors, buttons, LED. Upload firmware. Note Bluetooth name.

**2. BeagleBone Black:**
   *   Install Debian, Python, `paho-mqtt`, `pyserial`.
   *   Pair/bind ESP32 Bluetooth to `/dev/rfcomm0`.
   *   Configure and place CA certificate.

**3. Running the System (Data Collection):**
   *   Power ESP32.
   *   On BBB, run the Python data logging script.
   *   Use ESP32 buttons to label user/trial, then swipe. Data flows to BBB (local CSV & MQTT).

## 🔮 Future Enhancements

*   Explore more advanced ML models (e.g., LSTMs for sequence data).
*   Develop a simple web dashboard on BBB for live predictions.
*   Optimize the ML model for edge performance (quantization/pruning).

## 🤝 Contributing

For this course project, collaboration is within the team. For general inquiries or suggestions, feel free to open an issue.

## 📝 License

Educational Use Only.
