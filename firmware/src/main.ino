#include <Arduino.h>
#include "VapCo.h"
#include "SmsGps.h"
#include "HUD.h"

SmartHelmetSensor helmet(33, 32, 2);

TaskHandle_t hSensor = nullptr;
TaskHandle_t smsTaskHandle = nullptr;

void TaskSensor(void*) {
  helmet.begin();

  const TickType_t readPeriod = pdMS_TO_TICKS(10);
  TickType_t lastRead = xTaskGetTickCount();

  for (;;) {
    TickType_t now = xTaskGetTickCount();
    if (now - lastRead >= readPeriod) {
      lastRead = now;
      helmet.update();
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  initHUD();
  initSmsGps();

  xTaskCreatePinnedToCore(TaskSensor, "Sensor", 8192, NULL, 4, &hSensor, 1);
  xTaskCreatePinnedToCore(taskSMS, "TaskSMS", 8192, NULL, 1, &smsTaskHandle, 1);
  xTaskCreatePinnedToCore(taskHUD, "TaskHUD", 8192, NULL, 1, NULL, 0);
}

void loop() {}
