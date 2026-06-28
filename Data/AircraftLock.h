#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t gAircraftMutex;

static inline void aircraftLock() {
  if (gAircraftMutex) xSemaphoreTake(gAircraftMutex, portMAX_DELAY);
}

static inline void aircraftUnlock() {
  if (gAircraftMutex) xSemaphoreGive(gAircraftMutex);
}
