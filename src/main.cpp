// ESP32 code: src/main.cpp (adapted for servos only, no LED, no delays)
#include <Arduino.h>
#include <ESP32Servo.h>
#include <WiFi.h>

#define WIFI_SSID "Robolab124"
#define WIFI_PASSWORD "wifi123123123"
#define SERVER_IP "192.168.1.121" // Your PC IP
#define SERVER_PORT 5000

const int PIN_SERVO1 = 9;
const int PIN_SERVO2 = 10;
const int PIN_SERVO3 = 11;
const int PIN_SERVO4 = 12;

Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

WiFiClient client;

void setup() {
  Serial.begin(115200);
  delay(300); // Short init delay
  Serial.println("\nServo ESP32-S3-DevKitC-1 Network Control");

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Attach servos
  servo1.attach(PIN_SERVO1);
  servo2.attach(PIN_SERVO2);
  servo3.attach(PIN_SERVO3);
  servo4.attach(PIN_SERVO4);

  // Initial positions (90)
  servo1.write(90);
  servo2.write(90);
  servo3.write(90);
  servo4.write(90);
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

  // Read 4 bytes (positions for servos, binary)
  // Comment: Each byte is uint8_t angle (0-180), independent set, no delays for fast response
  if (client.available() >= 4) {
    uint8_t pos1 = client.read();
    uint8_t pos2 = client.read();
    uint8_t pos3 = client.read();
    uint8_t pos4 = client.read();

    // Set servos independently (no delay between)
    servo1.write(pos1);
    servo2.write(pos2);
    servo3.write(pos3);
    servo4.write(pos4);

    // Optional debug
    Serial.printf("Set servos: %d, %d, %d, %d\n", pos1, pos2, pos3, pos4);
  }

  // No busy-wait delay, but yield for WiFi
  yield();
}