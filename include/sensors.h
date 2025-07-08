#pragma once
#include <Adafruit_MPU6050.h>
#include "config.h"

extern Adafruit_MPU6050 mpu;

void setupSensors();
void mpuTask(void *pvParameters);
void DEBUG_TASK(void *pvParameters);