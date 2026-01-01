#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "Robolab134";
const char* password = "wifi123123123";

// Публичный IP твоего сервера (VPS или домашний с пробросом порта)
const char* serverIP = "213.184.249.66";
const int serverPort = 8080;

// Статический IP для ESP32 в локальной сети
IPAddress local_IP(192, 168, 1, 150);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Настраиваем статический IP ДО подключения к WiFi
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Ошибка: не удалось настроить статический IP!");
  }

  Serial.print("Подключение к WiFi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  // Таймаут подключения 20 сек
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nНе удалось подключиться к WiFi!");
    return;
  }

  Serial.println("\nWiFi подключён!");
  Serial.print("Локальный IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("MAC-адрес: ");
  Serial.println(WiFi.macAddress());

  // Запускаем UDP (локальный порт не важен, можно 0)
  udp.begin(0);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi потерян. Переподключение...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  // Формируем сообщение
  String localIP = WiFi.localIP().toString();
  String message = "REGISTER:" + localIP;

  // Отправляем UDP-пакет
  udp.beginPacket(serverIP, serverPort);
  udp.print(message);
  udp.endPacket();

  Serial.print("Отправлено UDP на ");
  Serial.print(serverIP);
  Serial.print(": ");
  Serial.println(message);

  // Ждём ответ максимум 3 секунды
  unsigned long startTime = millis();
  bool received = false;

  while (millis() - startTime < 3000) {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      char incomingPacket[64];
      int len = udp.read(incomingPacket, sizeof(incomingPacket) - 1);
      if (len > 0) {
        incomingPacket[len] = '\0';  // Завершаем строку
      }

      String response = String(incomingPacket);
      Serial.print("Получен ответ: ");
      Serial.println(response);

      if (response == "OK" || response.startsWith("OK")) {
        Serial.println("Подтверждение получено! Регистрация завершена.");
        // Можно мигнуть светодиодом или вывести сообщение
        while (true) {
          delay(10000);  // Бесконечный цикл — больше не шлём
        }
      }

      received = true;
      break;
    }
    delay(100);  // Не грузим процессор
  }

  if (!received) {
    Serial.println("Ответ не получен (таймаут). Повтор через 5 сек...");
  }

  delay(5000);  // Ждём 5 секунд перед следующей попыткой
}