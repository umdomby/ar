#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>

// WiFi
//const char* ssid = "Mate20Pro";
const char* ssid = "Robolab124";
const char* password = "wifi123123123";

// Signaling server
const char* serverIP = "213.184.249.66";
const int serverPort = 5000;
const String myId = "esp32";
const String peerId = "winapp";

// STUN Google
const char* stunHost = "stun.l.google.com";
const int stunPort = 19302;

// UDP
WiFiUDP udp;
int udpPort = 8474;  // начальное значение, потом переопределим

// Публичный адрес и reflected порт
String publicAddr = "";
int reflectedPort = 0;

// RGB пины
#define RED_PIN   18
#define GREEN_PIN 19
#define BLUE_PIN  20

WebSocketsClient wsClient;

// Отправка candidate
void sendCandidate(const String& addr) {
  DynamicJsonDocument doc(512);
  doc["type"] = "candidate";
  doc["data"] = addr;
  doc["from"] = myId;
  doc["to"] = peerId;
  String json;
  serializeJson(doc, json);
  wsClient.sendTXT(json);
  Serial.println("Sent candidate: " + addr);
}

void setRGB(bool on) {
  digitalWrite(RED_PIN, on ? HIGH : LOW);
  digitalWrite(GREEN_PIN, on ? HIGH : LOW);
  digitalWrite(BLUE_PIN, on ? HIGH : LOW);
  Serial.printf("RGB %s\n", on ? "ON" : "OFF");
}

void getPublicAddr() {
  WiFiUDP stunUdp;
  stunUdp.begin(udpPort + 1);

  IPAddress stunIP;
  if (!WiFi.hostByName(stunHost, stunIP)) {
    Serial.println("DNS lookup failed for STUN");
    return;
  }

  uint8_t req[20] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xa4, 0x42};
  randomSeed(millis());
  for (int i = 8; i < 20; i++) req[i] = random(256);

  stunUdp.beginPacket(stunIP, stunPort);
  stunUdp.write(req, 20);
  stunUdp.endPacket();

  unsigned long start = millis();
  while (millis() - start < 3000) {
    if (stunUdp.parsePacket()) {
      uint8_t resp[64];
      int len = stunUdp.read(resp, 64);
      if (len >= 28 && resp[0] == 0x01 && resp[1] == 0x01) {
        reflectedPort = (resp[22] << 8 | resp[23]) ^ 0x2112;
        IPAddress ip(resp[24] ^ 0x21, resp[25] ^ 0x12, resp[26] ^ 0xa4, resp[27] ^ 0x42);
        publicAddr = ip.toString() + ":" + String(reflectedPort);
        Serial.println("Public addr: " + publicAddr);

        // КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: слушаем UDP на reflected порту!
        udp.stop();
        udp.begin(reflectedPort);
        Serial.println("UDP now listening on reflected port: " + String(reflectedPort));
        return;
      }
    }
    delay(50);
  }
  Serial.println("STUN failed - using default port 5000");
  reflectedPort = udpPort;
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("Signaling connected");
      if (publicAddr != "") {
        sendCandidate(publicAddr);
        delay(3000);
        sendCandidate(publicAddr);
      }
      break;

    case WStype_TEXT: {
      String text = (char*)payload;
      DynamicJsonDocument doc(512);
      deserializeJson(doc, text);
      String msgType = doc["type"];
      if (msgType == "candidate") {
        String peer = doc["data"];
        Serial.println("WinApp candidate received: " + peer);

        if (peer != "unknown" && peer.indexOf(':') > 0) {
          IPAddress peerIP;
          peerIP.fromString(peer.substring(0, peer.indexOf(':')));
          int peerPort = peer.substring(peer.indexOf(':') + 1).toInt();

          udp.beginPacket(peerIP, peerPort);
          udp.write((uint8_t)0);
          udp.endPacket();
          Serial.println("Hole punching: sent packet to WinApp " + peer);
        }
      }
      break;
    }

    case WStype_DISCONNECTED:
      Serial.println("Signaling disconnected");
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  setRGB(false);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  udp.begin(udpPort);  // временно, потом переопределим
  getPublicAddr();

  wsClient.begin(serverIP, serverPort, "/ws?id=" + myId);
  wsClient.onEvent(webSocketEvent);
  wsClient.setReconnectInterval(3000);
}

void loop() {
  wsClient.loop();

  int packetSize = udp.parsePacket();
  if (packetSize) {
    Serial.println("UDP packet received! Size: " + String(packetSize));
    char buf[16];
    int len = udp.read(buf, 15);
    if (len > 0) {
      buf[len] = 0;
      String cmd = String(buf);
      Serial.println("Command received: " + cmd + " from " + udp.remoteIP().toString() + ":" + udp.remotePort());
      if (cmd == "on") setRGB(true);
      else if (cmd == "off") setRGB(false);
    } else {
      Serial.println("Empty packet received from " + udp.remoteIP().toString() + ":" + udp.remotePort());
    }
  }
}