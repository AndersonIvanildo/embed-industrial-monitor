#include <Arduino.h>
#include <DHT.h>

#include <esp_now.h>
#include <WiFi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

// Definições de hardware
#define DHT_PIN          33
#define LDR_PIN          34
#define MQ2_PIN          35
#define TRIG_PIN         22
#define ECHO_PIN         23
#define DHT_TYPE         DHT11

// Event Group bits
#define DHT_READY_BIT    (1 << 0)
#define LDR_READY_BIT    (1 << 1)
#define MQ2_READY_BIT    (1 << 2)
#define SONAR_READY_BIT  (1 << 3)

// Protótipos da função
void dhtTask(void *pvParameters);
void ldrTask(void *pvParameters);
void mq2Task(void *pvParameters);
void sonarTask(void *pvParameters);
void sendDataToSTA_ESP(void *pvParameters);

// Estrutura de dados compartilhados
typedef struct {
  float temperature;
  float humidity;
  int luminosity;
  int gas;
  float distance;
} SensorData;

uint8_t broadcastAddress[] = {0x24, 0x62, 0xab, 0xca, 0x7b, 0x88};
esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// Objetos globais
DHT dht(DHT_PIN, DHT_TYPE);
SemaphoreHandle_t mutex;
EventGroupHandle_t eventGroup;
SensorData sharedData;

void setup() {
  Serial.begin(115200);
  
  // Inicialização dos sensores
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  // Criação de recursos FreeRTOS
  mutex = xSemaphoreCreateMutex();
  eventGroup = xEventGroupCreate();

  // Criação de tasks
  xTaskCreatePinnedToCore(dhtTask, "DHT", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ldrTask, "LDR", 1024, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(mq2Task, "MQ2", 1024, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(sonarTask, "Sonar", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(sendDataToSTA_ESP, "HTTP", 4096, NULL, 2, NULL, 1);
}

void loop() {
}

void dhtTask(void *pvParameters) {
  while(1) {
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      sharedData.temperature = dht.readTemperature();
      sharedData.humidity = dht.readHumidity();
      xSemaphoreGive(mutex);
      
      xEventGroupSetBits(eventGroup, DHT_READY_BIT);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}


void ldrTask(void *pvParameters) {
  while(1) {
    int value = analogRead(LDR_PIN);
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      sharedData.luminosity = value;
      xSemaphoreGive(mutex);
      
      xEventGroupSetBits(eventGroup, LDR_READY_BIT);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void mq2Task(void *pvParameters) {
  while(1) {
    int value = analogRead(MQ2_PIN);
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      sharedData.gas = value;
      xSemaphoreGive(mutex);
      
      xEventGroupSetBits(eventGroup, MQ2_READY_BIT);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void sonarTask(void *pvParameters) {
  while(1) {
    digitalWrite(TRIG_PIN, LOW);
    vTaskDelay(pdMS_TO_TICKS(2));
    digitalWrite(TRIG_PIN, HIGH);
    vTaskDelay(pdMS_TO_TICKS(10));
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH);
    float distance = duration * 0.034 / 2;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      sharedData.distance = distance;
      xSemaphoreGive(mutex);
      
      xEventGroupSetBits(eventGroup, SONAR_READY_BIT);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void sendDataToSTA_ESP(void *pvParameters) {
  const EventBits_t allBits = DHT_READY_BIT | LDR_READY_BIT | MQ2_READY_BIT | SONAR_READY_BIT;
  
  while(1) {
    xEventGroupWaitBits(eventGroup, allBits, pdTRUE, pdTRUE, portMAX_DELAY);

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      Serial.printf("{\"temperature\":%.1f, \"humidity\":%.1f, \"luminosity\":%d, \"gas\":%d, \"distance\":%.1f}\n", sharedData.temperature, sharedData.humidity, sharedData.luminosity, sharedData.gas, sharedData.distance);

      // Send message via ESP-NOW
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &sharedData, sizeof(sharedData));
      
      if (result == ESP_OK) {
        Serial.println("Sent with success");
      }
      else {
        Serial.println("Error sending the data");
      }
      xSemaphoreGive(mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
