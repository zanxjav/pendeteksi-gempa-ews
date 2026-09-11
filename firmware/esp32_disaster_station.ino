/*
 * ==============================================================================
 * Project: GeoShield EWS - ESP32 Multi-Disaster Station Firmware
 * Author: Open Source Community
 * Target MCU: ESP32 DevKit V1 (NodeMCU-32S)
 * Sensors Supported:
 *   1. MPU-6050 6-DOF Accelerometer / Gyro (Gempa & Getaran)
 *   2. HC-SR04 Ultrasonic Distance Sensor (Level Air / Banjir)
 *   3. FC-37 / YL-83 Raindrop Sensor (Sensor Hujan)
 *   4. Keyestudio / Analog TDS Meter V1.0 (Total Dissolved Solids)
 *   5. 5V Active Buzzer (Sirine Lokasi)
 * ==============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

// ==========================================
// 1. PENGATURAN WIFI & SERVER WEB DASHBOARD
// ==========================================
const char* WIFI_SSID     = "NAMA_WIFI_ANDA";
const char* WIFI_PASSWORD = "PASSWORD_WIFI_ANDA";

// URL Endpoint Webhook / Backend Dashboard (Ganti dengan IP / Domain Anda)
const char* SERVER_API_URL = "http://192.168.1.100:8080/api/telemetry";

// Konfigurasi Identitas Posko / Stasiun
const char* STATION_ID   = "ST-01-JAKARTA";
const float STATION_LAT  = -6.2088;
const float STATION_LNG  = 106.8456;

// ==========================================
// 2. PINOUT HARDWARE ESP32
// ==========================================
#define I2C_SDA_PIN      21   // MPU6050 SDA
#define I2C_SCL_PIN      22   // MPU6050 SCL

#define HC_TRIG_PIN      5    // Ultrasonic HC-SR04 Trigger
#define HC_ECHO_PIN      18   // Ultrasonic HC-SR04 Echo

#define RAIN_ANALOG_PIN  34   // FC-37 Raindrop Analog (ADC1)
#define TDS_ANALOG_PIN   35   // Analog TDS Meter (ADC1)

#define BUZZER_PIN       4    // Active Buzzer EWS
#define LED_STATUS_PIN   2    // Built-in Blue LED ESP32

// ==========================================
// 3. MPU6050 REGISTER CONSTANTS
// ==========================================
#define MPU6050_ADDR         0x68
#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B

// Kalibrasi Muka Air & Sensor
const float DISTANCE_TO_RIVER_BED_CM = 250.0; // Jarak sensor ke dasar sungai saat kering

unsigned long lastTelemetryTime = 0;
const unsigned long TELEMETRY_INTERVAL_MS = 500; // Kirim data tiap 500ms

// ==========================================
// INICIALISASI
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[GeoShield] Memulai Sistem Stasiun Multi-Bencana...");

  // Setup GPIO Pin
  pinMode(HC_TRIG_PIN, OUTPUT);
  pinMode(HC_ECHO_PIN, INPUT);
  pinMode(RAIN_ANALOG_PIN, INPUT);
  pinMode(TDS_ANALOG_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_STATUS_PIN, OUTPUT);
  
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_STATUS_PIN, LOW);

  // Inisialisasi I2C & MPU6050
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  initMPU6050();

  // Koneksi WiFi
  connectToWiFi();
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  // 1. Baca data sensor akselerasi getaran
  float pga_g = readSeismicPga();

  // 2. Baca ketinggian muka air (Banjir)
  float water_level_cm = readWaterLevel();

  // 3. Baca sensor hujan analog
  int rain_raw = analogRead(RAIN_ANALOG_PIN);
  float rain_rate_mmh = mapRainfall(rain_raw);

  // 4. Baca sensor TDS air
  float tds_ppm = readTdsPpm();

  // 5. Cek Ambang Batas Darurat (Local EWS Buzzer)
  bool isEmergency = (pga_g >= 0.08 || water_level_cm >= 150.0);
  if (isEmergency) {
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(LED_STATUS_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(LED_STATUS_PIN, LOW);
  }

  // 6. Kirim Telemetri ke Web Dashboard secara berkala
  if (millis() - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = millis();
    sendTelemetryHttp(pga_g, water_level_cm, rain_raw, rain_rate_mmh, tds_ppm);
  }

  delay(20);
}

// ==========================================
// FUNGSI SENSOR SEISMIK / MPU6050
// ==========================================
void initMPU6050() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_PWR_MGMT_1);
  Wire.write(0x00); // Bangunkan MPU6050 dari sleep mode
  byte status = Wire.endTransmission();
  if (status == 0) {
    Serial.println("[MPU6050] Berhasil terhubung via I2C!");
  } else {
    Serial.println("[MPU6050] Gagal terhubung! Periksa kabel SDA/SCL.");
  }
}

float readSeismicPga() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(MPU6050_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU6050_ADDR, (size_t)6, true);

  if (Wire.available() >= 6) {
    int16_t rawX = Wire.read() << 8 | Wire.read();
    int16_t rawY = Wire.read() << 8 | Wire.read();
    int16_t rawZ = Wire.read() << 8 | Wire.read();

    // Sensitivitas skala default +/- 2g = 16384 LSB/g
    float ax = (float)rawX / 16384.0;
    float ay = (float)rawY / 16384.0;
    float az = (float)rawZ / 16384.0;

    // Vektor getaran dinamis tanpa gravitasi statis (az - 1.0g)
    float dynamicAccel = sqrt(ax * ax + ay * ay + (az - 1.0) * (az - 1.0));
    return dynamicAccel;
  }
  return 0.002;
}

// ==========================================
// FUNGSI SENSOR BANJIR / HC-SR04
// ==========================================
float readWaterLevel() {
  digitalWrite(HC_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(HC_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(HC_TRIG_PIN, LOW);

  long duration = pulseIn(HC_ECHO_PIN, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 0.0;

  // Kecepatan suara ~0.0343 cm/us
  float distance_sensor_to_water = (duration * 0.0343) / 2.0;
  float water_level_cm = DISTANCE_TO_RIVER_BED_CM - distance_sensor_to_water;
  if (water_level_cm < 0) water_level_cm = 0.0;

  return water_level_cm;
}

// ==========================================
// FUNGSI SENSOR HUJAN & TDS
// ==========================================
float mapRainfall(int rawAnalog) {
  // ESP32 ADC: 4095 (Kering) -> 0 (Basah lebat)
  if (rawAnalog >= 4000) return 0.0;
  float rainRate = ((4095.0 - rawAnalog) / 4095.0) * 100.0; // mm/jam
  return rainRate;
}

float readTdsPpm() {
  int rawTds = analogRead(TDS_ANALOG_PIN);
  float voltage = (rawTds / 4095.0) * 3.3; // Tegangan ESP32 ADC
  // Rumus estimasi TDS Analog Meter V1.0
  float tdsPpm = (133.42 * voltage * voltage * voltage - 255.86 * voltage * voltage + 857.39 * voltage) * 0.5;
  if (tdsPpm < 0) tdsPpm = 0;
  return tdsPpm;
}

// ==========================================
// KONEKSI WIFI & HTTP POST
// ==========================================
void connectToWiFi() {
  Serial.print("[WiFi] Menghubungkan ke ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Berhasil Terhubung!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Gagal terhubung ke WiFi. Berjalan dalam mode offline.");
  }
}

void sendTelemetryHttp(float pga, float waterLevel, int rainRaw, float rainRate, float tds) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(SERVER_API_URL);
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{";
  jsonPayload += "\"station_id\":\"" + String(STATION_ID) + "\",";
  jsonPayload += "\"seismic\":{\"pga\":" + String(pga, 4) + "},";
  jsonPayload += "\"flood\":{\"waterLevelCm\":" + String(waterLevel, 1) + "},";
  jsonPayload += "\"rain\":{\"rawAnalog\":" + String(rainRaw) + ",\"rateMmh\":" + String(rainRate, 1) + "},";
  jsonPayload += "\"tds\":{\"ppm\":" + String(tds, 1) + "},";
  jsonPayload += "\"location\":{\"lat\":" + String(STATION_LAT, 6) + ",\"lng\":" + String(STATION_LNG, 6) + "}";
  jsonPayload += "}";

  int httpCode = http.POST(jsonPayload);
  if (httpCode > 0) {
    // Berhasil kirim
  }
  http.end();
}
