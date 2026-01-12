#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// ================ НАСТРОЙКИ ПОЛЬЗОВАТЕЛЯ ================
const char* ssid = "Robolab124";                    // Ваш WiFi
const char* password = "wifi123123123";
// const char* WS_HOST = "a.ardu.live";
// const uint16_t WS_PORT = 444;           
// const char* WS_PATH = "/wsar";

const char* WS_HOST = "192.168.1.121";
const uint16_t WS_PORT = 8096;           
const char* WS_PATH = "/wsar";

// УНИКАЛЬНЫЙ 16-ЗНАЧНЫЙ КОД УСТРОЙСТВА (A-Z, a-z, 0-9)
const char* DEVICE_ID = "ABCD1234EFGH5678";   // ←←←← ИЗМЕНИТЕ НА СВОЙ КОД

// ================ RGB LED НАСТРОЙКИ ================
#define LED_PIN     48          // Пин для WS2812 / NeoPixel на ESP32-S3
#define NUM_LEDS    1           // Количество светодиодов (обычно 1 встроенный)
#define BRIGHTNESS  255
CRGB leds[NUM_LEDS];

WebSocketsClient webSocket;

bool isIdentified = false;
unsigned long lastHeartbeat = 0;

void setColor(uint8_t r, uint8_t g, uint8_t b, bool on) {
  if (on) {
    leds[0] = CRGB(r, g, b);
  } else {
    leds[0] = CRGB::Black;
  }
  FastLED.show();
}

void sendLog(const char* message) {
  if (!webSocket.isConnected()) return;
  
  DynamicJsonDocument doc(256);
  doc["ty"] = "log";
  doc["me"] = message;
  doc["de"] = DEVICE_ID;
  doc["on"] = (leds[0] != CRGB::Black);
  doc["r"] = leds[0].r;
  doc["g"] = leds[0].g;
  doc["b"] = leds[0].b;
  
  String output;
  serializeJson(doc, output);
  webSocket.sendTXT(output);
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      isIdentified = false;
      break;
      
    case WStype_CONNECTED: {
      Serial.printf("[WS] Connected to: %s\n", payload);
      
      // Сначала отправляем тип клиента
      webSocket.sendTXT("{\"ty\":\"clt\",\"ct\":\"esp\"}");
      
      // Затем идентификация — переменная внутри блока {}
      String idMsg = "{\"ty\":\"idn\",\"de\":\"" + String(DEVICE_ID) + "\"}";
      webSocket.sendTXT(idMsg);
      break;
    }
      
    case WStype_TEXT: {
      String text = (char*)payload;
      Serial.printf("[WS] Received: %s\n", text.c_str());
      
      DynamicJsonDocument doc(512);
      DeserializationError error = deserializeJson(doc, text);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
      }
      
      const char* ty = doc["ty"];
      if (!ty) return;
      
      if (strcmp(ty, "sys") == 0 && doc.containsKey("st") && strcmp(doc["st"], "con") == 0) {
        isIdentified = true;
        Serial.println("Успешно идентифицирован!");
        sendLog("ESP32 RGB подключён");
        return;
      }
      
      const char* co = doc["co"];
      if (!co) return;
      
      if (strcmp(co, "RGB") == 0) {
        bool on = doc["pa"]["on"];
        uint8_t r = doc["pa"]["r"] | 0;
        uint8_t g = doc["pa"]["g"] | 0;
        uint8_t b = doc["pa"]["b"] | 0;
        
        setColor(r, g, b, on);
        sendLog(on ? "Цвет установлен" : "Свет выключен");
        
        // Подтверждение
        DynamicJsonDocument ack(256);
        ack["ty"] = "ack";
        ack["co"] = "RGB";
        ack["de"] = DEVICE_ID;
        JsonObject pa = ack.createNestedObject("pa");
        pa["on"] = on;
        pa["r"] = r;
        pa["g"] = g;
        pa["b"] = b;
        String out;
        serializeJson(ack, out);
        webSocket.sendTXT(out);
      }
      break;
    }
      
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32 RGB Controller Starting...");

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  setColor(0, 0, 0, false);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  //webSocket.beginSSL(WS_HOST, WS_PORT, WS_PATH);
  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();
  
  if (isIdentified && millis() - lastHeartbeat > 10000) {
    lastHeartbeat = millis();
    sendLog("heartbeat");
  }
}