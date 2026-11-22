#include <Arduino.h>
#include <atomic>
#include <driver/gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Config.h"
#include "Icm42688.h"

#if DUSTMOVER_USE_SD
#include <SD.h>
#include <SPI.h>
#include <errno.h>
#include <fcntl.h>
#include <freertos/queue.h>
#include <unistd.h>
#endif

struct Sample {
  int64_t timeUs;
  uint32_t sequence;
  uint32_t dataReadyEvents;
  uint32_t coalesced;
  uint32_t queueDrops;
  uint32_t readErrors;
  Icm42688::RawSample raw;
};

struct RuntimeStats {
  std::atomic<uint32_t> sequence{0};
  std::atomic<uint32_t> coalesced{0};
  std::atomic<uint32_t> readErrors{0};
  std::atomic<uint32_t> sampled{0};
#if DUSTMOVER_USE_SD
  std::atomic<uint32_t> written{0};
  std::atomic<uint32_t> queueDrops{0};
  std::atomic<uint32_t> sdErrors{0};
#endif
};

static constexpr char csvHeader[] =
    "t_us,sequence,data_ready_events,coalesced,queue_drops,read_errors,"
    "x_raw,y_raw,z_raw,gx_raw,gy_raw,gz_raw\n";
static constexpr size_t csvLineSize = 160;

static Icm42688 sensor(config::sensorCs, config::sensorSpiHz);
static TaskHandle_t samplerHandle = nullptr;
static RuntimeStats stats;
static bool sampling = false;

#if DUSTMOVER_USE_SD
static constexpr char sdMountPoint[] = "/sd";
static QueueHandle_t sampleQueue = nullptr;
static TaskHandle_t loggerHandle = nullptr;
static std::atomic<bool> logging{false};
static int logFd = -1;

static bool writeBlock(const char *data, size_t length);
static void closeSession();
static bool openSession();
static void loggerTask(void *);
static bool startLogger();
#else
static void probeMiso();
#endif

