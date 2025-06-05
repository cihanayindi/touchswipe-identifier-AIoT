#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it.
#endif

BluetoothSerial SerialBT;

// Pin Tanımları
#define TOUCH_PIN_0 T0
#define TOUCH_PIN_2 T2
#define TOUCH_PIN_3 T3
#define TOUCH_PIN_4 T4
#define TOUCH_PIN_5 T5
#define TOUCH_PIN_6 T6
#define TOUCH_PIN_7 T7
#define TOUCH_PIN_8 T8
#define TOUCH_PIN_9 T9

#define START_SCAN_BUTTON_PIN 23
#define END_SCAN_BUTTON_PIN   22
#define DEBOUNCE_DELAY   50

#define LED_RED_PIN   18
#define LED_GREEN_PIN 19
#define LED_BLUE_PIN  21

#define TOUCH_THRESHOLD 30

// Sensör verilerini tutacak yapı (her sensör için tek bir anlık değer)
struct SensorReading {
  int rawValue;
  unsigned long currentDuration;
  unsigned long touchStartTime; // Her sensör için ayrı dokunma başlangıcı
  bool isCurrentlyTouched;    // Her sensör için ayrı dokunma durumu
};

SensorReading currentSensorValues[9]; // T9, T8, T7, T0, T2, T3, T6, T5, T4 sırasıyla
const int touchPins[9] = {TOUCH_PIN_9, TOUCH_PIN_8, TOUCH_PIN_7, TOUCH_PIN_0, TOUCH_PIN_2, TOUCH_PIN_3, TOUCH_PIN_6, TOUCH_PIN_5, TOUCH_PIN_4};

bool scanningActive = false;

int startButtonState; int lastStartButtonState = HIGH; unsigned long lastStartDebounceTime = 0;
int endButtonState;   int lastEndButtonState = HIGH;   unsigned long lastEndDebounceTime = 0;

void setLedColor(int r, int g, int b) {
  digitalWrite(LED_RED_PIN, r);
  digitalWrite(LED_GREEN_PIN, g);
  digitalWrite(LED_BLUE_PIN, b);
}

void resetCurrentSensorValues() {
  for (int i = 0; i < 9; i++) {
    currentSensorValues[i].rawValue = 80; // Boşta varsayılan bir değer (modelinize göre ayarlayın)
    currentSensorValues[i].currentDuration = 0;
    currentSensorValues[i].touchStartTime = 0;
    currentSensorValues[i].isCurrentlyTouched = false;
  }
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_SwipePredict");
  Serial.println("ESP32 Tahmin Modu Başlatıldı. BBB'ye bağlanın.");

  pinMode(START_SCAN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(END_SCAN_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);

  resetCurrentSensorValues();
  setLedColor(LOW, LOW, HIGH); // Başlangıçta Mavi LED (Hazır)
}

void loop() {
  unsigned long currentTime = millis();

  // Başlat Butonu Kontrolü
  int currentStartReading = digitalRead(START_SCAN_BUTTON_PIN);
  if (currentStartReading != lastStartButtonState) { lastStartDebounceTime = currentTime; }
  if ((currentTime - lastStartDebounceTime) > DEBOUNCE_DELAY) {
    if (currentStartReading != startButtonState) {
      startButtonState = currentStartReading;
      if (startButtonState == LOW && !scanningActive) {
        scanningActive = true;
        resetCurrentSensorValues(); // Tarama başında değerleri sıfırla/ayarla
        SerialBT.println("CMD:START_SWIPE");
        Serial.println("Taramayı başlat butonuna basıldı, okumaya hazır!");
        setLedColor(LOW, HIGH, LOW); // Yeşil LED (Tarama aktif)
      }
    }
  }
  lastStartButtonState = currentStartReading;

  // Tarama Aktifse Anlık Veri Oku ve currentSensorValues'u Güncelle
  if (scanningActive) {
    for (int i = 0; i < 9; i++) {
      int val = touchRead(touchPins[i]);
      currentSensorValues[i].rawValue = val; // Her zaman en son okunan raw değeri kaydet

      if (val < TOUCH_THRESHOLD) { // Dokunma algılandı
        if (!currentSensorValues[i].isCurrentlyTouched) { // Yeni dokunma
          currentSensorValues[i].isCurrentlyTouched = true;
          currentSensorValues[i].touchStartTime = currentTime;
          currentSensorValues[i].currentDuration = 0; // Yeni dokunmada süreyi sıfırla
        } else { // Dokunma devam ediyor
          currentSensorValues[i].currentDuration = currentTime - currentSensorValues[i].touchStartTime;
        }
      } else { // Dokunma bitti veya yok
        if (currentSensorValues[i].isCurrentlyTouched) { // Dokunma yeni bittiyse
          // Süre zaten en son güncellenmişti, isCurrentlyTouched'ı false yap
          currentSensorValues[i].isCurrentlyTouched = false;
          // currentDuration o dokunmanın toplam süresi olarak kalır
        } else {
          // Eğer sürekli dokunulmuyorsa süreyi sıfırlayabiliriz veya son geçerli süreyi tutabiliriz.
          // Model anlık süre bekliyorsa, dokunulmadığında 0 olmalı.
          currentSensorValues[i].currentDuration = 0;
        }
      }
    }
  }

  // Bitir Butonu Kontrolü
  int currentEndReading = digitalRead(END_SCAN_BUTTON_PIN);
  if (currentEndReading != lastEndButtonState) { lastEndDebounceTime = currentTime; }
  if ((currentTime - lastEndDebounceTime) > DEBOUNCE_DELAY) {
    if (currentEndReading != endButtonState) {
      endButtonState = currentEndReading;
      if (endButtonState == LOW && scanningActive) {
        sendDataToBBB(); // Tarama bitince currentSensorValues'daki son değerleri gönder
        scanningActive = false;
        SerialBT.println("CMD:DATA_SENT");
        Serial.println("Tarama bitti ve veriler BBB'ye aktarıldı! Tahmin bekleniyor.");
        setLedColor(HIGH, LOW, LOW); // Kırmızı LED (İşleniyor/Bekleniyor)
      }
    }
  }
  lastEndButtonState = currentEndReading;
}

void sendDataToBBB() {
  String dataString = "";
  // 9 raw değer
  for (int i = 0; i < 9; i++) {
    dataString += String(currentSensorValues[i].rawValue);
    if (i < 8) dataString += ",";
  }
  dataString += ",";
  // 9 duration değeri
  for (int i = 0; i < 9; i++) {
    dataString += String(currentSensorValues[i].currentDuration);
    if (i < 8) dataString += ",";
  }

  SerialBT.println("DATA:" + dataString);
  Serial.println("Sent to BBB: " + dataString);
}