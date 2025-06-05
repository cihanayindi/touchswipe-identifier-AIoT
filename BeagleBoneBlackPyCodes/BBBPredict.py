import serial
import time
import os
import joblib
import numpy as np
# import pandas as pd # Eğer gelen string'i DataFrame'e çevirmek isterseniz

# --- Yapılandırma ---
SERIAL_PORT_DEVICE = "/dev/rfcomm0"
BAUD_RATE = 9600 # ESP32 ile aynı olmalı

# Kaydedilmiş model ve nesnelerin BBB üzerindeki yolları
MODEL_DIR = "/home/debian/ml_model/" # Bu yolu kendi dosya yapınıza göre güncelleyin
MODEL_FILENAME = os.path.join(MODEL_DIR, 'xgboost_swipe_model.joblib')
SCALER_FILENAME = os.path.join(MODEL_DIR, 'scaler.joblib')
ENCODER_FILENAME = os.path.join(MODEL_DIR, 'label_encoder.joblib')

# Model, scaler ve encoder'ı yükle
try:
    loaded_model = joblib.load(MODEL_FILENAME)
    loaded_scaler = joblib.load(SCALER_FILENAME)
    loaded_label_encoder = joblib.load(ENCODER_FILENAME)
    print("Model, scaler ve label encoder başarıyla yüklendi.")
    print(f"Yüklenen Label Encoder Sınıfları: {list(loaded_label_encoder.classes_)}")
except Exception as e:
    print(f"Model/Scaler/Encoder yüklenirken hata: {e}")
    print("Lütfen dosyaların doğru yolda olduğundan ve bozuk olmadığından emin olun.")
    loaded_model = None

def predict_user(swipe_features_list):
    if not loaded_model or not loaded_scaler or not loaded_label_encoder:
        print("Model, scaler veya encoder yüklenemedi. Tahmin yapılamıyor.")
        return None
    try:
        if len(swipe_features_list) != 18: # XGBoost modelinin beklediği özellik sayısı
            print(f"HATA: Beklenen özellik sayısı 18, alınan: {len(swipe_features_list)}")
            return None
        
        new_data_np = np.array(swipe_features_list).reshape(1, -1)
        new_data_scaled = loaded_scaler.transform(new_data_np)
        prediction_encoded = loaded_model.predict(new_data_scaled)
        predicted_user_label_numeric = prediction_encoded[0]
        predicted_user_name = loaded_label_encoder.inverse_transform([predicted_user_label_numeric])[0]
        
        return predicted_user_name
    except Exception as e:
        print(f"Tahmin sırasında hata: {e}")
        return None

def main():
    ser = None
    print("--- BeagleBone Black - Swipe Tahmin Sistemi ---")
    print(f"Seri port açılmaya çalışılıyor: {SERIAL_PORT_DEVICE}")

    if not all([os.path.exists(f) for f in [MODEL_FILENAME, SCALER_FILENAME, ENCODER_FILENAME]]):
        print("Model, scaler veya encoder dosyalarından biri bulunamadı. Lütfen yolları kontrol edin.")
        return

    try:
        ser = serial.Serial(SERIAL_PORT_DEVICE, BAUD_RATE, timeout=1)
        print(f"{SERIAL_PORT_DEVICE} başarıyla açıldı.")
        print("ESP32'den komut bekleniyor...")

        while True:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"ESP32'den alındı: '{line}'")
                        
                        if line == "CMD:START_SWIPE":
                            print("Swipe taraması başlatıldı, veri bekleniyor...")
                        elif line.startswith("DATA:"):
                            print("Swipe verisi alındı, tahmin yapılıyor...")
                            feature_string = line.split("DATA:", 1)[1]
                            try:
                                # Özellikleri float'a çevir (XGBoost genellikle float bekler)
                                swipe_features = [float(val) for val in feature_string.split(',')]
                                
                                predicted_user = predict_user(swipe_features)
                                if predicted_user:
                                    print(f"\n>>> TAHMİN: Bu kaydırma işlemini yapan kişi {predicted_user}!\n")
                                else:
                                    print(">>> TAHMİN: Kullanıcı tanımlanamadı.\n")

                            except ValueError:
                                print("HATA: Gelen veri sayısal değerlere çevrilemedi.")
                                print(f"Sorunlu veri: {feature_string}")
                            except Exception as e_parse:
                                print(f"Veri işlenirken hata: {e_parse}")

                        elif line == "CMD:DATA_SENT":
                            print("ESP32 veri gönderimini tamamladığını bildirdi.")
                except serial.SerialException as se:
                    print(f"Seri port hatası: {se}")
                    break 
                except UnicodeDecodeError:
                    print("Seri porttan okurken Unicode decode hatası.")
                except Exception as e_read:
                    print(f"Seri okuma/işleme sırasında beklenmedik hata: {e_read}")
            
            time.sleep(0.05)

    except serial.SerialException as e:
        print(f"Seri port ({SERIAL_PORT_DEVICE}) açılamadı: {e}")
    except KeyboardInterrupt:
        print("\nProgram kullanıcı tarafından sonlandırıldı.")
    finally:
        if ser and ser.is_open:
            ser.close()
            print("Seri port kapatıldı.")
        print("Sistem kapatılıyor.")

if __name__ == '__main__':
    if loaded_model:
        main()
    else:
        print("Model yüklenemediği için program başlatılamıyor.")