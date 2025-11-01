#define BLYNK_TEMPLATE_ID "Your Template ID"
#define BLYNK_TEMPLATE_NAME "Your Template Name"
#define BLYNK_AUTH_TOKEN "Your Auth Token From BLYNK"

#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define BUZZER_PIN      D3
#define LED_PIN         D4
#define DHT_PIN         D7
#define GAS_PIN         A0
#define FLAME_PIN       D8
#define RELAY1_PIN      D5
#define RELAY2_PIN      D6
#define LCD_SDA_PIN     D2
#define LCD_SCL_PIN     D1

#define DHT_TYPE DHT11
#define TEMP_THRESHOLD      34.0f 
#define HUMIDITY_THRESHOLD  75.0f
#define GAS_THRESHOLD       300

#define EVENT_FIRE_ALERT    "fire_alert"
#define EVENT_GAS_ALERT     "gas_alert"
#define EVENT_TEMP_ALERT    "temp_alert"

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "Your Wifi Name";
char pass[] = "Your Wifi Password";
String deviceLocation = "Ruang H17 Gedung H, Teknik Elektro Universitas Lampung.";

DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- VARIABEL ALARM ---
bool isAlarmActive = false;
unsigned long lastAlarmActionTime = 0;
bool isBuzzerOn = false;
int currentBuzzerFreq = 0;
// Variabel durasi diubah dari const menjadi variabel biasa untuk diatur secara dinamis
unsigned long currentBeepOnTime = 500;  
unsigned long currentBeepOffTime = 1000; 

unsigned long lastSensorRead = 0;
const unsigned long sensorReadInterval = 2000;
int lcdScreenState = 0;

BLYNK_WRITE(V0) {
  digitalWrite(RELAY1_PIN, param.asInt());
}

BLYNK_WRITE(V1) {
  digitalWrite(RELAY2_PIN, param.asInt());
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(FLAME_PIN, INPUT_PULLUP);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.init();
  lcd.backlight();
  dht.begin();
  
  lcd.print("Connecting WiFi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  lcd.clear();
  lcd.print("Connecting Blynk");
  Blynk.begin(auth, ssid, pass);
  Blynk.virtualWrite(V6, deviceLocation);

  lcd.clear();
  lcd.print("Sistem SIAP!");
  Serial.println("\nSistem SIAP! Memulai monitoring...");
  delay(1500);
}

void loop() {
  Blynk.run();  
  handleAlarmSystem(); 
  if (millis() - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = millis();
    checkSensorsAndAlerts();
  }
}

// --- FUNGSI handleAlarmSystem ---
void handleAlarmSystem() {
  if (!isAlarmActive) {
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, LOW);
    isBuzzerOn = false;
    return;
  }

  digitalWrite(LED_PIN, (millis() % 400) < 200);

  if (isBuzzerOn) {
    if (millis() - lastAlarmActionTime > currentBeepOnTime) { // variabel dinamis
      noTone(BUZZER_PIN);
      isBuzzerOn = false;
      lastAlarmActionTime = millis();
    }
  } else {
    if (millis() - lastAlarmActionTime > currentBeepOffTime) { // variabel dinamis
      tone(BUZZER_PIN, currentBuzzerFreq);
      isBuzzerOn = true;
      lastAlarmActionTime = millis();
    }
  }
}

// --- FUNGSI checkSensorsAndAlerts ---
void checkSensorsAndAlerts() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int gasValue = analogRead(GAS_PIN);

  bool flameDetected = (digitalRead(FLAME_PIN) == LOW);
  bool gasDetected = (gasValue > GAS_THRESHOLD);
  bool tempHigh = (temp > TEMP_THRESHOLD);

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Gagal membaca dari sensor DHT!");
    return;
  }

  Blynk.virtualWrite(V2, temp);
  Blynk.virtualWrite(V3, hum);
  Blynk.virtualWrite(V4, gasValue);
  Blynk.virtualWrite(V5, flameDetected ? 255 : 0);

  if (flameDetected && gasDetected && tempHigh) {
    isAlarmActive = true;
    // --- Profil Suara: PALING CEPAT ---
    currentBuzzerFreq = 3000;
    currentBeepOnTime = 150;  
    currentBeepOffTime = 150;
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("!!KEBAKARAN!!");
    lcd.setCursor(0,1);
    lcd.print("Api,Gas,Suhu UP");
    Blynk.logEvent(EVENT_FIRE_ALERT, "KEBAKARAN TERDETEKSI di " + deviceLocation);

  } else if (flameDetected) {
    isAlarmActive = true;
    // --- Profil Suara: CEPAT ---
    currentBuzzerFreq = 2500;
    currentBeepOnTime = 200; 
    currentBeepOffTime = 400;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("!!PERINGATAN!!");
    lcd.setCursor(0,1);
    lcd.print("API TERDETEKSI");
    
  } else if (gasDetected) {
    isAlarmActive = true;
    // --- Profil Suara: SEDANG ---
    currentBuzzerFreq = 1500;
    currentBeepOnTime = 300;
    currentBeepOffTime = 700;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("!!PERINGATAN!!");
    lcd.setCursor(0,1);
    lcd.print("GAS BERBAHAYA");
    Blynk.logEvent(EVENT_GAS_ALERT, "Gas berbahaya terdeteksi di " + deviceLocation);

  } else if (tempHigh) {
    isAlarmActive = true;
    // --- Profil Suara: PALING LAMBAT ---
    currentBuzzerFreq = 800;
    currentBeepOnTime = 500;
    currentBeepOffTime = 1500;

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("!!PERINGATAN!!");
    lcd.setCursor(0,1);
    lcd.print("SUHU TINGGI: " + String(temp, 1) + "C");
    Blynk.logEvent(EVENT_TEMP_ALERT, "Suhu terlalu tinggi (" + String(temp, 1) + "C)");
    
  } else {
    if (isAlarmActive) {
      lcd.clear();
    }
    isAlarmActive = false;
    displayNormalStatus(temp, hum, gasValue, flameDetected);
  }
}

// FUNGSI displayNormalStatus
void displayNormalStatus(float temp, float hum, int gasValue, bool flame) {
  if (isAlarmActive) return;
  // Menggunakan metode padding spasi
  String line1, line2;

  if (lcdScreenState == 0) {
    line1 = "Suhu:   " + String(temp, 1) + "C";
    line2 = "Lembab: " + String(hum, 1) + "%";
    lcdScreenState = 1;
  } else {
    line1 = "Gas: " + String(gasValue);
    line2 = "Api:    " + String(flame ? "ADA" : "Aman");
    lcdScreenState = 0;
  }
  
  while(line1.length() < 16) line1 += " ";
  while(line2.length() < 16) line2 += " ";

  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  
  Serial.print("Suhu: " + String(temp, 1) + "C");
  Serial.print(" | Lembab: " + String(hum, 1) + "%");
  Serial.print(" | Gas: " + String(gasValue));
  Serial.print(" | Api: ");
  Serial.println(flame ? "ADA" : "Aman");
}