#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebsockets.h>
#include <ServoEasing.hpp>

using namespace websockets;

// Пины
#define PIN_ENA     D1
#define PIN_IN1     D2
#define PIN_IN2     D3
#define PIN_IN3     D4
#define PIN_IN4     D5
#define PIN_ENB     D6
#define PIN_RELAY   D0      // active LOW
#define PIN_SERVO1  D7
#define PIN_SERVO2  D8
#define PIN_VOLTAGE A0

// Настройки
const char* ssid       = "Robolab124";
const char* password   = "wifi123123123";
const char* ws_url     = "wss://a.ardu.live:444/wsar";
const char* DEVICE_ID  = "9999999999999999";  // 16 символов

ServoEasing Servo1, Servo2;
WebsocketsClient client;

bool identified = false;
unsigned long lastHbTx = 0;
unsigned long lastClientHbTime = 0;
unsigned long lastStatusTx = 0;
bool enableMotorProtection = false;

// Типы сообщений
#define CMD_IDENTIFY       0x01
#define CMD_CLIENT_TYPE    0x02
#define CMD_HEARTBEAT      0x10  // От ESP к серверу
#define CMD_HBT_MOTOR      0x11  // От клиента для защиты моторов
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

void sendHeartbeat() {
  uint8_t buf[1] = {CMD_HEARTBEAT};
  sendBinary(buf, 1);
}

void sendFullStatus() {
  uint8_t buf[9] = {0};
  buf[0] = RSP_FULL_STATUS;
  buf[1] = (digitalRead(PIN_RELAY) == LOW) ? 1 : 0;   // relay
  buf[2] = Servo1.read();                             // servo1
  buf[3] = Servo2.read();                             // servo2

  int raw = analogRead(PIN_VOLTAGE);
  buf[4] = highByte(raw);
  buf[5] = lowByte(raw);

  buf[6] = 0;  // alarm
  buf[7] = 0;  // motion
  buf[8] = (uint8_t)constrain(WiFi.RSSI(), -128, 127);  // rssi

  sendBinary(buf, sizeof(buf));
}

