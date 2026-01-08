#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ServoEasing.hpp>

using namespace websockets;

// Пины для ESP32-S3
#define PIN_ENA     18
#define PIN_IN1     19
#define PIN_IN2     20
#define PIN_IN3     21
#define PIN_IN4     17
#define PIN_ENB     15
#define PIN_RELAY   16      // active LOW
#define PIN_SERVO1  13
#define PIN_SERVO2  14
#define PIN_VOLTAGE 4

// Настройки
const char* ssid       = "Robolab124";
const char* password   = "wifi123123123";
const char* ws_url     = "wss://a.ardu.live:444/wsar";
const char* DEVICE_ID  = "9999999999999999";  // 16 символов

ServoEasing Servo1, Servo2;
WebsocketsClient client;

bool identified = false;
unsigned long lastStatusTx = 0;
unsigned long lastClientHbTime = 0;
bool enableMotorProtection = false;

// Типы сообщений
#define CMD_IDENTIFY       0x01
#define CMD_CLIENT_TYPE    0x02
#define CMD_HEARTBEAT      0x10
#define CMD_HBT_MOTOR      0x11
#define CMD_MOTOR          0x20
#define CMD_SERVO_ABS      0x30
#define CMD_RELAY          0x40
#define CMD_ALARM          0x41

#define RSP_FULL_STATUS    0x50
#define RSP_ACK            0x51

// ────────────────────────────────────────────────────────────────
void sendBinary(const uint8_t* data, size_t len) {
  if (client.available()) {
    client.sendBinary((const char*)data, len);
  }
}

void sendFullStatus() {
  uint8_t buf[9] = {0};
  buf[0] = RSP_FULL_STATUS;
  buf[1] = (digitalRead(PIN_RELAY) == LOW) ? 1 : 0;
  buf[2] = Servo1.read();
  buf[3] = Servo2.read();

  int raw = analogRead(PIN_VOLTAGE);
  buf[4] = highByte(raw);
  buf[5] = lowByte(raw);

  buf[6] = 0;
  buf[7] = 0;
  buf[8] = (uint8_t)constrain(WiFi.RSSI(), -128, 127);

  sendBinary(buf, sizeof(buf));
}

void stopMotors() {
  analogWrite(PIN_ENA, 0);
  analogWrite(PIN_ENB, 0);
}

