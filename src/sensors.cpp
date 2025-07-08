#include "sensors.h"
#include "config.h"

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

void setupSensors() {
    Serial.println("Begin sensors...");
    
    if (!mpu.begin()) {
        Serial.println("Fail to find MPU6050!");
        while (1) { delay(10); }
    }
    Serial.println("MPU6050 finded!");
    
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void mpuTask(void *pvParameters) {
    sensors_event_t a, g, temp;

    while(1) {
        mpu.getEvent(&a, &g, &temp);

        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            
            sharedData.accel_x = a.acceleration.x;
            sharedData.accel_y = a.acceleration.y;
            sharedData.accel_z = a.acceleration.z;
            
            sharedData.gyro_x = g.gyro.x;
            sharedData.gyro_y = g.gyro.y;
            sharedData.gyro_z = g.gyro.z;

            xSemaphoreGive(dataMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}

void DEBUG_TASK(void *pvParameters) {
    while(1) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            
            Serial.print("Accel [X: ");
            Serial.print(sharedData.accel_x, 2);
            Serial.print(" | Y: ");
            Serial.print(sharedData.accel_y, 2);
            Serial.print(" | Z: ");
            Serial.print(sharedData.accel_z, 2);
            Serial.print("]  ");
            Serial.print("\t\t\t");

            Serial.print("Gyro [X: ");
            Serial.print(sharedData.gyro_x, 2);
            Serial.print(" | Y: ");
            Serial.print(sharedData.gyro_y, 2);
            Serial.print(" | Z: ");
            Serial.print(sharedData.gyro_z, 2);
            Serial.println("]");

            xSemaphoreGive(dataMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}