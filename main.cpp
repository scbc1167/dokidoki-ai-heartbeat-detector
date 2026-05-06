#include <Arduino.h>
#include "Seeed_Arduino_mmWave.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// --- Wi-Fi configuration ---
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
const char* flux_url = "YOUR_API_ENDPOINT";

// --- OLED configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- LED & Button pin configuration ---
#define LED_PIN 2
#define BUTTON_PIN 1
#define SAMPLE_COUNT 100

// --- For mmWave sensor ---
#ifdef ESP32
  HardwareSerial mmWaveSerial(0);
#else
  #define mmWaveSerial Serial1
#endif

SEEED_MR60BHA2 mmWave;

// --- Sensor data structure ---
typedef struct {
  float total_phase;
  float breath_phase;
  float heart_phase;
  float breath_rate;
  float heart_rate;
  float distance;
} SensorData_t;

// --- FreeRTOS queue ---
static QueueHandle_t sensorQueuePrint   = nullptr;
static QueueHandle_t sensorQueueDisplay = nullptr;
static QueueHandle_t heartRateQueue     = nullptr;
static QueueHandle_t sensorQueueFlux    = nullptr;

// --- Data buffer ---
float heartRateBuffer[SAMPLE_COUNT];
float breathRateBuffer[SAMPLE_COUNT];
float distanceBuffer[SAMPLE_COUNT];

// --- Display message ---
String displayMessage = "";
unsigned long messageExpireTime = 0;
SemaphoreHandle_t displayMutex;

// --- WiFi connection ---
void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

// --- Measurement task ---
void MeasureTask(void *pvParameters) {
  SensorData_t data;
  for (;;) {
    if (mmWave.update(100)) {
      mmWave.getHeartBreathPhases(data.total_phase, data.breath_phase, data.heart_phase);
      mmWave.getBreathRate(data.breath_rate);
      mmWave.getHeartRate(data.heart_rate);
      mmWave.getDistance(data.distance);
    } else {
      data = {0, 0, 0, 0, 0, 0};
    }

    xQueueOverwrite(sensorQueuePrint, &data);
    xQueueOverwrite(sensorQueueDisplay, &data);
    xQueueOverwrite(sensorQueueFlux, &data);

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// --- OLED display task ---
void DisplayTask(void *pvParameters) {
  SensorData_t data;
  for (;;) {
    if (xQueueReceive(sensorQueueDisplay, &data, pdMS_TO_TICKS(100)) == pdPASS) {
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.printf("Breath: %.1f bpm", data.breath_rate);
        display.setCursor(0, 16);
        display.printf("Heart : %.1f bpm", data.heart_rate);
        display.setCursor(0, 32);
        display.printf("Dist. : %.1f cm", data.distance);

        if (millis() < messageExpireTime && displayMessage.length() > 0) {
          display.setCursor(0, 48);
          display.print(displayMessage);
        }

        display.display();
        xSemaphoreGive(displayMutex);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// --- Serial output & heartbeat transmission task for LED ---
void PrintTask(void *pvParameters) {
  SensorData_t data;
  for (;;) {
    if (xQueueReceive(sensorQueuePrint, &data, portMAX_DELAY) == pdPASS) {
      Serial.printf("total_phase: %.2f\tbreath_phase: %.2f\theart_phase: %.2f\n",
                    data.total_phase, data.breath_phase, data.heart_phase);
      Serial.printf("breath_rate: %.2f\theart_rate: %.2f\tdistance: %.2f\n\n",
                    data.breath_rate, data.heart_rate, data.distance);

      xQueueOverwrite(heartRateQueue, &data.heart_rate);
    }
  }
}

// --- LED blinking task (synchronized with heartbeat) ---
void HeartLEDTask(void *pvParameters) {
  pinMode(LED_PIN, OUTPUT);
  float rate = 0.0;

  for (;;) {
    xQueueReceive(heartRateQueue, &rate, pdMS_TO_TICKS(100));

    if (rate < 10.0 || isnan(rate) || rate > 200.0) {
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    float interval = 60.0 / rate;
    int delay_ms = (int)(interval * 1000 / 2);

    digitalWrite(LED_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    digitalWrite(LED_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

// --- 10-second data transmission task triggered by a button press ---
void ButtonTriggeredFluxTask(void *pvParameters) {
  SensorData_t data;

  for (;;) {
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Button pressed: start 10s collection");

      // メッセージ表示（button pressed !）
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        displayMessage = "button pressed !";
        messageExpireTime = millis() + 10000;
        xSemaphoreGive(displayMutex);
      }

      int count = 0;
      while (count < SAMPLE_COUNT) {
        if (xQueueReceive(sensorQueueFlux, &data, pdMS_TO_TICKS(200)) == pdPASS) {
          heartRateBuffer[count]  = data.heart_rate;
          breathRateBuffer[count] = data.breath_rate;
          distanceBuffer[count]   = data.distance;
          count++;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
      }

      String json = "{\"heart_rate\":[";
      for (int i = 0; i < SAMPLE_COUNT; i++) {
        json += String(heartRateBuffer[i], 1);
        if (i < SAMPLE_COUNT - 1) json += ",";
      }
      json += "],\"breath_rate\":[";
      for (int i = 0; i < SAMPLE_COUNT; i++) {
        json += String(breathRateBuffer[i], 1);
        if (i < SAMPLE_COUNT - 1) json += ",";
      }
      json += "],\"distance\":[";
      for (int i = 0; i < SAMPLE_COUNT; i++) {
        json += String(distanceBuffer[i], 1);
        if (i < SAMPLE_COUNT - 1) json += ",";
      }
      json += "]}";

      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(flux_url);
        http.addHeader("Content-Type", "application/json");
        int res = http.POST(json);
        Serial.print("POST Flux (button): "); Serial.println(json);
        Serial.print("Response: "); Serial.println(res);
        http.end();
      }

      // Message display（data sended !）
      if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        displayMessage = "data sended !";
        messageExpireTime = millis() + 3000;
        xSemaphoreGive(displayMutex);
      }

      vTaskDelay(pdMS_TO_TICKS(3000)); // ボタン連打防止
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  mmWave.begin(&mmWaveSerial);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 init failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  connectToWiFi();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  displayMutex = xSemaphoreCreateMutex();

  sensorQueuePrint   = xQueueCreate(1, sizeof(SensorData_t));
  sensorQueueDisplay = xQueueCreate(1, sizeof(SensorData_t));
  heartRateQueue     = xQueueCreate(1, sizeof(float));
  sensorQueueFlux    = xQueueCreate(1, sizeof(SensorData_t));

  xTaskCreate(MeasureTask,            "MeasureTask",     4096, nullptr, 2, nullptr);
  xTaskCreate(PrintTask,              "PrintTask",       4096, nullptr, 1, nullptr);
  xTaskCreate(DisplayTask,            "DisplayTask",     4096, nullptr, 1, nullptr);
  xTaskCreate(HeartLEDTask,           "LEDTask",         2048, nullptr, 1, nullptr);
  xTaskCreate(ButtonTriggeredFluxTask,"BtnFluxTask",     8192, nullptr, 1, nullptr);
}

void loop() {
  // Empty (reserved for FreeRTOS use)
}
