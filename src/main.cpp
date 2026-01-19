#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define BRIGHTNESS 30     // 20–60 обычно комфортно, 255 = очень ярко
#define PIN_LED    48    // 38 - если новая версия платы
#define NUM_LEDS   1

Adafruit_NeoPixel strip(NUM_LEDS, PIN_LED, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nRainbow ESP32-S3-DevKitC-1");

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();          // сразу выключаем
}

void loop() {
  static uint16_t hue = 0;

  uint32_t rgb = strip.ColorHSV(hue, 255, 255);
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >> 8) & 0xFF;
  uint8_t b = rgb & 0xFF;

  // Цветное название для самых основных цветов (примерно)
  const char* color_name = "???";
  if (hue <  4000)  color_name = "Красный";
  else if (hue < 12000) color_name = "Оранжевый";
  else if (hue < 20000) color_name = "Жёлтый";
  else if (hue < 30000) color_name = "Салатовый";
  else if (hue < 38000) color_name = "Зелёный";
  else if (hue < 46000) color_name = "Бирюзовый";
  else if (hue < 54000) color_name = "Голубой";
  else if (hue < 62000) color_name = "Синий";
  else if (hue < 65536) color_name = "Фиолетовый";

  Serial.printf("[%5u] %-10s  #%02X%02X%02X  (%3d,%3d,%3d)\n",
                hue, color_name, r, g, b, r, g, b);

  strip.setPixelColor(0, rgb);
  strip.show();

  hue += 280;
  if (hue >= 65536) hue -= 65536;

  delay(30);
}