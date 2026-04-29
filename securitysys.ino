#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
 
// --- Pin Config ---
#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER_PIN 19
#define LED_PIN 2
#define SDA_PIN 21
#define SCL_PIN 22
 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_MPU6050 mpu;
 
// --- WiFi & Cloud Config ---
const char* ssid     = "Chin_2.4G";
const char* password = "!159875321AAs";
String cloud_url     = "https://script.google.com/macros/s/AKfycbx6hBz2SpfdNSrj76-2-XOBoi1vAv0P1lPdI4vvP96RcpVK8ffkH7CCwdLM-EDtck04uQ/exec";
 
String cloud_command = "NORMAL";
unsigned long lastSend = 0;
const int sendInterval = 10000;
 
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  Wire.begin(SDA_PIN, SCL_PIN);
 
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED Error");
  if (!mpu.begin()) Serial.println("MPU6050 Error");
 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi Connected!");
}
 
void loop() {
  // หากคำสั่งเป็น OFF จะหยุดการอ่านเซนเซอร์เพื่อประหยัดทรัพยากร/พลังงาน
  if (cloud_command == "OFF") {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    updateOLED(0, 0, false);
    // ตรวจสอบคำสั่งใหม่ทุก 5 วินาทีแม้จะ OFF อยู่
    if (millis() - lastSend > 5000) {
      sendToCloud(0, 0, 0, 0, false);
      lastSend = millis();
    }
  } else {
    // --- 1. อ่านค่าเซนเซอร์ (ทำงานในโหมด NORMAL และ MUTE) ---
    digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    float distance = pulseIn(ECHO_PIN, HIGH) * 0.034 / 2;
 
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float vib = sqrt(sq(a.acceleration.x) + sq(a.acceleration.y) + sq(a.acceleration.z));
 
    // --- 2. วิเคราะห์การบุกรุก ---
    bool isDetected = false;
    if (distance > 0 && distance < 25) isDetected = true;
    if (vib > 15.0 || vib < 5.0) isDetected = true;
 
    // --- 3. ควบคุม Hardware ตามโหมด ---
    if (isDetected) {
      digitalWrite(LED_PIN, HIGH);
      // ดังเฉพาะโหมด NORMAL เท่านั้น
      if (cloud_command == "NORMAL") digitalWrite(BUZZER_PIN, HIGH);
      else digitalWrite(BUZZER_PIN, LOW); // กรณีโหมด MUTE
    } else {
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    }
 
    // --- 4. ส่งข้อมูลเข้า Cloud ---
    if (millis() - lastSend > sendInterval || (isDetected && (millis() - lastSend > 2000))) {
      sendToCloud(distance, a.acceleration.x, a.acceleration.y, vib, isDetected);
      lastSend = millis();
    }
 
    updateOLED(distance, vib, isDetected);
  }
  delay(100);
}
 
void updateOLED(float d, float v, bool alarm) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("MODE: "); display.println(cloud_command);
  if (cloud_command != "OFF") {
    display.print("Dist: "); display.print(d, 1); display.println(" cm");
    display.print("Vib : "); display.println(v, 1);
    if (alarm) {
      display.setCursor(0, 40); display.setTextSize(2); display.println("!!ALARM!!");
    } else {
      display.setCursor(0, 40); display.setTextSize(1); display.println("SYSTEM SECURE");
    }
  } else {
    display.setCursor(0, 30); display.setTextSize(2); display.println("SYS OFF");
  }
  display.setTextSize(1);
  display.display();
}
 
void sendToCloud(float dist, float tx, float ty, float v, bool alarm) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(cloud_url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    StaticJsonDocument<256> doc;
    doc["event"] = (cloud_command == "OFF") ? "Standby" : (alarm ? "Intrusion" : "Normal");
    doc["distance"] = dist;
    doc["tilt_x"] = tx;
    doc["tilt_y"] = ty;
    doc["vibration"] = v;
    doc["status"] = (cloud_command == "OFF") ? "DISABLED" : (alarm ? "!!ALARM!!" : "SECURE");
 
    String jsonStr;
    serializeJson(doc, jsonStr);
    int httpCode = http.POST(jsonStr);
 
    if (httpCode > 0) {
      String response = http.getString();
      StaticJsonDocument<200> resDoc;
      deserializeJson(resDoc, response);
      if (resDoc.containsKey("command")) {
        cloud_command = resDoc["command"].as<String>();
      }
    }
    http.end();
  }
}