// ────────────────────────────────────────────────────────────────
void onMessageCallback(WebsocketsMessage message) {
  if (!message.isBinary()) {
    Serial.println("Получено НЕ бинарное сообщение — игнорируем");
    return;
  }

  const uint8_t* data = reinterpret_cast<const uint8_t*>(message.c_str());
  size_t len = message.length();

  if (len == 0) return;

  uint8_t cmd = data[0];
  Serial.printf("Получено бинарное: cmd=0x%02X, len=%d  ", cmd, len);

  switch (cmd) {
    case CMD_HEARTBEAT:
      Serial.println("→ HEARTBEAT");
      break;

    case CMD_HBT_MOTOR:
      Serial.print("HBT_MOTOR ");
      lastClientHbTime = millis();
      enableMotorProtection = true;
      break;

    case CMD_MOTOR:
      if (len < 5) { Serial.println("→ CMD_MOTOR: слишком короткое"); break; }
      {
        char motor = data[1];
        uint8_t speed = data[2];
        uint8_t dir = data[3];

        Serial.printf("→ MOTOR %c: speed=%d, dir=%d\n", motor, speed, dir);

        uint8_t pwmPin = (motor == 'A') ? PIN_ENA : PIN_ENB;
        uint8_t inPin1 = (motor == 'A') ? PIN_IN1 : PIN_IN3;
        uint8_t inPin2 = (motor == 'A') ? PIN_IN2 : PIN_IN4;

        analogWrite(pwmPin, speed);

        if (dir == 1) {
          digitalWrite(inPin1, HIGH);
          digitalWrite(inPin2, LOW);
        } else if (dir == 2) {
          digitalWrite(inPin1, LOW);
          digitalWrite(inPin2, HIGH);
        } else {
          digitalWrite(inPin1, LOW);
          digitalWrite(inPin2, LOW);
        }

        lastClientHbTime = millis();
        enableMotorProtection = true;
      }
      break;

    case CMD_SERVO_ABS:
      if (len < 4) { Serial.println("→ CMD_SERVO_ABS: слишком короткое"); break; }
      {
        uint8_t num = data[1];
        uint8_t angle = constrain(data[2], 0, 180);
        Serial.printf("→ SERVO %d → angle=%d\n", num, angle);

        if (num == 1) Servo1.write(angle);
        else if (num == 2) Servo2.write(angle);
      }
      break;

    case CMD_RELAY:
      if (len < 3) { Serial.println("→ CMD_RELAY: слишком короткое"); break; }
      {
        uint8_t state = data[1];
        Serial.printf("→ RELAY state=%d\n", state);
        digitalWrite(PIN_RELAY, state ? LOW : HIGH);
      }
      break;

    case CMD_ALARM:
      if (len < 2) { Serial.println("→ CMD_ALARM: слишком короткое"); break; }
      Serial.printf("→ ALARM state=%d\n", data[1]);
      break;

    default:
      Serial.printf("→ НЕИЗВЕСТНАЯ КОМАНДА 0x%02X\n", cmd);
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  switch (event) {
    case WebsocketsEvent::ConnectionOpened:
      Serial.println("[WS] Connected");
      identified = false;

      {
        uint8_t buf_type[2] = {CMD_CLIENT_TYPE, 2};
        sendBinary(buf_type, 2);
      }

      {
        uint8_t buf_id[17];
        buf_id[0] = CMD_IDENTIFY;
        memcpy(buf_id + 1, DEVICE_ID, 16);
        sendBinary(buf_id, 17);
      }

      identified = true;
      break;

    case WebsocketsEvent::ConnectionClosed:
      Serial.println("[WS] Disconnected");
      identified = false;
      stopMotors();
      enableMotorProtection = false;
      break;
  }
}

// ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Binary Protocol 2026 with HBT_MOTOR на ESP32-S3 ===\n");

  Servo1.attach(PIN_SERVO1, 90);
  Servo2.attach(PIN_SERVO2, 90);
  Servo1.write(90);
  Servo2.write(90);

  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_ENB, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH);

  stopMotors();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  client.onMessage(onMessageCallback);
  client.onEvent(onEventsCallback);

  // Для ESP32 в библиотеке gilmaimon/ArduinoWebsockets WSS работает БЕЗ setInsecure() и БЕЗ fingerprint
  // Просто подключаемся к wss://...
  // Если сервер использует валидный сертификат (Let's Encrypt или подобный), цепочка проверяется автоматически.
  // На ESP8266 работало с игнором, на ESP32 — строже, но для вашего сервера должно пройти.

  client.connect(ws_url);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost → reconnect");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  if (!client.available()) {
    Serial.print("WS reconnect... ");
    if (client.connect(ws_url)) {
      Serial.println("OK");
    } else {
      Serial.println("fail");
    }
    delay(3000);
    return;
  }

  client.poll();

  unsigned long now = millis();

  if (identified && now - lastStatusTx >= 30000) {
    lastStatusTx = now;
    sendFullStatus();
  }

  if (identified && enableMotorProtection && now - lastClientHbTime > 700) {
    static bool warned = false;
    if (!warned) {
      Serial.println("\nHBT_MOTOR TIMEOUT → STOP MOTORS");
      warned = true;
    }
    stopMotors();
    enableMotorProtection = false;
  } else if (enableMotorProtection && now - lastClientHbTime <= 700) {
    static unsigned long lastPrint = 0;
    if (now - lastPrint > 299) {
      Serial.print("HBT_MOTOR ");
      lastPrint = now;
    }
  }
}