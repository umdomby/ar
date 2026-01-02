#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "Robolab124";
const char* password = "wifi123123123";

const char* serverIP = "213.184.249.66";
const int serverPort = 5000;

// Уникальное имя для каждой ESP32
const char* deviceName = "ESP32-LivingRoom";  // ← поменяй на каждой плате!

WiFiUDP udp;
unsigned int localUdpPort = 12345;  // фиксированный порт для P2P

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);  // встроенный светодиод (или подключи RGB)

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  udp.begin(localUdpPort);
}

void loop() {
  // Регистрация каждые 10 сек
  String msg = String("REGISTER:") + deviceName + ":" + WiFi.localIP().toString();
  udp.beginPacket(serverIP, serverPort);
  udp.write((const uint8_t*)msg.c_str(), msg.length());
  udp.endPacket();

  // Получаем команды от Windows
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incoming[32];
    int len = udp.read(incoming, sizeof(incoming));
    incoming[len] = 0;
    String cmd = String(incoming);

    if (cmd.startsWith("COLOR:")) {
      // Формат: COLOR:#FF00AA
      String hex = cmd.substring(6);
      long color = strtol(hex.c_str(), NULL, 16);
      int r = (color >> 16) & 0xFF;
      int g = (color >> 8) & 0xFF;
      int b = color & 0xFF;

      // Для встроенного LED просто включаем/выключаем
      digitalWrite(LED_BUILTIN, HIGH);  // или используй analogWrite для RGB
      Serial.printf("Set color: #%s -> R%d G%d B%d\n", hex.c_str(), r, g, b);
    }
  }

  delay(10000);  // регистрация каждые 10 сек
}