#include "Icm42688.h"

#include <Arduino.h>
#include <driver/gpio.h>

namespace {
// User bank 0, TDK InvenSense DS-000347.
constexpr uint8_t deviceConfig = 0x11;
constexpr uint8_t intConfig = 0x14;
constexpr uint8_t accelDataX1 = 0x1f;
constexpr uint8_t intStatus = 0x2d;
constexpr uint8_t intfConfig0 = 0x4c;
constexpr uint8_t powerMgmt0 = 0x4e;
constexpr uint8_t gyroConfig0 = 0x4f;
constexpr uint8_t accelConfig0 = 0x50;
constexpr uint8_t intConfig1 = 0x64;
constexpr uint8_t intSource3 = 0x68;
constexpr uint8_t whoAmIReg = 0x75;
constexpr uint8_t bankSelect = 0x76;

constexpr uint8_t softReset = 0x01;
constexpr uint8_t resetDone = 0x10;
constexpr uint8_t dataReady = 0x08;
constexpr uint8_t int2ActiveHighPushPull = 0x18;
constexpr uint8_t intAsyncReset = 0x10;
constexpr uint8_t odr100Hz = 0x08;
constexpr uint8_t accel2g = 0x60;
constexpr uint8_t gyro250Dps = 0x60;
constexpr uint8_t bothLowNoise = 0x0f;
constexpr uint8_t disableI2c = 0x03;
constexpr size_t sampleBytes = 12;

int16_t decodeAxis(const uint8_t *bytes) {
  const uint16_t bits = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  return static_cast<int16_t>(bits);
}
}

Icm42688::Icm42688(int chipSelect, uint32_t clockHz)
    : cs(chipSelect),
#if DUSTMOVER_USE_SD
      settings(clockHz, MSBFIRST, SPI_MODE0) {}
#else
      frequencyHz(clockHz) {}
#endif

esp_err_t Icm42688::begin(int sck, int miso, int mosi) {
  // Preload idle levels before enabling the outputs.
  gpio_set_level(static_cast<gpio_num_t>(cs), HIGH);
  gpio_set_level(static_cast<gpio_num_t>(sck), LOW);
  gpio_set_level(static_cast<gpio_num_t>(mosi), LOW);
  gpio_config_t outputs{};
  outputs.pin_bit_mask = (1ULL << cs) | (1ULL << sck) | (1ULL << mosi);
  outputs.mode = GPIO_MODE_OUTPUT;
  esp_err_t error = gpio_config(&outputs);
  if (error != ESP_OK) {
    return error;
  }
  gpio_config_t input{};
  input.pin_bit_mask = 1ULL << miso;
  input.mode = GPIO_MODE_INPUT;
  error = gpio_config(&input);
  if (error != ESP_OK) {
    return error;
  }

#if DUSTMOVER_USE_SD
  // Arduino SD and the sensor share Arduino's SPI bus lock in this build.
  SPI.begin(sck, miso, mosi, config::sdCs);
  return SPI.bus() ? ESP_OK : ESP_FAIL;
#else
  spi_bus_config_t bus{};
  bus.sclk_io_num = sck;
  bus.miso_io_num = miso;
  bus.mosi_io_num = mosi;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  error = spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_DISABLED);
  if (error != ESP_OK) {
    return error;
  }

  spi_device_interface_config_t imu{};
  imu.clock_speed_hz = frequencyHz;
  imu.mode = 0;
  imu.spics_io_num = cs;
  imu.queue_size = 1;
  error = spi_bus_add_device(SPI3_HOST, &imu, &device);
  if (error != ESP_OK) {
    spi_bus_free(SPI3_HOST);
  }
  return error;
#endif
}

void Icm42688::startTransfer() {
#if DUSTMOVER_USE_SD
  SPI.beginTransaction(settings);
#endif
}

void Icm42688::endTransfer() {
#if DUSTMOVER_USE_SD
  SPI.endTransaction();
#endif
}

