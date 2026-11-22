#include <Arduino.h>

constexpr int ledPin = 16;
constexpr unsigned long halfPeriodMs = 500;

void setup() {
  digitalWrite(ledPin, LOW);
  pinMode(ledPin, OUTPUT);
}

void loop() {
  digitalWrite(ledPin, HIGH);
  delay(halfPeriodMs);
  digitalWrite(ledPin, LOW);
  delay(halfPeriodMs);
}
