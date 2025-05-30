import paho.mqtt.client as mqtt
import ssl
import serial
import time
import os
import csv # For CSV operations

# --- Configuration Section ---
# MQTT Broker Details (User should configure these)
MQTT_BROKER_ADDRESS = "YOUR_MQTT_BROKER_ADDRESS"  # e.g., "test.mosquitto.org" or your private broker
MQTT_PORT = 8883                                  # Default TLS port for MQTT
MQTT_TOPIC = "your_prefix/esp32_data"             # MQTT topic to publish data to
MQTT_USERNAME = "YOUR_MQTT_USERNAME"              # Your MQTT username (if required)
MQTT_PASSWORD = "YOUR_MQTT_PASSWORD"              # Your MQTT password (if required)
# Path to your CA certificate file for TLS connection
CA_CERT_PATH = "/path/to/your/emqxsl-ca.crt" # e.g., "/home/debian/certs/ca.crt"

# Serial Port (Bluetooth) Details
SERIAL_PORT_DEVICE = "/dev/rfcomm0" # Default for first Bluetooth serial device
BAUD_RATE = 9600                    # Should match ESP32's Bluetooth serial baud rate

# CSV File Details
# Path where the collected sensor data will be saved as a CSV file
CSV_FILE_PATH = "/path/to/your/dataset.csv" # e.g., "/home/debian/data/sensor_data.csv"
# Optional: CSV header if you want to write it when the file is created
# If the ESP32 already sends a header, this might not be needed here.
# CSV_HEADER = ["UserID", "Timestamp", "RawT9", ..., "DurationT0"]


# --- MQTT Callback Functions ---
def on_connect(client, userdata, flags, rc):
    """Callback function executed when the client connects to the MQTT broker."""
    if rc == 0:
        print(f"Successfully connected to MQTT Broker: {MQTT_BROKER_ADDRESS}:{MQTT_PORT}")
    else:
        print(f"Failed to connect to MQTT Broker, return code {rc}")

def on_disconnect(client, userdata, rc):
    """Callback function executed when the client disconnects from the MQTT broker."""
    print(f"Disconnected from MQTT, return code {rc}")
    if rc != 0:
        print("Unexpected disconnection.")

def on_publish(client, userdata, mid):
    """
    Callback function executed when a message is successfully published.
    This can be very verbose if enabled.
    """
    # print(f"Message ID {mid} successfully published.")
    pass


# --- CSV Writing Function ---
def write_to_csv(file_path, data_row_string):
    """
    Appends a given comma-separated string as a new row to a CSV file.
    
    :param file_path: Path to the CSV file.
    :param data_row_string: A string containing data values separated by commas (e.g., "value1,value2,value3").
    """
    try:
        # Split the incoming string by commas to create a list
        data_list = data_row_string.split(',')

        # Check if file exists (could be used to write header if file is new, currently not used)
        # file_exists = os.path.isfile(file_path)

        # Open the file in append mode ('a')
        with open(file_path, 'a', newline='') as csvfile:
            csv_writer = csv.writer(csvfile)
            # If header is desired and file is new/empty:
            # if not file_exists or os.path.getsize(file_path) == 0:
            #     if CSV_HEADER:
            #         csv_writer.writerow(CSV_HEADER)
            csv_writer.writerow(data_list)
        # print(f"Data written to CSV: {data_row_string}") # Can be too frequent for logs
    except Exception as e:
        print(f"Error writing to CSV file ({file_path}): {e}")


