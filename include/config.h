/* ============ Configs used on project ============*/
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

// Configure Pins to modules
#define DHT_PIN

typedef struct { 
    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
} SensorData;

extern SemaphoreHandle_t dataMutex;
extern EventGroupHandle_t eventGroup;
extern SensorData sharedData;
