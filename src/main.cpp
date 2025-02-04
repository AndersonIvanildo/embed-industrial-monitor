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
#define DHT_PIN          4
#define IR_PIN           5
#define LDR_PIN          34
#define MQ2_PIN          35
#define TRIG_PIN         18
#define ECHO_PIN         19
#define DHT_TYPE         DHT11

// Event Group bits
#define DHT_READY_BIT    (1 << 0)
#define IR_READY_BIT     (1 << 1)
#define LDR_READY_BIT    (1 << 2)
#define MQ2_READY_BIT    (1 << 3)
#define SONAR_READY_BIT  (1 << 4)

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void dhtTask(void *pvParameters);
void irTask(void *pvParameters);
void ldrTask(void *pvParameters);
void mq2Task(void *pvParameters);
void sonarTask(void *pvParameters);
void espnowTask(void *pvParameters);

// Estrutura de dados compartilhados
typedef struct {
  float temperature;
  float humidity;
  int luminosity;
  int gas;
  int ir_status;
  float distance;
} SensorData;

// Objetos globais
DHT dht(DHT_PIN, DHT_TYPE);
SemaphoreHandle_t mutex;
EventGroupHandle_t eventGroup;
SensorData sharedData;

// Endereço MAC do receptor ESP-NOW
uint8_t receiverMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

void setup() {
  Serial.begin(115200);
  
  // Inicialização dos sensores
  dht.begin();
  pinMode(IR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Inicialização do ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ESP-NOW");
    return;
  }
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMac, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Criação de recursos FreeRTOS
  mutex = xSemaphoreCreateMutex();
  eventGroup = xEventGroupCreate();

  // Criação de tasks
  xTaskCreate(dhtTask, "DHT", 2048, NULL, 1, NULL);
  xTaskCreate(irTask, "IR", 1024, NULL, 1, NULL);
  xTaskCreate(ldrTask, "LDR", 1024, NULL, 1, NULL);
  xTaskCreate(mq2Task, "MQ2", 1024, NULL, 1, NULL);
  xTaskCreate(sonarTask, "Sonar", 2048, NULL, 1, NULL);
  xTaskCreate(espnowTask, "ESP-NOW", 4096, NULL, 2, NULL);
}

void loop() {
  // Nada aqui - tudo é tratado pelas tasks
}

// Callback de envio ESP-NOW
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Envio OK" : "Falha");
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

void irTask(void *pvParameters) {
  while(1) {
    int status = digitalRead(IR_PIN);
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      sharedData.ir_status = status;
      xSemaphoreGive(mutex);
      
      xEventGroupSetBits(eventGroup, IR_READY_BIT);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
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

void espnowTask(void *pvParameters) {
  const EventBits_t allBits = DHT_READY_BIT | IR_READY_BIT | LDR_READY_BIT | MQ2_READY_BIT | SONAR_READY_BIT;
  
  while(1) {
    xEventGroupWaitBits(eventGroup, allBits, pdTRUE, pdTRUE, portMAX_DELAY);

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      // Simula o envio imprimindo os dados no Serial
      Serial.println("=== Simulando envio de dados via ESP-NOW ===");
      Serial.print("Temperatura: "); Serial.println(sharedData.temperature);
      Serial.print("Umidade: "); Serial.println(sharedData.humidity);
      Serial.print("Luminosidade: "); Serial.println(sharedData.luminosity);
      Serial.print("Gás: "); Serial.println(sharedData.gas);
      Serial.print("IR Status: "); Serial.println(sharedData.ir_status);
      Serial.print("Distância: "); Serial.println(sharedData.distance);
      Serial.println("============================================");

      // Mantém o código original para quando a conectividade estiver disponível
      esp_now_register_send_cb(OnDataSent);
      esp_err_t result = esp_now_send(receiverMac, (uint8_t *) &sharedData, sizeof(sharedData));
      
      if (result != ESP_OK) {
        Serial.println("Erro no envio");
      }
      xSemaphoreGive(mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
