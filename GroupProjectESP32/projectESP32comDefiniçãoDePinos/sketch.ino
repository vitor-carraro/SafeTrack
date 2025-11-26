#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ============================
// 🔧 Definições de pinos
// ============================
#define GAS_SENSOR_PIN 34        // Pino analógico (ex: MQ-2, MQ-135, etc.)
#define PRESENCE_SENSOR_PIN 27   // Pino digital (ex: sensor PIR HC-SR501)
#define LED_PIN 2                // LED (pode usar o LED onboard do ESP32)

// ============================
// 🔢 Variáveis globais
// ============================
int gasLevel = 0;
int presence = 0;
int connected = 1;
SemaphoreHandle_t semaforo;

// ============================
// 🧭 Task 1: Leitura do sensor de gás
// ============================
void task_gas(void *pvParameter) {
  while (1) {
    if (xSemaphoreTake(semaforo, portMAX_DELAY)) {
      gasLevel = analogRead(GAS_SENSOR_PIN);  // Leitura analógica real
      Serial.printf("[Sensor Gás] Nível atual: %d\n", gasLevel);
      xSemaphoreGive(semaforo);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// ============================
// 👁️ Task 2: Leitura do sensor de presença
// ============================
void task_presence(void *pvParameter) {
  while (1) {
    if (xSemaphoreTake(semaforo, portMAX_DELAY)) {
      presence = digitalRead(PRESENCE_SENSOR_PIN);  // Leitura digital real
      Serial.printf("[Sensor Presença] Pessoa detectada: %s\n", presence ? "SIM" : "NÃO");
      xSemaphoreGive(semaforo);
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

// ============================
// 🚨 Task 3: Comunicação e alarme
// ============================
void task_comm_alarm(void *pvParameter) {
  while (1) {
    if (xSemaphoreTake(semaforo, portMAX_DELAY)) {
      if (gasLevel > 2000 && presence == 1) {  // limiar de exemplo
        Serial.println("⚠️ ALARME: Gás acima do limite e presença detectada!");
        digitalWrite(LED_PIN, HIGH); // Liga o LED
      } else {
        Serial.printf("[Rede] Enviando dados: Gás = %d, Presença = %d\n", gasLevel, presence);
        digitalWrite(LED_PIN, LOW);  // Desliga o LED
      }
      xSemaphoreGive(semaforo);
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// ============================
// 🌐 Task 4: Simulação de rede
// ============================
void task_network(void *pvParameter) {
  while (1) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    int fail = random(0, 10);
    if (fail < 3) {
      connected = 0;
      Serial.println("[Network] Falha na conexão. Tentando reconectar...");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      connected = 1;
      Serial.println("[Network] Reconectado com sucesso.");
    }

    if (connected) {
      if (xSemaphoreTake(semaforo, portMAX_DELAY)) {
        int data = gasLevel;
        xSemaphoreGive(semaforo);
        Serial.printf("[Network] Dados enviados ao servidor: %d ppm\n", data);
      }
    }
  }
}

// ============================
// ⚙️ Configuração inicial
// ============================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Configuração dos pinos
  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(PRESENCE_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  // Inicializa semáforo
  semaforo = xSemaphoreCreateMutex();

  // Criação das tasks FreeRTOS
  xTaskCreatePinnedToCore(task_gas, "task_gas", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(task_presence, "task_presence", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(task_comm_alarm, "task_comm_alarm", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_network, "task_network", 2048, NULL, 1, NULL, 1);

  Serial.println("✅ Sistema iniciado com sensores reais e LED");
}

// ============================
// 🔁 Loop principal (vazio)
// ============================
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