# --- Main Application Logic ---
def main():
    """Main function to set up connections and process data."""
    print("--- BeagleBone Black - ESP32 Data Gateway ---")
    print("Initializing...")

    if not os.path.exists(CA_CERT_PATH):
        print(f"ERROR: CA Certificate file not found at: {CA_CERT_PATH}")
        print("Please ensure the CA_CERT_PATH variable is set correctly.")
        return

    # Initialize MQTT Client
    # Using a unique client_id is good practice
    client = mqtt.Client(client_id=f"bbb_gateway_{os.getpid()}", protocol=mqtt.MQTTv311, clean_session=True)
    if MQTT_USERNAME and MQTT_PASSWORD: # Set username and password if provided
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    
    # Configure TLS
    try:
        client.tls_set(ca_certs=CA_CERT_PATH, cert_reqs=ssl.CERT_REQUIRED, tls_version=ssl.PROTOCOL_TLS_CLIENT)
        # Note: If PROTOCOL_TLS_CLIENT causes issues, some older OpenSSL versions might need ssl.PROTOCOL_TLSv1_2
    except Exception as e:
        print(f"Error setting up TLS: {e}. Check your OpenSSL version.")
        print("You might try 'tls_version=ssl.PROTOCOL_TLSv1_2' in client.tls_set(...)")
        return

    # Assign callback functions
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_publish = on_publish

    # Connect to MQTT Broker
    print(f"Attempting to connect to MQTT Broker: {MQTT_BROKER_ADDRESS}:{MQTT_PORT}")
    try:
        client.connect(MQTT_BROKER_ADDRESS, MQTT_PORT, 60) # 60-second keepalive
    except Exception as e:
        print(f"MQTT connection error: {e}")
        return

    client.loop_start() # Start the MQTT client loop in a separate thread

    # Initialize Serial Port (for Bluetooth communication)
    ser = None # Initialize ser to None for the finally block
    try:
        print(f"Attempting to open serial port: {SERIAL_PORT_DEVICE} at {BAUD_RATE} baud")
        ser = serial.Serial(SERIAL_PORT_DEVICE, BAUD_RATE, timeout=1) # 1-second timeout for reads
        print(f"Successfully opened serial port {SERIAL_PORT_DEVICE}.")
    except serial.SerialException as e:
        print(f"Error opening serial port ({SERIAL_PORT_DEVICE}): {e}")
        print("Ensure Bluetooth device is paired, trusted, and bound to rfcomm0 (or configured port).")
        client.loop_stop()
        client.disconnect()
        return

    print(f"Waiting for data from ESP32 on {SERIAL_PORT_DEVICE}...")
    print(f"Publishing data to MQTT topic: {MQTT_TOPIC}")
    print(f"Saving data to CSV file: {CSV_FILE_PATH}")

    try:
        while True:
            if ser.in_waiting > 0: # Check if there's data waiting in the serial buffer
                try:
                    line = ser.readline() # Read a line (terminated by newline)
                    if line:
                        # Decode from bytes to string, ignore errors if any non-UTF-8 chars appear
                        message = line.decode('utf-8', errors='ignore').strip() 
                        if message: # Ensure the message is not empty after stripping
                            print(f"Received: '{message}'")

                            # 1. Write to CSV file
                            write_to_csv(CSV_FILE_PATH, message)

                            # 2. Publish to MQTT
                            # QoS 1: At least once delivery
                            result = client.publish(MQTT_TOPIC, message, qos=1) 
                            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                                # Print only a snippet if the message is long
                                print(f"Successfully published '{message[:30]}...' to MQTT.")
                            else:
                                print(f"Failed to publish message to MQTT, error code: {result.rc}")
                        # else:
                        #     print("Received empty line from ESP32.")
                except UnicodeDecodeError:
                    print("Unicode decode error while reading from serial. Ensure ESP32 sends UTF-8.")
                except Exception as read_err:
                    print(f"Error reading from or processing serial data: {read_err}")

            # Brief pause to reduce CPU usage and allow other processes to run
            time.sleep(0.05) 

    except KeyboardInterrupt:
        print("\nProgram terminated by user (Ctrl+C).")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
    finally:
        print("Cleaning up resources...")
        if ser and ser.is_open:
            ser.close()
            print(f"Serial port {SERIAL_PORT_DEVICE} closed.")
        
        if client.is_connected(): # Check if client is connected before attempting to stop loop/disconnect
            client.loop_stop()
            client.disconnect()
        print("MQTT client disconnected.")
        print("Exiting.")

if __name__ == '__main__':
    main()