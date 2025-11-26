#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ====================================
// Definição de pinos (WOKWI)
// ====================================
#define GAS_SENSOR_PIN 34        
#define PRESENCE_SENSOR_PIN 27   
#define LED_PIN 2                

// ====================================
// Variáveis globais
// ====================================
int gasLevel = 0;
int presence = 0;
SemaphoreHandle_t semaforo;

// ====================================
// Config Wi-Fi (Wokwi)
// ====================================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

// ====================================
// Config MQTT (HiveMQ público)
// ====================================
const char* MQTT_BROKER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENTID = "esp32-safetrack-demo";
const char* MQTT_TOPIC    = "safetrack/dados";

// ====================================
// Objetos WiFi/MQTT
// ====================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ====================================
//  Conectar Wi-Fi
// ====================================
void connectWiFi() {
  Serial.println("🔌 Conectando ao WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(400);
  }

  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ====================================
//  Reconectar MQTT
// ====================================
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("📡 Conectando ao broker MQTT... ");

    if (mqttClient.connect(MQTT_CLIENTID)) {
      Serial.println("Conectado!");
      mqttClient.subscribe("safetrack/cmd");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" — tentando em 3s");
      delay(3000);
    }
  }
}

// ====================================
// Callback MQTT
// ====================================
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("MQTT recebido: ");
  Serial.print(topic);
  Serial.print(" => ");

  for (unsigned int i = 0; i < len; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// ====================================
// Task 1: Leitura do sensor de gás
// ====================================
void task_gas(void *pvParameter) {
  while (1) {
    if (xSemaphoreTake(semaforo, portMAX_DELAY)) {
      gasLevel = analogRead(GAS_SENSOR_PIN);
      Serial.printf("[GÁS] Valor: %d\n", gasLevel);
      xSemaphoreGive(semaforo);
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// ====================================
// Task 2: Leitura de presença
// ====================================
void task_presence(void *pvParameter) {
  while (1) {
    if (xSemaphoreTake(semaforo, portMAX_DELAY)) {
      presence = digitalRead(PRESENCE_SENSOR_PIN);
      Serial.printf("[PRESENÇA] Detectado: %s\n", presence ? "SIM" : "NÃO");
      xSemaphoreGive(semaforo);
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

// ====================================
// Task 3: Alarme + lógica local
// ====================================
void task_alarm(void *pvParameter) {
  while (1) {
    if (xSemaphoreTake(semaforo, portMAX_DELAY)) {

      if (gasLevel > 1800 && presence == 1) {
        Serial.println("ALERTA CRÍTICO! Gás alto + presença.");
        digitalWrite(LED_PIN, HIGH);
      } else {
        digitalWrite(LED_PIN, LOW);
      }

      xSemaphoreGive(semaforo);
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// ====================================
// Task 4: MQTT – Envio dos dados
// ====================================
void task_mqtt(void *pvParameter) {
  while (1) {

    if (!mqttClient.connected()) {
      reconnectMQTT();
    }

    mqttClient.loop();

    if (xSemaphoreTake(semaforo, portMAX_DELAY)) {
      int gas = gasLevel;
      int pres = presence;
      xSemaphoreGive(semaforo);

      // Monta JSON
      char payload[200];
      snprintf(payload, sizeof(payload),
        "{\"gas\": %d, \"presenca\": %d}", gas, pres);

      mqttClient.publish(MQTT_TOPIC, payload);

      Serial.printf("MQTT enviado: %s\n", payload);
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// ====================================
// Setup
// ====================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(GAS_SENSOR_PIN, INPUT);
  pinMode(PRESENCE_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  semaforo = xSemaphoreCreateMutex();

  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  // Tasks
  xTaskCreatePinnedToCore(task_gas,"task_gas",2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_presence,"task_presence", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_alarm,"task_alarm",2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(task_mqtt,"task_mqtt",4096, NULL, 1, NULL, 1);

  Serial.println("🚀 Sistema iniciado no Wokwi com MQTT");
}

// ====================================
// Loop
// ====================================
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
