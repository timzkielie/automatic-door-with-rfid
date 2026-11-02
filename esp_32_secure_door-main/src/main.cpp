#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// ---------- CONFIG ----------
// Networks are tried in this order (first one first, then the next)
struct Net { const char* ssid; const char* pass; };
Net NETS[] = {
  {"Connecting..", "L@nuz0_F@mily"},   // 1st choice
  {"Deuz",          "12345678"},             // fallback
};
#define WIFI_TRY_MS 10000

const char* SUPABASE_URL = "https://zxysonquxjsylmnhnpbw.supabase.co";
const char* SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Inp4eXNvbnF1eGpzeWxtbmhucGJ3Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3OTIzODcwNzYsImV4cCI6MjEwNzk2MzA3Nn0.Y1fGhNvaGWqeNrjmsE8BVTAWGr7LiBxUgfT4ARRnur0";

// Authorized cards (UID in uppercase hex, no spaces)
struct Card { const char* uid; const char* name; };
Card CARDS[] = {
  {"A1B2C3D4", "Guest"},
  {"EA447A32", "Admin"},
};

// ---------- PINS (all on the same side of a 30-pin DevKit) ----------
#define RC522_SS   5
#define RC522_RST  4
// SCK=18, MISO=19, MOSI=23 (default VSPI)
// I2C SDA=21, SCL=22 (default)
#define SERVO_PIN  15

#define OLED_ADDR 0x3C  // try 0x3D if blank
#define OLED_W 128
#define OLED_H 64
// 180-degree servo: write(angle) goes to that position and holds it.
#define DOOR_CLOSED_ANGLE 0
#define DOOR_OPEN_ANGLE   90    // raise up to 180 for a bigger swing
#define DOOR_OPEN_MS      5000  // how long the door stays open
#define WIFI_RETRY_MS     30000

MFRC522 rfid(RC522_SS, RC522_RST);
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
Servo door;

unsigned long openedAt = 0;
unsigned long lastWifiTry = 0;
bool isOpen = false;

void show(const String& l0, const String& l1 = "", const String& l2 = "", const String& l3 = "") {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  const String* lines[4] = {&l0, &l1, &l2, &l3};
  for (int i = 0; i < 4; i++) {
    oled.setCursor(0, i * 16);
    oled.print(*lines[i]);
  }
  oled.display();
}

void showIdle() {
  show("   SECURE DOOR", "", " Scan your card...",
       WiFi.status() == WL_CONNECTED ? String(" WiFi: ") + WiFi.SSID() : String(" WiFi: OFFLINE"));
}

// Tries each network in NETS[] in order. Returns true when connected.
bool connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  for (auto& n : NETS) {
    Serial.printf("Trying WiFi: %s\n", n.ssid);
    show("Connecting WiFi...", n.ssid);
    WiFi.disconnect();
    WiFi.begin(n.ssid, n.pass);
    unsigned long t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < WIFI_TRY_MS) delay(250);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Connected to %s\n", n.ssid);
      return true;
    }
  }
  Serial.println("No WiFi network available");
  return false;
}

String uidToString() {
  String s;
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) s += "0";
    s += String(rfid.uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

const char* lookup(const String& uid) {
  for (auto& c : CARDS) if (uid == c.uid) return c.name;
  return nullptr;
}

void sendLog(const String& uid, const char* name, const char* status, const char* doorState) {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("No WiFi, log skipped"); return; }

  JsonDocument doc;
  doc["uid"] = uid;
  doc["name"] = name ? name : "Unknown";
  doc["status"] = status;
  doc["door_state"] = doorState;
  String body;
  serializeJson(doc, body);

  WiFiClientSecure client;
  client.setInsecure();               // simple; pin the cert for production
  HTTPClient http;
  http.begin(client, String(SUPABASE_URL) + "/rest/v1/door_logs");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_KEY);
  http.addHeader("Prefer", "return=minimal");
  int code = http.POST(body);
  Serial.printf("POST %s -> %d\n", body.c_str(), code);
  http.end();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) Serial.println("OLED not found");

  ESP32PWM::allocateTimer(0);
  door.setPeriodHertz(50);
  door.attach(SERVO_PIN, 500, 2400);
  door.write(DOOR_CLOSED_ANGLE);

  SPI.begin(18, 19, 23, RC522_SS);
  rfid.PCD_Init();

  connectWifi();
  lastWifiTry = millis();
  showIdle();
}

void loop() {
  // auto-close
  if (isOpen && millis() - openedAt >= DOOR_OPEN_MS) {
    Serial.println("Servo -> closed");
    door.write(DOOR_CLOSED_ANGLE);
    isOpen = false;
    sendLog("-", "System", "granted", "closed");
    showIdle();
  }

  // if offline and idle, retry WiFi every 30 s
  if (!isOpen && WiFi.status() != WL_CONNECTED && millis() - lastWifiTry > WIFI_RETRY_MS) {
    connectWifi();
    lastWifiTry = millis();
    showIdle();
  }

  // ignore cards while the door is open
  if (isOpen) return;
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = uidToString();
  const char* name = lookup(uid);
  Serial.println("Card: " + uid);

  if (name) {
    show("  ACCESS GRANTED", String("Welcome ") + name, "UID: " + uid);
    sendLog(uid, name, "granted", "opened");
    delay(500);
    Serial.println("Servo -> open");
    door.write(DOOR_OPEN_ANGLE);
    isOpen = true;
    openedAt = millis();          // timer starts when the door actually opens
  } else {
    show("  ACCESS DENIED", "Unknown card", "UID: " + uid);
    sendLog(uid, nullptr, "denied", "closed");
    delay(2000);
    showIdle();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}