bool Icm42688::exchange(const uint8_t *tx, uint8_t *rx, size_t length) {
#if DUSTMOVER_USE_SD
  digitalWrite(cs, LOW);
  for (size_t i = 0; i < length; ++i) {
    const uint8_t value = SPI.transfer(tx[i]);
    if (rx) {
      rx[i] = value;
    }
  }
  digitalWrite(cs, HIGH);
  return true;
#else
  spi_transaction_t transaction{};
  transaction.length = length * 8;
  transaction.tx_buffer = tx;
  transaction.rx_buffer = rx;
  return spi_device_polling_transmit(device, &transaction) == ESP_OK;
#endif
}

bool Icm42688::readSelectedRegister(uint8_t reg, uint8_t *data, size_t length) {
  uint8_t tx[1 + sampleBytes]{};
  uint8_t rx[1 + sampleBytes];
  tx[0] = reg | 0x80;
  if (length > sampleBytes || !exchange(tx, rx, length + 1)) {
    return false;
  }
  memcpy(data, rx + 1, length);
  return true;
}

bool Icm42688::readRegister(uint8_t reg, uint8_t &value) {
  startTransfer();
  const bool ok = readSelectedRegister(reg, &value, 1);
  endTransfer();
  return ok;
}

bool Icm42688::writeRegister(uint8_t reg, uint8_t value) {
  const uint8_t tx[] = {reg, value};
  startTransfer();
  const bool ok = exchange(tx, nullptr, sizeof(tx));
  endTransfer();
  return ok;
}

bool Icm42688::writeChecked(uint8_t reg, uint8_t value) {
  uint8_t readback;
  return writeRegister(reg, value) && readRegister(reg, readback) && readback == value;
}

bool Icm42688::whoAmI(uint8_t &id) {
  return writeRegister(bankSelect, 0) && readRegister(whoAmIReg, id);
}

bool Icm42688::configure(bool useInt2) {
  if (!writeRegister(deviceConfig, softReset)) {
    return false;
  }
  delay(2);
  uint8_t id, status;
  if (!readRegister(whoAmIReg, id) || id != expectedId ||
      !readRegister(intStatus, status) || (status & resetDone) == 0) {
    return false;
  }

  // Keep big-endian sensor output and leave the FIFO in bypass (reset default).
  uint8_t interface;
  if (!readRegister(intfConfig0, interface) ||
      !writeChecked(intfConfig0, interface | disableI2c) ||
      !writeChecked(accelConfig0, accel2g | odr100Hz) ||
      !writeChecked(gyroConfig0, gyro250Dps | odr100Hz)) {
    return false;
  }
  uint8_t intSettings;
  if (useInt2 &&
      (!writeChecked(intConfig, int2ActiveHighPushPull) ||
       !readRegister(intConfig1, intSettings) ||
       !writeChecked(intConfig1, intSettings & ~intAsyncReset) ||
       !writeChecked(intSource3, dataReady))) {
    return false;
  }

  if (!writeRegister(powerMgmt0, bothLowNoise)) {
    return false;
  }
  delay(50);
  uint8_t power;
  return readRegister(powerMgmt0, power) && power == bothLowNoise;
}

Icm42688::ReadResult Icm42688::readSample(RawSample &sample) {
  uint8_t status = 0;
  uint8_t data[sampleBytes];
  // Hold Arduino's bus lock across both reads in the shared SD build.
  startTransfer();
  const bool validStatus = readSelectedRegister(intStatus, &status, 1) &&
                           (status & (0x80 | resetDone)) == 0;
  const bool ready = validStatus && (status & dataReady) != 0;
  bool validData = true;
  if (ready) {
    validData = readSelectedRegister(accelDataX1, data, sizeof(data));
  }
  endTransfer();
  if (!validStatus || !validData) {
    return ReadResult::invalid;
  }
  if (!ready) {
    return ReadResult::pending;
  }

  sample = {decodeAxis(data), decodeAxis(data + 2), decodeAxis(data + 4),
            decodeAxis(data + 6), decodeAxis(data + 8), decodeAxis(data + 10)};
  if (sample.ax == INT16_MIN || sample.ay == INT16_MIN || sample.az == INT16_MIN ||
      sample.gx == INT16_MIN || sample.gy == INT16_MIN || sample.gz == INT16_MIN) {
    return ReadResult::invalid;
  }
  return ReadResult::ready;
}
