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

  // Плавная радуга через HSV
  uint32_t color = strip.ColorHSV(hue, 255, 255);   // hue 0..65535
  strip.setPixelColor(0, color);
  strip.show();

  hue += 280;             // скорость: 150–450 — хороший диапазон
  if (hue >= 65536) hue = 0;

  delay(20);              // 15–35 мс — плавно и красиво
}