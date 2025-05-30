#include "BluetoothSerial.h" // Bluetooth library

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it.
#endif

BluetoothSerial SerialBT; // Bluetooth Serial object

// Pin Definitions
#define TOUCH_PIN_0 T0  // GPIO 4
#define TOUCH_PIN_2 T2  // GPIO 2
#define TOUCH_PIN_3 T3  // GPIO 15
#define TOUCH_PIN_4 T4  // GPIO 13
#define TOUCH_PIN_5 T5  // GPIO 12
#define TOUCH_PIN_6 T6  // GPIO 14
#define TOUCH_PIN_7 T7  // GPIO 27
#define TOUCH_PIN_8 T8  // GPIO 33
#define TOUCH_PIN_9 T9  // GPIO 32

#define RESET_BUTTON_PIN 23     // Button to reset trial for the current user
#define USER_BUTTON_PIN  22     // Button to switch to the next user
#define DEBOUNCE_DELAY   50     // Debounce delay for buttons in milliseconds

#define LED_RED_PIN   18        // Red component of RGB LED
#define LED_GREEN_PIN 19        // Green component of RGB LED
#define LED_BLUE_PIN  21        // Blue component of RGB LED

// Thresholds and Timings
#define TOUCH_THRESHOLD 30        // Value below which a touch is registered
#define IDLE_THRESHOLD 30         // Threshold to consider a sensor idle for printing logic
#define CSV_OUTPUT_INTERVAL 100   // Interval for sending CSV data in milliseconds
#define MAX_ACTIVE_READINGS 10    // Max number of readings to record for a sensor once touched
#define INACTIVE_SENSOR_CSV_VALUE -1 // Value to send for sensors that have reached their reading limit
#define GREEN_BLINK_DURATION 50   // Duration for green LED blink on touch
#define YELLOW_FLASH_DURATION 200 // Duration for yellow LED flash on trial reset
#define BLUE_FLASH_DURATION 200   // Duration for blue LED flash on user change
#define MAGENTA_FLASH_DURATION 250 // Duration for magenta LED flash when a sensor reaches its limit

// Global Variables
unsigned long lastCsvOutputTime = 0; // Timestamp of the last CSV output
int currentUserID = 0;               // Current user ID
int currentTrialID = 0;              // Current trial ID for the user

bool allSensorsReadLimitReachedGlobal = false; // Flag if all sensors have completed their readings for a trial
// LED Flash State Variables
bool greenBlinkActive = false; unsigned long greenBlinkStartTime = 0;
bool yellowFlashActive = false; unsigned long yellowFlashStartTime = 0;
bool blueFlashActive = false; unsigned long blueFlashStartTime = 0;
bool magentaFlashActive = false; unsigned long magentaFlashStartTime = 0;

// Structure to hold data for each touch sensor
struct TouchSensorData {
  const int pinMacro;                 // ESP32 touch pin (e.g., T0, T2)
  const char* name;                   // Name of the sensor (e.g., "T9")
  int rawValue;                       // Last read raw touch value
  bool isLogicallyActiveForDurationCalc; // If sensor is currently active for duration calculation
  unsigned long currentOngoingDuration; // Current duration of the ongoing touch
  unsigned long touchStartTime;         // Timestamp when the current touch started
  bool wasPhysicallyTouchedInPreviousCycle; // If the sensor was touched in the previous loop cycle
  int activeCsvPrintCount;            // Number of times this sensor's active data has been printed
  bool limitReachedThisSensor;        // If this sensor has reached its MAX_ACTIVE_READINGS
};

// Array of sensor data structures
TouchSensorData sensors[] = {
  {TOUCH_PIN_9, "T9", 0, false, 0, 0, false, 0, false}, {TOUCH_PIN_8, "T8", 0, false, 0, 0, false, 0, false},
  {TOUCH_PIN_7, "T7", 0, false, 0, 0, false, 0, false}, {TOUCH_PIN_0, "T0", 0, false, 0, 0, false, 0, false},
  {TOUCH_PIN_2, "T2", 0, false, 0, 0, false, 0, false}, {TOUCH_PIN_3, "T3", 0, false, 0, 0, false, 0, false},
  {TOUCH_PIN_6, "T6", 0, false, 0, 0, false, 0, false}, {TOUCH_PIN_5, "T5", 0, false, 0, 0, false, 0, false},
  {TOUCH_PIN_4, "T4", 0, false, 0, 0, false, 0, false}
};
const int numSensors = sizeof(sensors) / sizeof(TouchSensorData);