void sendAck(uint8_t cmd, uint8_t value = 0) {
  uint8_t buf[3] = {RSP_ACK, cmd, value};
  sendBinary(buf, 3);
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

  if (len == 0) {
    Serial.println("Получено пустое сообщение");
    return;
  }

  Serial.printf("Получено бинарное: cmd=0x%02X, len=%d  ", data[0]);

  switch (data[0]) {
    case CMD_HEARTBEAT:
      Serial.println("→ HEARTBEAT (от клиента, но используем только для сервера)");
      break;

    case CMD_HBT_MOTOR:
      Serial.print("HBT_MOTOR ");  // В линию без новой строки
      lastClientHbTime = millis();
      enableMotorProtection = true;
      break;

case CMD_MOTOR:
      if (len < 5) {
        Serial.println("→ CMD_MOTOR: слишком короткое");
        return;
      }
      {
        char motor = data[1];
        uint8_t speed = data[2];
        uint8_t dir = data[3];

        Serial.printf("→ MOTOR %c: speed=%d, dir=%d\n", motor, speed, dir);

        uint8_t pwmPin = (motor == 'A') ? PIN_ENA : PIN_ENB;
        uint8_t inPin1 = (motor == 'A') ? PIN_IN1 : PIN_IN3;
        uint8_t inPin2 = (motor == 'A') ? PIN_IN2 : PIN_IN4;

        // Устанавливаем скорость
        analogWrite(pwmPin, speed);

        // Управление направлением
        if (dir == 1) {         // forward
          digitalWrite(inPin1, HIGH);
          digitalWrite(inPin2, LOW);
        } else if (dir == 2) {  // backward
          digitalWrite(inPin1, LOW);
          digitalWrite(inPin2, HIGH);
        } else {                // dir == 0 → стоп (но PWM уже 0, просто сбрасываем направление)
          digitalWrite(inPin1, LOW);
          digitalWrite(inPin2, LOW);
        }

        // Включаем защиту по HBT_MOTOR — если пришла любая команда мотора
        lastClientHbTime = millis();
        enableMotorProtection = true;

        sendAck(CMD_MOTOR, speed);
      }
      break;

    case CMD_SERVO_ABS:
      if (len < 4) {
        Serial.println("→ CMD_SERVO_ABS: слишком короткое");
        return;
      }
      {
        uint8_t num = data[1];
        uint8_t angle = constrain(data[2], 0, 180);
        Serial.printf("→ SERVO %d → angle=%d\n", num, angle);

        if (num == 1) Servo1.write(angle);
        else if (num == 2) Servo2.write(angle);

        sendAck(CMD_SERVO_ABS, angle);
      }
      break;

    case CMD_RELAY:
      if (len < 3) {
        Serial.println("→ CMD_RELAY: слишком короткое");
        return;
      }
      {
        uint8_t state = data[1];
        Serial.printf("→ RELAY state=%d\n", state);
        digitalWrite(PIN_RELAY, state ? LOW : HIGH);
        sendAck(CMD_RELAY, state);
      }
      break;

    case CMD_ALARM:
      if (len < 2) {
        Serial.println("→ CMD_ALARM: слишком короткое");
        return;
      }
      Serial.printf("→ ALARM state=%d\n", data[1]);
      sendAck(CMD_ALARM, data[1]);
      break;

    default:
      Serial.printf("→ НЕИЗВЕСТНАЯ КОМАНДА 0x%02X\n", data[0]);
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
  switch (event) {
    case WebsocketsEvent::ConnectionOpened:
      Serial.println("[WS] Connected");
      identified = false;

      // Тип клиента
      {
        uint8_t buf[2] = {CMD_CLIENT_TYPE, 2}; // 2 = esp
        sendBinary(buf, 2);
      }

      // Идентификация
      {
        uint8_t buf[17];
        buf[0] = CMD_IDENTIFY;
        memcpy(buf + 1, DEVICE_ID, 16);
        sendBinary(buf, 17);
      }
      identified = true;  // Предполагаем успешную идентификацию после отправки
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
  Serial.println("\n=== Binary Protocol 2026 with HBT_MOTOR ===\n");

  Servo1.attach(PIN_SERVO1, 90);
  Servo2.attach(PIN_SERVO2, 90);
  Servo1.write(90);
  Servo2.write(90);

  pinMode(PIN_ENA,  OUTPUT);
  pinMode(PIN_ENB,  OUTPUT);
  pinMode(PIN_IN1,  OUTPUT);
  pinMode(PIN_IN2,  OUTPUT);
  pinMode(PIN_IN3,  OUTPUT);
  pinMode(PIN_IN4,  OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, HIGH); // выключено

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
  client.setInsecure();
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

  // Heartbeat от ESP к серверу каждые ~2.5 сек
  if (now - lastHbTx >= 2500) {
    lastHbTx = now;
    sendHeartbeat();
  }

  // Статус каждые ~800 мс
  if (identified && now - lastStatusTx >= 800) {
    lastStatusTx = now;
    sendFullStatus();
  }

  // Защита моторов по таймауту HBT_MOTOR
    if (identified && enableMotorProtection && now - lastClientHbTime > 700) {
        static bool warned = false;
        if (!warned) {
        Serial.println("\nHBT_MOTOR TIMEOUT → STOP MOTORS");
        warned = true;
        }
        stopMotors();
        enableMotorProtection = false;  // больше не ждём, пока клиент снова не активируется
    } else if (enableMotorProtection && now - lastClientHbTime <= 700) {
        // Выводим HBT_MOTOR в одну строку без переноса
        static unsigned long lastPrint = 0;
        if (now - lastPrint > 299) {  // примерно каждые 300 мс
        Serial.print("HBT_MOTOR ");
        lastPrint = now;
        }
    }
}