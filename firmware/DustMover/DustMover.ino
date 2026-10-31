#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <ADXL345_WE.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "Config.h"

// The timer wakes the sampler. All I2C work belongs to the sampler;
// all file writes belong to the logger. No I/O in the timer callback.
struct Sample {
  int64_t timeUs;
  uint32_t sequence;
  uint32_t skippedTicks;
  uint32_t queueDrops;
  uint32_t readErrors;
  float x, y, z;
};

static ADXL345_WE sensor(config::sensorAddress);
static QueueHandle_t samples = nullptr;
static TaskHandle_t samplerHandle = nullptr;
static TaskHandle_t loggerHandle = nullptr;
static esp_timer_handle_t sampleTimer = nullptr;
static File logFile;

static void timerCallback(void *) {
  xTaskNotifyGive(samplerHandle);  // ESP_TIMER_TASK context, not an ISR
}

static void samplerTask(void *) {
  uint32_t sequence = 0, skippedTicks = 0, queueDrops = 0, readErrors = 0;
  for (;;) {
    const uint32_t ticks = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    // Late wakeups read one current value; never invent historical samples.
    skippedTicks += ticks - 1;
    sequence += ticks;
    xyzFloat acceleration;
    const int64_t readStartedUs = esp_timer_get_time();
    if (!sensor.getGValues(&acceleration)) {
      ++readErrors;
      if (readErrors == 1) Serial.println("ERROR: Sensor read failed; invalid values are skipped.");
      continue;
    }
    Sample sample{readStartedUs, sequence, skippedTicks, queueDrops, readErrors,
                  acceleration.x, acceleration.y, acceleration.z};
    if (xQueueSend(samples, &sample, 0) != pdTRUE) {
      ++queueDrops;  // finite queue, explicit drop-newest policy
    }
  }
}

static bool writeBlock(const char *data, size_t length) {
  return logFile.write(reinterpret_cast<const uint8_t *>(data), length) == length
         && !logFile.getWriteError();
}

static void loggerTask(void *) {
  // Do not write until setup has successfully created and started the timer.
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  char block[2048];
  size_t used = 0;
  uint32_t lastFlush = millis();
  bool healthy = true;
  for (;;) {
    Sample sample;
    if (xQueueReceive(samples, &sample, pdMS_TO_TICKS(100)) == pdTRUE) {
      char line[160];
      const int length = snprintf(line, sizeof(line), "%lld;%lu;%lu;%lu;%lu;%.6f;%.6f;%.6f\n",
          static_cast<long long>(sample.timeUs),
          static_cast<unsigned long>(sample.sequence),
          static_cast<unsigned long>(sample.skippedTicks),
          static_cast<unsigned long>(sample.queueDrops),
          static_cast<unsigned long>(sample.readErrors), sample.x, sample.y, sample.z);
      if (length <= 0 || static_cast<size_t>(length) >= sizeof(line)) {
        healthy = false;
      } else {
        if (used + length > sizeof(block)) {
          healthy = writeBlock(block, used);
          used = 0;
        }
        if (healthy) {
          memcpy(block + used, line, length);
          used += length;
        }
      }
    }
    if (healthy && static_cast<uint32_t>(millis() - lastFlush) >= config::flushPeriodMs) {
      if (used) healthy = writeBlock(block, used);
      used = 0;
      logFile.flush();
      healthy = healthy && !logFile.getWriteError();
      lastFlush = millis();
    }
    if (!healthy) {
      esp_timer_stop(sampleTimer);
      logFile.close();
      Serial.println("ERROR: SD write failed. Recording stopped; reboot after checking card.");
      vTaskSuspend(nullptr);
    }
  }
}

static bool openSession() {
  char path[24];
  for (unsigned int i = 0; i < 100000; ++i) {
    snprintf(path, sizeof(path), "/dm%05u.csv", i);
    if (SD.exists(path)) continue;
    logFile = SD.open(path, FILE_WRITE);
    if (!logFile) return false;
    const char *header = "t_us;sequence;skipped_timer_ticks;queue_drops;read_errors;x_g;y_g;z_g\n";
    if (!writeBlock(header, strlen(header))) {
      logFile.close();
      return false;
    }
    logFile.flush();
    if (logFile.getWriteError()) {
      logFile.close();
      return false;
    }
    Serial.printf("Log: %s\n", path);
    return true;
  }
  return false;
}

static void cleanupStartup() {
  if (sampleTimer) {
    esp_timer_delete(sampleTimer);
    sampleTimer = nullptr;
  }
  if (samplerHandle) vTaskDelete(samplerHandle);
  if (loggerHandle) vTaskDelete(loggerHandle);
  if (samples) vQueueDelete(samples);
  logFile.close();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("DustMover: ESP32 / Arduino / FreeRTOS / 100 Hz polling");
  Wire.begin(config::sda, config::scl);
  Wire.setClock(400000);
  Wire.setTimeOut(20);
  if (!sensor.init()) {
    Serial.println("ERROR: ADXL345 missing. Check wiring/address. Recording not started.");
    return;
  }
  // 200 Hz sensor ODR, 100 Hz latest-value polling; intentionally no FIFO.
  if (!sensor.setDataRate(ADXL345_DATA_RATE_200) ||
      !sensor.setRange(ADXL345_RANGE_2G) ||
      !sensor.setFifoMode(ADXL345_BYPASS) ||
      !sensor.setMeasureMode(true)) {
    Serial.println("ERROR: Sensor configuration failed. Recording not started.");
    return;
  }

  SPI.begin(config::sdSck, config::sdMiso, config::sdMosi, config::sdCs);
  if (!SD.begin(config::sdCs, SPI, 4000000) || SD.cardType() == CARD_NONE || !openSession()) {
    Serial.println("ERROR: SD unavailable. Recording not started.");
    logFile.close();
    return;
  }
  samples = xQueueCreate(config::queueLength, sizeof(Sample));
  if (!samples ||
      xTaskCreate(samplerTask, "sampler", 4096, nullptr, 3, &samplerHandle) != pdPASS ||
      xTaskCreate(loggerTask, "sd-logger", 6144, nullptr, 1, &loggerHandle) != pdPASS) {
    Serial.println("ERROR: Cannot allocate queue/tasks. Recording not started.");
    cleanupStartup();
    return;
  }
  esp_timer_create_args_t timerArgs{};
  timerArgs.callback = timerCallback;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "sample-clock";
  if (esp_timer_create(&timerArgs, &sampleTimer) != ESP_OK ||
      esp_timer_start_periodic(sampleTimer, config::samplePeriodUs) != ESP_OK) {
    Serial.println("ERROR: Timer start failed. Recording not started.");
    cleanupStartup();
    return;
  }
  xTaskNotifyGive(loggerHandle);
  Serial.println("Recording started. Keep card inserted and power stable.");
}

void loop() {
  delay(1000); // FreeRTOS tasks perform acquisition and logging.
}