// Button state variables for debouncing
int resetButtonState; int lastResetButtonState = HIGH; unsigned long lastResetDebounceTime = 0;
int userButtonState;  int lastUserButtonState = HIGH;  unsigned long lastUserDebounceTime = 0;

// Function to set RGB LED color
void setLedColor(int redState, int greenState, int blueState) {
  digitalWrite(LED_RED_PIN, redState ? HIGH : LOW);
  digitalWrite(LED_GREEN_PIN, greenState ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN, blueState ? HIGH : LOW);
}

// Function to handle trial reset button press
void triggerTrialReset() {
  currentTrialID++; 
  Serial.print("TRIAL RESET for User"); Serial.print(currentUserID); // USB debug message
  Serial.print("."); Serial.print(currentTrialID);
  Serial.println(". YELLOW FLASH...");
  // SerialBT.println("TRIAL_RESET"); // Optionally, send trial reset info to BBB via Bluetooth
  
  // Reset sensor states
  for (int i = 0; i < numSensors; i++) {
    sensors[i].touchStartTime = 0; sensors[i].currentOngoingDuration = 0;
    sensors[i].isLogicallyActiveForDurationCalc = false; sensors[i].wasPhysicallyTouchedInPreviousCycle = false;
    sensors[i].activeCsvPrintCount = 0; sensors[i].limitReachedThisSensor = false;
  }
  allSensorsReadLimitReachedGlobal = false; 
  // Reset LED flags and trigger yellow flash
  greenBlinkActive = false; blueFlashActive = false; magentaFlashActive = false;
  yellowFlashActive = true; yellowFlashStartTime = millis(); setLedColor(1, 1, 0); // Yellow
}

// Function to handle user change button press
void triggerUserChange() {
  currentUserID++; currentTrialID = 0; 
  Serial.print("USER CHANGED TO: User"); Serial.print(currentUserID); // USB debug message
  Serial.print("."); Serial.print(currentTrialID); 
  Serial.println(". BLUE FLASH & SENSOR RESET...");
  // SerialBT.print("USER_CHANGED:"); SerialBT.println(currentUserID); // Optionally, send user change info to BBB

  // Reset sensor states
  for (int i = 0; i < numSensors; i++) {
    sensors[i].touchStartTime = 0; sensors[i].currentOngoingDuration = 0;
    sensors[i].isLogicallyActiveForDurationCalc = false; sensors[i].wasPhysicallyTouchedInPreviousCycle = false;
    sensors[i].activeCsvPrintCount = 0; sensors[i].limitReachedThisSensor = false;
  }
  allSensorsReadLimitReachedGlobal = false;
  // Reset LED flags and trigger blue flash
  yellowFlashActive = false; greenBlinkActive = false; magentaFlashActive = false;
  blueFlashActive = true; blueFlashStartTime = millis(); setLedColor(0, 0, 1); // Blue
}


void setup() {
  Serial.begin(115200); // Initialize USB Serial Monitor (useful for debugging)
  delay(1000); // Wait for serial to initialize
  Serial.println("ESP32 Bluetooth Data Sender Initializing...");

  SerialBT.begin("ESP32_TouchData"); // Initialize Bluetooth with a device name
  Serial.println("Bluetooth initialized. Visible as 'ESP32_TouchData'.");
  Serial.println("Connect to this device from BeagleBone Black via Bluetooth.");

  // Initialize button pins
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP); resetButtonState = digitalRead(RESET_BUTTON_PIN);
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);  userButtonState = digitalRead(USER_BUTTON_PIN);
  // Initialize LED pins
  pinMode(LED_RED_PIN, OUTPUT); pinMode(LED_GREEN_PIN, OUTPUT); pinMode(LED_BLUE_PIN, OUTPUT);
  setLedColor(0, 0, 0); // Turn LED off initially

  // Prepare and send CSV Header via Bluetooth (BBB script can use this as the first line)
  // NOTE: Printing to USB Serial monitor (Serial.print) can remain for debugging.
  String header = "user,timestamp,";
  for (int i = 0; i < numSensors; i++) { header += "raw"; header += sensors[i].name; header += ","; }
  for (int i = 0; i < numSensors; i++) { 
    header += "ongoingDur"; header += sensors[i].name;
    if (i < numSensors - 1) header += ",";
  }
  SerialBT.println(header); // Send the header via Bluetooth
  Serial.println("CSV Header sent to Bluetooth:"); // USB debug message
  Serial.println(header);                        // USB debug message
}

