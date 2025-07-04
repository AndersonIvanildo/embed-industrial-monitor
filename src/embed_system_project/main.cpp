#include <Arduino.h>

// COMUNICAÇÃO E ENVIO DE DADOS
#include <esp_now.h>
#include <WiFi.h>

// SENSORES DA APLICAÇÃO
#include <DHT.h>

// GERENCIAMENTO DE TAREFAS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

// DEFINIÇÕES DOS PINOS DO HARDWARE
#define DHT_PIN          23
#define LDR_PIN          34
#define MQ2_PIN          35
#define TRIG_PIN         12
#define ECHO_PIN         13
#define DHT_TYPE         DHT11

#define ENA 14    // GPIO14 (D14)
#define IN1 26    // GPIO26 (D26)
#define IN2 25    // GPIO25 (D25)
#define ENB 27    // GPIO27 (D27)
#define IN3 32    // GPIO32 (D32)
#define IN4 33    // GPIO33 (D33)

// Event Group bits
#define DHT_READY_BIT    (1 << 0)
#define LDR_READY_BIT    (1 << 1)
#define MQ2_READY_BIT    (1 << 2)
#define SONAR_READY_BIT  (1 << 3)

// PROTÓTIPO DAS FUNÇÕES
void dhtTask(void *pvParameters);
void ldrTask(void *pvParameters);
void mq2Task(void *pvParameters);
void sonarTask(void *pvParameters);
void vTask_sendData(void *pvParameters);
void movementTask(void *pvParameters);
void initialize_Comunication();
void moveForward();
void moveBackward();
void turnLeft();
void turnRight();
void stopMotors();

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

/* ================================ COMUNICAÇÃO ================================ */
uint8_t broadcastAddress[] = {}; // Endereço do receptor
esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nSTATUS DO PACOTE ENVIADO:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Enviado com sucesso!" : "Entrega Falhou!");
}


void setup() {
  Serial.begin(115200);

  initialize_Comunication();

  
  // Inicialização dos sensores
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  // Inicialização do PWM para motores
  ledcSetup(0, 5000, 8); // Canal 0 para ENA
  ledcSetup(1, 5000, 8); // Canal 1 para ENB
  ledcAttachPin(ENA, 0);
  ledcAttachPin(ENB, 1);

  // Configuração dos pinos dos motores
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Criação de recursos FreeRTOS
  mutex = xSemaphoreCreateMutex();
  eventGroup = xEventGroupCreate();

  // Teste inicial dos motores
  moveForward();
  delay(2000);
  stopMotors();

  // Criação de tasks
  xTaskCreatePinnedToCore(dhtTask, "DHT", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(ldrTask, "LDR", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(mq2Task, "MQ2", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(sonarTask, "Sonar", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(vTask_sendData, "ESP_NOW", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(movementTask, "Movement", 4096, NULL, 1, NULL, 1);
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
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void vTask_sendData(void *pvParameters) {
  const EventBits_t allBits = DHT_READY_BIT | LDR_READY_BIT | MQ2_READY_BIT | SONAR_READY_BIT;
  
  while(1) {
    xEventGroupWaitBits(eventGroup, allBits, pdTRUE, pdTRUE, portMAX_DELAY);

    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      SensorData dataToSend = sharedData;
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &dataToSend, sizeof(dataToSend)); // Envio dos dados
      if (result == ESP_OK) {
        Serial.println("Enviado com Sucesso!");
      } else {
        Serial.println("Erro ao enviar o pacote!");
      }
      xSemaphoreGive(mutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void initialize_Comunication() {
  // Inicialização da transmissão
  WiFi.mode(WIFI_STA); // COnfigurando o ESP no modo de estação

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Erro ao Inicializar ESP-NOW!");
    return;
  }
  
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Falha no pareamento");
    return;
  }
}

// Nova tarefa de controle de movimento
void movementTask(void *pvParameters) {
  while (true) {
    float currentDistance;
    if (xSemaphoreTake(mutex, portMAX_DELAY)) {
      currentDistance = sharedData.distance;
      xSemaphoreGive(mutex);
    }

    if (currentDistance < 20 && currentDistance > 0) {
      stopMotors();
      vTaskDelay(pdMS_TO_TICKS(500));
      turnRight();
      vTaskDelay(pdMS_TO_TICKS(800)); 
      stopMotors();
      vTaskDelay(pdMS_TO_TICKS(500));
      moveForward();
    } else {
      moveForward();
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Funções de controle do motor (adicione após as definições de pinos)
void moveForward() {
  ledcWrite(0, 220);
  ledcWrite(1, 220);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void moveBackward() {
  ledcWrite(0, 220);
  ledcWrite(1, 220);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  ledcWrite(0, 220);
  ledcWrite(1, 220);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  ledcWrite(0, 220);
  ledcWrite(1, 220);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotors() {
  ledcWrite(0, 0);
  ledcWrite(1, 0);
}