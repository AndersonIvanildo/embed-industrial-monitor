#include <Arduino.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

// Definições de hardware
#define DHT_PIN          33
#define LDR_PIN          32
#define MQ2_PIN          21
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
//void irTask(void *pvParameters);
void ldrTask(void *pvParameters);
void mq2Task(void *pvParameters);
void sonarTask(void *pvParameters);
void httpTask(void *pvParameters);

// Estrutura de dados compartilhados
typedef struct {
  float temperature;
  float humidity;
  int luminosity;
  int gas;
  float distance;
} SensorData;

// Objetos globais
DHT dht(DHT_PIN, DHT_TYPE);
SemaphoreHandle_t mutex;
EventGroupHandle_t eventGroup;
SensorData sharedData;

// Configurações de Wi-Fi
const char* ssid = "SUA_REDE_WIFI";
const char* password = "SUA_SENHA_WIFI";

// Endereço do servidor local
const char* serverUrl = "http://192.168.1.100:5000/data"; // Substitua pelo IP e porta do seu servidor

void setup() {
  Serial.begin(115200);
  
  // Inicialização dos sensores
  dht.begin();
  //pinMode(IR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Conectar ao Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao Wi-Fi...");
  }
  Serial.println("Conectado ao Wi-Fi");

  // Criação de recursos FreeRTOS
  mutex = xSemaphoreCreateMutex();
  eventGroup = xEventGroupCreate();

  // Criação de tasks
  xTaskCreatePinnedToCore(dhtTask, "DHT", 2048, NULL, 1, NULL, 0);
  //xTaskCreatePinnedToCore(irTask, "IR", 1024, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ldrTask, "LDR", 1024, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(mq2Task, "MQ2", 1024, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(sonarTask, "Sonar", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(httpTask, "HTTP", 4096, NULL, 2, NULL, 1);
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

void httpTask(void *pvParameters) {
  const EventBits_t allBits = DHT_READY_BIT | LDR_READY_BIT | MQ2_READY_BIT | SONAR_READY_BIT;
  
  while(1) {
    xEventGroupWaitBits(eventGroup, allBits, pdTRUE, pdTRUE, portMAX_DELAY);

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      String jsonData = "{\"Temperatura\":" + String(sharedData.temperature) + 
                        ",\"Humidade\":" + String(sharedData.humidity) + 
                        ",\"Luminosidade\":" + String(sharedData.luminosity) + 
                        ",\"Gás\":" + String(sharedData.gas) +
                        ",\"Distância\":" + String(sharedData.distance) + "}";

      HTTPClient http;
      http.begin(serverUrl);
      http.addHeader("Content-Type", "application/json");
      
      int httpResponseCode = http.POST(jsonData);
      if (httpResponseCode > 0) {
        Serial.println("Dados enviados com sucesso!");
        Serial.println("Resposta do servidor: " + http.getString());
      } else {
        Serial.println("Erro no envio: " + String(httpResponseCode));
      }
      http.end();
      
      xSemaphoreGive(mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