void loop() {
  unsigned long currentTime = millis(); // Get current time

  // --- Button Controls ---
  // Read reset button with debouncing
  int currentResetReading = digitalRead(RESET_BUTTON_PIN);
  if (currentResetReading != lastResetButtonState) { lastResetDebounceTime = currentTime; }
  if ((currentTime - lastResetDebounceTime) > DEBOUNCE_DELAY) {
    if (currentResetReading != resetButtonState) {
      resetButtonState = currentResetReading;
      if (resetButtonState == LOW) { // Button pressed (LOW due to INPUT_PULLUP)
        triggerTrialReset(); 
      } 
    }
  }
  lastResetButtonState = currentResetReading;

  // Read user button with debouncing
  int currentUserReading = digitalRead(USER_BUTTON_PIN);
  if (currentUserReading != lastUserButtonState) { lastUserDebounceTime = currentTime; }
  if ((currentTime - lastUserDebounceTime) > DEBOUNCE_DELAY) {
    if (currentUserReading != userButtonState) {
      userButtonState = currentUserReading;
      if (userButtonState == LOW) { // Button pressed
        triggerUserChange(); 
      }
    }
  }
  lastUserButtonState = currentUserReading;

  // --- Prepare Sensor Data ---
  bool anySensorJustReachedLimit = false; // Flag if any sensor reached its limit in this cycle
  for (int i = 0; i < numSensors; i++) {
    sensors[i].rawValue = touchRead(sensors[i].pinMacro); // Read raw touch value
    bool isCurrentlyPhysicallyTouched = (sensors[i].rawValue <= TOUCH_THRESHOLD);

    if (!sensors[i].limitReachedThisSensor) { // If this sensor hasn't finished its readings
        if (sensors[i].activeCsvPrintCount < MAX_ACTIVE_READINGS) { // And still has readings to take
            if (isCurrentlyPhysicallyTouched) {
                sensors[i].isLogicallyActiveForDurationCalc = true; // Mark as active for duration
                if (!sensors[i].wasPhysicallyTouchedInPreviousCycle) { // If it's a new touch
                    sensors[i].touchStartTime = currentTime; 
                    sensors[i].currentOngoingDuration = 0;
                } else { // If touch continues
                    sensors[i].currentOngoingDuration = currentTime - sensors[i].touchStartTime; 
                }
            } else { // If not touched
                sensors[i].isLogicallyActiveForDurationCalc = false; 
                sensors[i].currentOngoingDuration = 0; 
                sensors[i].touchStartTime = 0;
            }
        } else { // If MAX_ACTIVE_READINGS reached for this sensor
            sensors[i].isLogicallyActiveForDurationCalc = false; 
            sensors[i].currentOngoingDuration = 0;
            if (!sensors[i].limitReachedThisSensor) { // Mark as limit reached if not already
                 sensors[i].limitReachedThisSensor = true; 
                 anySensorJustReachedLimit = true; 
            }
        }
    } else { // If sensor limit was already reached
        sensors[i].isLogicallyActiveForDurationCalc = false; 
        sensors[i].currentOngoingDuration = 0;
    }
    sensors[i].wasPhysicallyTouchedInPreviousCycle = isCurrentlyPhysicallyTouched; // Store current touch state for next cycle
  }
  
  // --- Magenta Flash Trigger ---
  // If any sensor just reached its limit and not all sensors are done globally
  if (anySensorJustReachedLimit && !allSensorsReadLimitReachedGlobal) {
      greenBlinkActive = false; yellowFlashActive = false; blueFlashActive = false;   
      magentaFlashActive = true; magentaFlashStartTime = currentTime; setLedColor(1, 0, 1); // Magenta
  }

  // --- Global Finish Check ---
  // Check if all sensors have completed their readings for the current trial
  if (!allSensorsReadLimitReachedGlobal) {
    bool allDoneCheck = true;
    for (int i = 0; i < numSensors; i++) { 
      if (!sensors[i].limitReachedThisSensor) { 
        allDoneCheck = false; break; 
      } 
    }
    if (allDoneCheck) { // If all sensors are done
      allSensorsReadLimitReachedGlobal = true;
      // Turn off any temporary flashes
      greenBlinkActive = false; yellowFlashActive = false; blueFlashActive = false; magentaFlashActive = false;
      // Solid red might indicate end of trial sequence, handled below
    }
  }

  // --- LED Status Management ---
  // Handle timed flashes
  if (blueFlashActive) {
    if (currentTime - blueFlashStartTime >= BLUE_FLASH_DURATION) { blueFlashActive = false; } 
    else { setLedColor(0, 0, 1); } // Blue
  } else if (yellowFlashActive) {
    if (currentTime - yellowFlashStartTime >= YELLOW_FLASH_DURATION) { yellowFlashActive = false; } 
    else { setLedColor(1, 1, 0); } // Yellow
  } else if (magentaFlashActive) {
    if (currentTime - magentaFlashStartTime >= MAGENTA_FLASH_DURATION) { magentaFlashActive = false; } 
    else { setLedColor(1, 0, 1); } // Magenta
  }
  // Set default/persistent LED states if no flash is active
  if (!blueFlashActive && !yellowFlashActive && !magentaFlashActive) {
    if (allSensorsReadLimitReachedGlobal) { 
      setLedColor(1, 0, 0); // Solid Red: All sensors done for this trial
    } 
    else if (greenBlinkActive) { // Green blink for active touch reading
      if (currentTime - greenBlinkStartTime >= GREEN_BLINK_DURATION) { 
        greenBlinkActive = false; setLedColor(0, 0, 0); // Turn off after blink
      }
    } else { 
      setLedColor(0, 0, 0); // Off: Idle or between blinks
    }
  }

  // --- Periodic CSV Output ---
  if (currentTime - lastCsvOutputTime >= CSV_OUTPUT_INTERVAL) {
    lastCsvOutputTime = currentTime; // Update last output time
    
    // Determine if any sensor is actively being touched (and not yet done with its readings)
    // This is to avoid sending CSV data when the system is truly idle.
    bool allSensorsTrulyIdleOrDoneForPrinting = true;
    for (int i = 0; i < numSensors; i++) {
      // If a sensor has not reached its limit AND its raw value indicates it's not idle (being touched or close)
      if (!sensors[i].limitReachedThisSensor && sensors[i].rawValue <= IDLE_THRESHOLD) {
        allSensorsTrulyIdleOrDoneForPrinting = false; 
        break; 
      }
    }

    if (!allSensorsTrulyIdleOrDoneForPrinting) { 
      // Construct CSV line
      // Creating a string and sending it at once might be more efficient,
      // but sending in parts also works.
      String csvLine = "User" + String(currentUserID) + "." + String(currentTrialID) + ",";
      csvLine += String(currentTime) + ",";

      bool activeReadingThisCycle = false; // Flag if any sensor recorded an active touch this cycle
      // Add raw sensor values
      for (int i = 0; i < numSensors; i++) { 
        if (!sensors[i].limitReachedThisSensor) { csvLine += String(sensors[i].rawValue); } 
        else { csvLine += String(INACTIVE_SENSOR_CSV_VALUE); } // Send inactive value if limit reached
        csvLine += ","; 
      }
      // Add ongoing duration values
      for (int i = 0; i < numSensors; i++) { 
        unsigned long durationToPrint = 0;
        if (sensors[i].isLogicallyActiveForDurationCalc) {
            durationToPrint = sensors[i].currentOngoingDuration;
            if(sensors[i].rawValue <= TOUCH_THRESHOLD) { // If actually being touched
               sensors[i].activeCsvPrintCount++; // Increment print count for this sensor
               activeReadingThisCycle = true;    // Mark that an active reading occurred
            }
        }
        csvLine += String(durationToPrint);
        if (i < numSensors - 1) csvLine += ",";
      }
      
      SerialBT.println(csvLine); // Send the CSV line via Bluetooth
      Serial.println("Sent to BT: " + csvLine); // Also print to USB for debugging

      // If an active reading occurred, and no other major LED event is active, blink green
      if (activeReadingThisCycle && !allSensorsReadLimitReachedGlobal && 
          !yellowFlashActive && !blueFlashActive && !magentaFlashActive) {
        setLedColor(0, 1, 0); // Green
        greenBlinkActive = true; greenBlinkStartTime = currentTime;
      }
    } 
  } 
}