// ESP32 code: src/main.cpp (for PlatformIO) or .ino for Arduino IDE

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>

#define BRIGHTNESS 30     // 20–60 обычно комфортно, 255 = очень ярко
#define PIN_LED    48    // 38 - если новая версия платы
#define NUM_LEDS   1

// WiFi credentials (replace with your own)
#define WIFI_SSID "Robolab124"
#define WIFI_PASSWORD "wifi123123123"

// Default server (PC) IP and port - these can be changed and reflashed
#define SERVER_IP "192.168.1.121"
#define SERVER_PORT 5000

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);
WiFiClient client;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nRainbow ESP32-S3-DevKitC-1 Network Control");

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();          // сразу выключаем
}

void loop() {
  if (!client.connected()) {
    if (client.connect(SERVER_IP, SERVER_PORT)) {
      Serial.println("Connected to server");
    } else {
      Serial.println("Connection failed - retrying in 5s");
      delay(5000);
      return;
    }
  }

  // Read RGB commands (3 bytes: R, G, B)
  if (client.available() >= 3) {
    uint8_t r = client.read();
    uint8_t g = client.read();
    uint8_t b = client.read();

    // Set the LED color
    strip.setPixelColor(0, r, g, b);
    strip.show();

    // Optional debug print
    Serial.printf("Set color: (%d, %d, %d)\n", r, g, b);
  }

  delay(10);  // Small delay to avoid busy loop
}