#pragma once

#include "Config.h"
#include <stddef.h>
#include <esp_err.h>

#if DUSTMOVER_USE_SD
#include <SPI.h>
#else
#include <driver/spi_master.h>
#endif

class Icm42688 {
 public:
  static constexpr uint8_t expectedId = 0x47;  // ICM-42688-P

  struct RawSample {
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
  };

  enum class ReadResult { ready, pending, invalid };

  Icm42688(int chipSelect, uint32_t clockHz);
  esp_err_t begin(int sck, int miso, int mosi);
  bool whoAmI(uint8_t &id);
  bool configure(bool useInt2);
  ReadResult readSample(RawSample &sample);

 private:
  void startTransfer();
  void endTransfer();
  bool exchange(const uint8_t *tx, uint8_t *rx, size_t length);
  bool readRegister(uint8_t reg, uint8_t &value);
  bool writeRegister(uint8_t reg, uint8_t value);
  bool writeChecked(uint8_t reg, uint8_t value);
  bool readSelectedRegister(uint8_t reg, uint8_t *data, size_t length);

  const int cs;
#if DUSTMOVER_USE_SD
  const SPISettings settings;
#else
  const uint32_t frequencyHz;
  spi_device_handle_t device = nullptr;
#endif
};
