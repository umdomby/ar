// ======================================================================
//  ESP32 WebSocket сервер с управлением моторами
//  Статический IP: 192.168.1.201
//  Подключение:    ws://192.168.1.201/ws
//  Дата:           март 2026
// ======================================================================

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>

// ─── Wi-Fi настройки ────────────────────────────────────────────────
const char* ssid     = "Robolab124";
const char* password = "wifi123123123";

// ─── Статический IP ─────────────────────────────────────────────────
IPAddress local_IP(192, 168, 1, 201);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);    // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4);

// ─── Пины для моторов (BTS7960 / аналог) ────────────────────────────
#define PIN_ENA   4     // PWM скорость A
#define PIN_IN1   15    // направление A1
#define PIN_IN2   16    // направление A2
#define PIN_ENB   5     // PWM скорость B
#define PIN_IN3   17    // направление B1
#define PIN_IN4   18    // направление B2

// ─── PWM параметры ──────────────────────────────────────────────────
#define PWM_FREQ    25000
#define PWM_RES     8
#define PWM_CH_A    0
#define PWM_CH_B    1

// ─── WebSocket путь ─────────────────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ─── Таймаут безопасности моторов ──────────────────────────────────
unsigned long lastMotorCommandTime = 0;
const unsigned long MOTOR_TIMEOUT_MS = 500;     // стоп, если нет команд > 800 мс


// Текущие скорости моторов (обновляются при командах и торможении)
uint8_t currentSpeedA = 0;
uint8_t currentSpeedB = 0;

// Флаг плавного торможения
bool isBraking = false;

// Время начала торможения и шаг уменьшения
unsigned long brakingLastUpdate = 0;
const uint8_t BRAKE_STEP = 5;         // шаг уменьшения скорости (5 единиц)
const unsigned long BRAKE_INTERVAL_MS = 20;  // интервал обновления (каждые 20 мс)

// =====================================================================
//  Обработчик WebSocket событий
// =====================================================================
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client,
               AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  switch (type)
  {
    case WS_EVT_CONNECT:
      Serial.printf("[WS] Клиент #%u подключился  IP: %s\n", 
                    client->id(), client->remoteIP().toString().c_str());
      client->text("ESP32 WebSocket ready");
      break;

    case WS_EVT_DISCONNECT:
      Serial.printf("[WS] Клиент #%u отключился\n", client->id());
      break;

    case WS_EVT_DATA:
    {
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      if (!info->final || info->index != 0 || info->len != len) {
        return; // поддерживаем только цельные кадры
      }

      if (info->opcode == WS_BINARY && len >= 1)
      {
        lastMotorCommandTime = millis();

        uint8_t cmd = data[0];

        if (cmd == 0x20 && len >= 4)          // CMD_MOTOR
        {
            uint8_t motor_char = data[1];       // 'A'=65, 'B'=66
            uint8_t speed = data[2];            // 0..255
            uint8_t dir   = data[3];            // 0=стоп, 1=вперёд, 2=назад

            bool isA = (motor_char == 'A' || motor_char == 65);
            bool isB = (motor_char == 'B' || motor_char == 66);

            if (isA || isB)
            {
                uint8_t ch   = isA ? PWM_CH_A : PWM_CH_B;
                uint8_t pin1 = isA ? PIN_IN1 : PIN_IN3;
                uint8_t pin2 = isA ? PIN_IN2 : PIN_IN4;

                // Устанавливаем направление
                digitalWrite(pin1, (dir == 1) ? HIGH : LOW);
                digitalWrite(pin2, (dir == 2) ? HIGH : LOW);

                // Скорость (0 при стопе)
                uint8_t effectiveSpeed = (dir == 0) ? 0 : speed;
                ledcWrite(ch, effectiveSpeed);

                // Обновляем текущую скорость
                if (isA) currentSpeedA = effectiveSpeed;
                else currentSpeedB = effectiveSpeed;

                Serial.printf("[MOTOR] %c  speed=%3d  dir=%d\n", 
                              isA ? 'A' : 'B', speed, dir);
            }
        }
        else if (cmd == 0x11)                 // CMD_HBT_MOTOR (heartbeat)
        {
          lastMotorCommandTime = millis();
          // Serial.println("[WS] HBT_MOTOR received");
        }
        else
        {
          Serial.printf("[WS] Неизвестная команда 0x%02X  len=%d\n", cmd, len);
        }
      }
      break;
    }

    default:
      break;
  }
}

// =====================================================================
//  Остановить оба мотора
// =====================================================================
void stopMotors()
{
  ledcWrite(PWM_CH_A, 0);
  ledcWrite(PWM_CH_B, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}

// =====================================================================
//  SETUP
// =====================================================================
void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 WebSocket Motor Control  192.168.1.201 ===\n");

  // Настройка статического IP
  WiFi.mode(WIFI_STA);
  
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Ошибка настройки статического IP!");
  } else {
    Serial.println("Статический IP задан → 192.168.1.201");
  }

  WiFi.begin(ssid, password);
  Serial.print("Подключение к WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\nПодключено!");
  Serial.print("IP адрес: ");   Serial.println(WiFi.localIP());
  Serial.print("MAC: ");        Serial.println(WiFi.macAddress());

  // Настройка пинов моторов
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  // PWM каналы
  ledcSetup(PWM_CH_A, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(PIN_ENA, PWM_CH_A);
  ledcAttachPin(PIN_ENB, PWM_CH_B);

  stopMotors();

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Простая страница для проверки в браузере
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String msg = "WebSocket сервер работает\n";
    msg += "Подключайтесь: ws://192.168.1.201/ws\n";
    msg += "Время: " + String(millis() / 1000) + " сек";
    request->send(200, "text/plain", msg);
  });

  server.begin();

  Serial.println("Готово → ws://192.168.1.201/ws");
}

// =====================================================================
//  LOOP
// =====================================================================
void loop()
{
    ws.cleanupClients();

    unsigned long now = millis();

    // Проверяем таймаут
    if (now - lastMotorCommandTime > MOTOR_TIMEOUT_MS)
    {
        // Если команды не приходят >500 мс, начинаем или продолжаем торможение
        if (!isBraking && (currentSpeedA > 0 || currentSpeedB > 0))
        {
            Serial.println("TIMEOUT → starting smooth braking");
            isBraking = true;
            brakingLastUpdate = now;
        }

        // Плавное торможение, если флаг активен
        if (isBraking)
        {
            if (now - brakingLastUpdate >= BRAKE_INTERVAL_MS)
            {
                brakingLastUpdate = now;

                // Уменьшаем скорости на шаг
                if (currentSpeedA > 0)
                {
                    currentSpeedA = (currentSpeedA > BRAKE_STEP) ? currentSpeedA - BRAKE_STEP : 0;
                    ledcWrite(PWM_CH_A, currentSpeedA);
                }
                

                if (currentSpeedB > 0)
                {
                    currentSpeedB = (currentSpeedB > BRAKE_STEP) ? currentSpeedB - BRAKE_STEP : 0;
                    ledcWrite(PWM_CH_B, currentSpeedB);
                }

                // Если обе скорости =0, завершаем торможение
                if (currentSpeedA == 0 && currentSpeedB == 0)
                {
                    Serial.println("Braking complete → motors stopped");
                    stopMotors();  // финальная очистка пинов
                    isBraking = false;
                }
            }
        }
    }
    else
    {
        // Если команды приходят (lastMotorCommandTime свежий), сбрасываем торможение
        if (isBraking)
        {
            Serial.println("New command received → interrupting braking");
            isBraking = false;
        }
    }

    delay(4);
}