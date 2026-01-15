#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>  // Links2004
#include <ServoEasing.hpp>

// Пины для ESP32-S3 BTS7960 — оставляем только один мотор (A)
#define PIN_ENA     18
#define PIN_IN1     19
#define PIN_IN2     20
#define PIN_RELAY   16      // active LOW
#define PIN_SERVO2  14      // оставляем только второе серво
#define PIN_VOLTAGE 4

// Настройки
const char* ssid       = "Robolab124";
const char* password   = "wifi123123123";
const char* ws_host    = "a.ardu.live";
const uint16_t ws_port = 444;
const char* ws_path    = "/wsar";
const char* DEVICE_ID  = "9999999999999999";  // 16 символов

// PWM настройки
#define PWM_FREQ    25000
#define PWM_RES     8       // 0..255

#define MOTOR_A_CHANNEL 4

ServoEasing Servo2;           // ← только одно серво
WebSocketsClient client;

bool identified = false;
unsigned long lastStatusTx = 0;
unsigned long lastClientHbTime = 0;
bool enableMotorProtection = false;

unsigned long now = millis();
unsigned long nowcmd = millis();

// Типы сообщений (без изменений)
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
  if (client.isConnected()) {
    client.sendBIN(data, len);
  }
}

void sendFullStatus() {
  uint8_t buf[9] = {0};
  buf[0] = RSP_FULL_STATUS;
  buf[1] = (digitalRead(PIN_RELAY) == LOW) ? 1 : 0;
  buf[2] = Servo2.read();       // только одно серво
  buf[3] = 0;                   // раньше было Servo2, теперь зарезервировано / пусто

  int raw = analogRead(PIN_VOLTAGE);
  buf[4] = highByte(raw);
  buf[5] = lowByte(raw);

  buf[6] = 0;
  buf[7] = 0;
  buf[8] = (uint8_t)constrain(WiFi.RSSI(), -128, 127);

  sendBinary(buf, sizeof(buf));
}

void stopMotors() {
  ledcWrite(MOTOR_A_CHANNEL, 0);
}

// ────────────────────────────────────────────────────────────────
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Disconnected");
      identified = false;
      stopMotors();
      enableMotorProtection = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WS] Connected to url: %s\n", payload);

      // Тип клиента
      {
        uint8_t buf[2] = {CMD_CLIENT_TYPE, 2};
        sendBinary(buf, 2);
      }

      // Идентификация
      {
        uint8_t buf[17];
        buf[0] = CMD_IDENTIFY;
        memcpy(buf + 1, DEVICE_ID, 16);
        sendBinary(buf, 17);
      }

      identified = true;
      break;

    case WStype_BIN:
      if (length == 0) return;
      nowcmd = millis();
      uint8_t cmd = payload[0];
      Serial.printf("Получено бинарное: cmd=0x%02X, len=%d  ", cmd, length);

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
          if (length < 5) { Serial.println("→ CMD_MOTOR: слишком короткое"); break; }
          {
            char motor = payload[1];
            uint8_t speed = payload[2];
            uint8_t dir = payload[3];

            // Принимаем только мотор A
            if (motor != 'A') {
              Serial.println("→ Игнорируем мотор B — он отключен в прошивке");
              break;
            }

            Serial.printf("→ MOTOR A: speed=%d, dir=%d\n", speed, dir);

            if (dir == 1) {
              digitalWrite(PIN_IN1, HIGH);
              digitalWrite(PIN_IN2, LOW);
            } else if (dir == 2) {
              digitalWrite(PIN_IN1, LOW);
              digitalWrite(PIN_IN2, HIGH);
            } else {
              digitalWrite(PIN_IN1, LOW);
              digitalWrite(PIN_IN2, LOW);
            }

            ledcWrite(MOTOR_A_CHANNEL, speed);

            lastClientHbTime = millis();
            enableMotorProtection = true;
          }
          break;

        case CMD_SERVO_ABS:
          if (length < 4) { Serial.println("→ CMD_SERVO_ABS: слишком короткое"); break; }
          {
            uint8_t num = payload[1];
            uint8_t value = payload[2];

            // Принимаем только серво 2
            if (num == 2) {
                int angle = map(value, 0, 180, 0, 100);
                angle = constrain(angle, 0, 100);
                Serial.printf("SERVO 2: %d → %d°\n", value, angle);
                Servo2.write(angle);
            } else {
                Serial.println("Игнор серво ≠2");
}
          }
          break;

        case CMD_RELAY:
          if (length < 3) { Serial.println("→ CMD_RELAY: слишком короткое"); break; }
          {
            uint8_t state = payload[1];
            Serial.printf("→ RELAY state=%d\n", state);
            digitalWrite(PIN_RELAY, state ? LOW : HIGH);
          }
          break;

        case CMD_ALARM:
          if (length < 2) { Serial.println("→ CMD_ALARM: слишком короткое"); break; }
          Serial.printf("→ ALARM state=%d\n", payload[1]);
          break;

        default:
          Serial.printf("→ НЕИЗВЕСТНАЯ КОМАНДА 0x%02X\n", cmd);
      }
      break;
  }
}

// ────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Binary Protocol 2026 - ESP32-S3 - One Motor + One Servo ===\n");

  // Только одно серво
  Servo2.attach(PIN_SERVO2, 50);
  Servo2.write(50);
  delay(50);

  // Мотор A
  pinMode(PIN_ENA, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH);

  stopMotors();

  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);

  // LEDC только для одного мотора
  ledcSetup(MOTOR_A_CHANNEL, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENA, MOTOR_A_CHANNEL);
  ledcWrite(MOTOR_A_CHANNEL, 0);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

  client.beginSSL(ws_host, ws_port, ws_path);
  client.onEvent(webSocketEvent);
  client.setReconnectInterval(3000);
}

void loop() {
  client.loop();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost → reconnect");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  now = millis();

  if (identified && now - lastStatusTx >= 5000 && now - nowcmd >= 2000) {
    lastStatusTx = now;
    sendFullStatus();
  }

  if (identified && enableMotorProtection && now - lastClientHbTime > 700) {
    static bool warned = false;
    if (!warned) {
      Serial.println("\nHBT_MOTOR TIMEOUT → STOP MOTOR");
      warned = true;
    }
    stopMotors();
    enableMotorProtection = false;
  }
}