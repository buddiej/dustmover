#pragma once
#include <stdint.h>

#ifndef DUSTMOVER_USE_SD
#define DUSTMOVER_USE_SD 0
#endif

// Classic ESP32 DevKit / WROOM-32. Verify pins for your actual board.
namespace config {
constexpr int spiSck = 18;
constexpr int spiMiso = 19;
constexpr int spiMosi = 23;
constexpr int sensorCs = 5;
constexpr int dataReady = 27;
constexpr bool useDataReadyInterrupt = true;  // Connect ICM-42688-P INT2 to dataReady.
constexpr uint32_t sensorSpiHz = 1000000;
constexpr uint32_t serialBaud = 230400;
// Held high in every build so an attached SD reader stays deselected.
constexpr int sdCs = 26;
static_assert(sensorCs != sdCs, "The SPI sensor and SD reader need separate CS pins.");
#if DUSTMOVER_USE_SD
constexpr uint32_t sdSpiHz = 4000000;
constexpr uint32_t queueLength = 512;
constexpr uint32_t flushPeriodMs = 1000;
#endif
}
