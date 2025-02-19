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
#define TRIG_PIN         19
#define ECHO_PIN         21
#define DHT_TYPE         DHT11

#define IN1_PIN          12
#define IN2_PIN          14
#define IN3_PIN          26
#define IN4_PIN          27
#define ENA_PIN          13
#define ENB_PIN          25

// Configuração do Sonar
#define MAX_DISTANCE     200  // cm
#define COLLISION_DISTANCE 20 // cm
#define PULSE_TIMEOUT    6000 // microseconds (200cm)

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

// Estados do motor
enum MotorState { MOVING_FORWARD, AVOIDING, REVERSING, TURNING };
MotorState currentState = MOVING_FORWARD;
unsigned long avoidanceStart = 0;

// Função auxiliar do sonar atualizada
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, PULSE_TIMEOUT);
  if (duration == 0) return -1; // Timeout
  
  return duration * 0.034 / 2;
}

// Funções de controle do motor
void setMotorForward() {
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, HIGH);
  digitalWrite(IN4_PIN, LOW);
  analogWrite(ENA_PIN, 200);
  analogWrite(ENB_PIN, 200);
}

void setMotorReverse() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, HIGH);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, 200);
  analogWrite(ENB_PIN, 200);
}

void setMotorTurnRight() {
  digitalWrite(IN1_PIN, HIGH);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, HIGH);
  analogWrite(ENA_PIN, 200);
  analogWrite(ENB_PIN, 200);
}

void stopMotor() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
}

// Cria uma queue para transmitir as medidas de distância
QueueHandle_t xDistanceQueue;

// Estrutura de dados compartilhados
typedef struct {
  float temperature;
  float humidity;
  int luminosity;
  int gas;
  long distance;
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

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(ENB_PIN, OUTPUT);

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
  xTaskCreatePinnedToCore(sonarTask, "Sonar", 2048, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(sendDataToSTA_ESP, "HTTP", 4096, NULL, 3, NULL, 1);

  // Iniciar movimento
  setMotorForward();
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

// Modifique a task do sonar para incluir evitação
void sonarTask(void *pvParameters) {
  long distance;
  while(1) {
    distance = measureDistance();
    
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      sharedData.distance = distance;
      xSemaphoreGive(mutex);
      
      // Lógica de evitação de obstáculos
      if (distance > 0 && distance < COLLISION_DISTANCE && currentState == MOVING_FORWARD) {
        currentState = REVERSING;
        avoidanceStart = millis();
        setMotorReverse();
      }
      xEventGroupSetBits(eventGroup, SONAR_READY_BIT);
    }
    
    // Gerenciamento de estados
    if (currentState != MOVING_FORWARD) {
      if (millis() - avoidanceStart > 1000 && currentState == REVERSING) {
        currentState = TURNING;
        setMotorTurnRight();
        avoidanceStart = millis();
      }
      else if (millis() - avoidanceStart > 3000 && currentState == TURNING) {
        currentState = MOVING_FORWARD;
        setMotorForward();
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}


void sendDataToSTA_ESP(void *pvParameters) {
  const EventBits_t allBits = DHT_READY_BIT | LDR_READY_BIT | MQ2_READY_BIT | SONAR_READY_BIT;
  
  while(1) {
    xEventGroupWaitBits(eventGroup, allBits, pdTRUE, pdTRUE, portMAX_DELAY);

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      Serial.println("Dados recebidos:");
      Serial.print("Temperatura: "); Serial.println(sharedData.temperature);
      Serial.print("Umidade: "); Serial.println(sharedData.humidity);
      Serial.print("Luminosidade: "); Serial.println(sharedData.luminosity);
      Serial.print("Gás: "); Serial.println(sharedData.gas);
      Serial.print("Distância: "); Serial.println(sharedData.distance);
      Serial.println("---------------------------------");
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
