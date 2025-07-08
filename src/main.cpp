#include "config.h"
#include "sensors.h"

SensorData sharedData;
SemaphoreHandle_t dataMutex;
EventGroupHandle_t sensorEventGroup;

void setup() {
    Serial.begin(115200);

    dataMutex = xSemaphoreCreateMutex();
    sensorEventGroup = xEventGroupCreate();
    
    setupSensors();

    xTaskCreate(mpuTask, "MPU Task", 4096, NULL, 2, NULL); 
    xTaskCreate(DEBUG_TASK, "DEBUG", 1024, NULL, 3, NULL);
}

void loop() {
}