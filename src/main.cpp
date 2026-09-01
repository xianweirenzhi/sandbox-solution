#include <Arduino.h>

// HUZZAH ESP8266: 板载红色 LED 接在 GPIO0，低电平点亮
const int LED_PIN = 0;
const unsigned long BLINK_INTERVAL = 500;  // 闪烁间隔 ms

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // 低电平点亮，先拉高熄灭

  Serial.begin(115200);
  delay(1000);  // 等待串口就绪
  Serial.println();
  Serial.println("=== ESP8266 HUZZAH 开发板测试 ===");
}

void loop() {
  digitalWrite(LED_PIN, LOW);   // 点亮
  Serial.println("[LED] ON");
  delay(BLINK_INTERVAL);

  digitalWrite(LED_PIN, HIGH);  // 熄灭
  Serial.println("[LED] OFF");
  delay(BLINK_INTERVAL);
}