static void IRAM_ATTR dataReadyISR() {
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(samplerHandle, &higherPriorityTaskWoken);
  if (higherPriorityTaskWoken == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

static int formatSample(char *line, const Sample &sample) {
  return snprintf(line, csvLineSize, "%lld,%lu,%lu,%lu,%lu,%lu,%d,%d,%d,%d,%d,%d\n",
                  static_cast<long long>(sample.timeUs),
                  static_cast<unsigned long>(sample.sequence),
                  static_cast<unsigned long>(sample.dataReadyEvents),
                  static_cast<unsigned long>(sample.coalesced),
                  static_cast<unsigned long>(sample.queueDrops),
                  static_cast<unsigned long>(sample.readErrors),
                  sample.raw.ax, sample.raw.ay, sample.raw.az,
                  sample.raw.gx, sample.raw.gy, sample.raw.gz);
}

#if DUSTMOVER_USE_SD
static bool writeBlock(const char *data, size_t length) {
  return write(logFd, data, length) == static_cast<ssize_t>(length);
}

static void closeSession() {
  if (logFd >= 0) {
    close(logFd);
    logFd = -1;
  }
}

static bool openSession() {
  char path[32];
  for (unsigned int index = 0; index < 100000; ++index) {
    snprintf(path, sizeof(path), "%s/dm%05u.csv", sdMountPoint, index);
    logFd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (logFd < 0) {
      if (errno == EEXIST) {
        continue;
      }
      return false;
    }
    if (!writeBlock(csvHeader, sizeof(csvHeader) - 1) || fsync(logFd) != 0) {
      closeSession();
      return false;
    }
    Serial.printf("LOGGER file=%s\n", path + sizeof(sdMountPoint) - 1);
    return true;
  }
  return false;
}
#endif

static void samplerTask(void *) {
  uint32_t sequence = 0;
  uint32_t coalesced = 0;
  uint32_t readErrors = 0;
  uint32_t queueDrops = 0;
  uint32_t sampled = 0;
  char line[csvLineSize];

  for (;;) {
    uint32_t events = 1;
    if (config::useDataReadyInterrupt) {
      events = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      sequence += events;
      coalesced += events - 1;
      stats.sequence = sequence;
      stats.coalesced = coalesced;
    } else {
      // Poll the sensor's latched data-ready flag and yield between checks.
      vTaskDelay(1);
    }

    Icm42688::RawSample raw;
    const auto result = sensor.readSample(raw);
    if (result != Icm42688::ReadResult::ready) {
      if (result == Icm42688::ReadResult::invalid) {
        stats.readErrors = ++readErrors;
      }
      continue;
    }

    if (!config::useDataReadyInterrupt) {
      stats.sequence = ++sequence;
    }
    const int64_t timeUs = esp_timer_get_time();
    Sample sample{timeUs, sequence, events, coalesced, queueDrops, readErrors, raw};
    stats.sampled = ++sampled;
#if DUSTMOVER_USE_SD
    if (logging && xQueueSend(sampleQueue, &sample, 0) != pdTRUE) {
      sample.queueDrops = ++queueDrops;
      stats.queueDrops = queueDrops;
    }
#endif

    const int length = formatSample(line, sample);
    if (length > 0 && static_cast<size_t>(length) < sizeof(line)) {
      Serial.printf("RAW,%s", line);
    }
  }
}

#if DUSTMOVER_USE_SD
static void loggerTask(void *) {
  char block[2048];
  size_t used = 0;
  uint32_t pendingRows = 0;
  uint32_t written = 0;
  uint32_t lastFlush = millis();

  for (;;) {
    Sample sample;
    if (xQueueReceive(sampleQueue, &sample, pdMS_TO_TICKS(50)) == pdTRUE) {
      char line[csvLineSize];
      const int length = formatSample(line, sample);
      if (length <= 0 || static_cast<size_t>(length) >= sizeof(line)) {
        break;
      }
      if (used + static_cast<size_t>(length) > sizeof(block)) {
        if (!writeBlock(block, used)) {
          break;
        }
        used = 0;
      }
      memcpy(block + used, line, length);
      used += length;
      ++pendingRows;
    }

    const uint32_t now = millis();
    if (pendingRows && now - lastFlush >= config::flushPeriodMs) {
      if (!writeBlock(block, used) || fsync(logFd) != 0) {
        break;
      }
      used = 0;
      written += pendingRows;
      pendingRows = 0;
      stats.written = written;
      lastFlush = now;
    }
  }

  logging = false;
  stats.sdErrors = 1;
  closeSession();
  Serial.println("ERROR: SD write or sync failed. UART streaming continues.");
  vTaskDelete(nullptr);
}

static bool startLogger() {
  if (!openSession()) {
    Serial.println("ERROR: Could not create or sync the SD log file.");
    return false;
  }
  sampleQueue = xQueueCreate(config::queueLength, sizeof(Sample));
  if (sampleQueue &&
      xTaskCreate(loggerTask, "sd-logger", 6144, nullptr, 1, &loggerHandle) == pdPASS) {
    return true;
  }
  if (sampleQueue) {
    vQueueDelete(sampleQueue);
    sampleQueue = nullptr;
  }
  closeSession();
  Serial.println("ERROR: Could not allocate SD logger resources.");
  return false;
}
#endif

#if !DUSTMOVER_USE_SD
static void probeMiso() {
  const auto miso = static_cast<gpio_num_t>(config::spiMiso);
  const gpio_pull_mode_t modes[] = {GPIO_PULLUP_ONLY, GPIO_PULLDOWN_ONLY};
  const char *labels[] = {"pull-up", "pull-down"};
  for (size_t i = 0; i < 2; ++i) {
    if (gpio_set_pull_mode(miso, modes[i]) != ESP_OK) {
      Serial.println("DIAG: Could not set MISO pull mode.");
      break;
    }
    delayMicroseconds(10);
    const int idle = gpio_get_level(miso);
    uint8_t id;
    if (sensor.whoAmI(id)) {
      Serial.printf("DIAG: MISO GPIO%d %s idle=%d WHO_AM_I=0x%02X\n",
                    config::spiMiso, labels[i], idle, static_cast<unsigned>(id));
    } else {
      Serial.printf("DIAG: MISO GPIO%d %s idle=%d SPI transfer failed\n",
                    config::spiMiso, labels[i], idle);
    }
  }
  gpio_set_pull_mode(miso, GPIO_FLOATING);
}
#endif

void setup() {
  Serial.begin(config::serialBaud);
  digitalWrite(config::sdCs, HIGH);
  pinMode(config::sdCs, OUTPUT);
  if (config::useDataReadyInterrupt) {
    pinMode(config::dataReady, INPUT_PULLDOWN);
  }
  const esp_err_t spiError = sensor.begin(config::spiSck, config::spiMiso, config::spiMosi);
  delay(300);
  if (spiError != ESP_OK) {
    Serial.printf("ERROR: SPI initialization failed: %s\n", esp_err_to_name(spiError));
    return;
  }

#if DUSTMOVER_USE_SD
  Serial.println("DustMover: ICM-42688-P Arduino SPI and microSD");
  Serial.printf("INIT: SCK=%d MISO=%d MOSI=%d ICM_CS=%d SD_CS=%d\n",
                config::spiSck, config::spiMiso, config::spiMosi,
                config::sensorCs, config::sdCs);

  // Put the SD card into SPI mode before addressing the sensor.
  const bool sdReady = SD.begin(config::sdCs, SPI, config::sdSpiHz, sdMountPoint) &&
                       SD.cardType() != CARD_NONE;
  if (sdReady) {
    Serial.printf("INIT: SD card size=%llu MiB\n",
                  static_cast<unsigned long long>(SD.cardSize() / (1024ULL * 1024ULL)));
  } else {
    digitalWrite(config::sdCs, HIGH);
    stats.sdErrors = 1;
    Serial.println("ERROR: SD unavailable. Trying sensor with UART output only.");
  }
#else
  Serial.println("DustMover: ICM-42688-P ESP-IDF SPI3, UART output");
  Serial.printf("INIT: SCK=%d OUT, MOSI=%d OUT, MISO=%d IN, CS=%d OUT active low\n",
                config::spiSck, config::spiMosi, config::spiMiso, config::sensorCs);
  if (config::useDataReadyInterrupt) {
    Serial.printf("INIT: INT2=%d IN pull-down, rising edge\n", config::dataReady);
  }
#endif

  uint8_t id;
  if (!sensor.whoAmI(id)) {
    Serial.println("ERROR: SPI transfer failed during WHO_AM_I read.");
    return;
  }
  Serial.printf("INIT: ICM-42688-P WHO_AM_I=0x%02X\n", static_cast<unsigned>(id));
  if (id != Icm42688::expectedId) {
    Serial.println("ERROR: Unexpected sensor ID. Expected 0x47.");
#if !DUSTMOVER_USE_SD
    probeMiso();
#endif
    return;
  }
  if (!sensor.configure(config::useDataReadyInterrupt)) {
    Serial.println("ERROR: ICM-42688-P configuration failed.");
    return;
  }
  Serial.println("INIT: 100 Hz, accel +/-2 g, gyro +/-250 deg/s");
  Serial.printf("CSV: %s", csvHeader);

#if DUSTMOVER_USE_SD
  if (sdReady) {
    logging = startLogger();
    if (!logging) {
      stats.sdErrors = 1;
    }
  }
#endif
  if (xTaskCreate(samplerTask, "sampler", 4096, nullptr, 3, &samplerHandle) != pdPASS) {
#if DUSTMOVER_USE_SD
    if (logging) {
      logging = false;
      vTaskDelete(loggerHandle);
      vQueueDelete(sampleQueue);
      closeSession();
    }
#endif
    Serial.println("ERROR: Could not start sampler task.");
    return;
  }

  if (config::useDataReadyInterrupt) {
    attachInterrupt(digitalPinToInterrupt(config::dataReady), dataReadyISR, RISING);
    Serial.printf("INIT: INT2 data ready on GPIO%d\n", config::dataReady);
  } else {
    Serial.println("INIT: Polling sensor data-ready status");
  }
  sampling = true;
#if DUSTMOVER_USE_SD
  Serial.println(logging ? "LOGGER started." : "LOGGER unavailable. UART streaming only.");
#else
  Serial.println("UART streaming started.");
#endif
}

void loop() {
  if (sampling) {
#if DUSTMOVER_USE_SD
    Serial.printf("DBG state=%s seq=%lu sampled=%lu written=%lu coalesced=%lu "
                  "queue_drops=%lu read_errors=%lu sd_errors=%lu\n",
                  logging ? "RUN" : "UART",
                  static_cast<unsigned long>(stats.sequence.load()),
                  static_cast<unsigned long>(stats.sampled.load()),
                  static_cast<unsigned long>(stats.written.load()),
                  static_cast<unsigned long>(stats.coalesced.load()),
                  static_cast<unsigned long>(stats.queueDrops.load()),
                  static_cast<unsigned long>(stats.readErrors.load()),
                  static_cast<unsigned long>(stats.sdErrors.load()));
#else
    Serial.printf("DBG state=UART seq=%lu sampled=%lu written=0 coalesced=%lu "
                  "queue_drops=0 read_errors=%lu sd_errors=0\n",
                  static_cast<unsigned long>(stats.sequence.load()),
                  static_cast<unsigned long>(stats.sampled.load()),
                  static_cast<unsigned long>(stats.coalesced.load()),
                  static_cast<unsigned long>(stats.readErrors.load()));
#endif
  }
  delay(1000);
}
