#pragma once
#include <stdint.h>

// Classic ESP32 DevKit / WROOM-32. Verify pins for your actual board.
namespace config {
constexpr int sda = 21;
constexpr int scl = 22;
constexpr int sdCs = 5;
constexpr int sdSck = 18;
constexpr int sdMiso = 19;
constexpr int sdMosi = 23;
constexpr uint8_t sensorAddress = 0x53;
constexpr uint32_t samplePeriodUs = 10000;  // nominal 100 Hz polling
constexpr uint32_t queueLength = 512;
constexpr uint32_t flushPeriodMs = 1000;
}
