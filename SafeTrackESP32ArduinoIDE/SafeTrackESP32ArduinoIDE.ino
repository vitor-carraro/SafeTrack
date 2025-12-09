#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ----------------------
// Pinos
// ----------------------
#define PIN_PRESENCA   14
#define PIN_GAS        34
#define PIN_LED        26
#define PIN_BUZZER     27

// ----------------------
// WiFi / MQTT
// ----------------------
const char* WIFI_SSID = "WAN_FM_VP";
const char* WIFI_PASS = "fmvp2312";

const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT = 1883;

// Tópicos
const char* TOPIC_SENSOR = "safetrack/sensor";
const char* TOPIC_LOG    = "safetrack/log";

WiFiClient espClient;
PubSubClient mqtt(espClient);

// ----------------------
// Estruturas e sincronização
// ----------------------
typedef struct {
  int gasPPM;
  bool presenca;
  uint32_t timestamp;
} SensorState_t;

SensorState_t sharedState;

SemaphoreHandle_t mutexState;
SemaphoreHandle_t logQueueSem;

#define LOG_QUEUE_LEN 10
String logQueue[LOG_QUEUE_LEN];
int logWriteIdx = 0;
int logReadIdx  = 0;

bool pushLog(const String& msg) {
  int next = (logWriteIdx + 1) % LOG_QUEUE_LEN;
  if (next == logReadIdx) return false; // fila cheia
  logQueue[logWriteIdx] = msg;
  logWriteIdx = next;
  xSemaphoreGive(logQueueSem);
  return true;
}

bool popLog(String& out) {
  if (logReadIdx == logWriteIdx) return false; // vazia
  out = logQueue[logReadIdx];
  logReadIdx = (logReadIdx + 1) % LOG_QUEUE_LEN;
  return true;
}

// ----------------------
// Funções auxiliares
// ----------------------
int readGasPPM() {
  int raw = analogRead(PIN_GAS);
  float ppm = (raw / 4095.0f) * 1000.0f;
  return (int)ppm;
}

void applyAlert(int gas, bool pres) {
  if (gas > 400 && pres) {
    digitalWrite(PIN_LED, HIGH);
    tone(PIN_BUZZER, 1000);
  } else {
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
  }
}

// ----------------------
// Tarefas
// ----------------------
void WifiTask(void *pv) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
      }
      pushLog("WiFi conectado.");
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void SensorTask(void *pv) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    bool pres = !digitalRead(PIN_PRESENCA);
    int gas   = readGasPPM();
    uint32_t ts = millis();

    if (xSemaphoreTake(mutexState, pdMS_TO_TICKS(20))) {
      sharedState.gasPPM = gas;
      sharedState.presenca = pres;
      sharedState.timestamp = ts;
      xSemaphoreGive(mutexState);
    }

    pushLog("SensorTask: gas=" + String(gas) + " pres=" + String(pres));

    vTaskDelayUntil(&last, pdMS_TO_TICKS(200));
  }
}

void AlertTask(void *pv) {
  for (;;) {
    if (xSemaphoreTake(mutexState, pdMS_TO_TICKS(20))) {
      int gas = sharedState.gasPPM;
      bool pres = sharedState.presenca;
      xSemaphoreGive(mutexState);

      applyAlert(gas, pres);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void MqttTask(void *pv) {
  TickType_t last = xTaskGetTickCount();

  for (;;) {
    if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
      pushLog("MQTT: tentando conectar...");
      if (mqtt.connect("SafeTrackDevice")) {
        pushLog("MQTT conectado.");
      }
    }

    if (mqtt.connected()) {
      SensorState_t st;
      if (xSemaphoreTake(mutexState, pdMS_TO_TICKS(20))) {
        st = sharedState;
        xSemaphoreGive(mutexState);
      }

      String json = "{";
      json += "\"gas_ppm\": " + String(st.gasPPM) + ",";
      json += "\"presenca\": " + String(st.presenca ? 1 : 0) + ",";
      json += "\"timestamp\": " + String(st.timestamp);
      json += "}";

      mqtt.publish(TOPIC_SENSOR, json.c_str());
      String logMsg;
      while (popLog(logMsg)) {
          mqtt.publish(TOPIC_LOG, logMsg.c_str());
      }
      pushLog("MQTT enviado: " + json);
      mqtt.loop();
    }

    vTaskDelayUntil(&last, pdMS_TO_TICKS(100));
  }
}

void LogTask(void *pv) {
  for (;;) {
    if (xSemaphoreTake(logQueueSem, pdMS_TO_TICKS(200))) {
      String msg;
      while (popLog(msg)) {
        Serial.println("[LOG] " + msg);
      }
    }
  }
}

// ----------------------
// Setup
// ----------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_PRESENCA, INPUT);
  pinMode(PIN_GAS, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  mutexState = xSemaphoreCreateMutex();
  logQueueSem = xSemaphoreCreateBinary();

  WiFi.mode(WIFI_STA);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  xTaskCreatePinnedToCore(WifiTask,   "WifiTask",   4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(SensorTask, "SensorTask", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(AlertTask,  "AlertTask",  4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(MqttTask,   "MqttTask",   4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(LogTask,    "LogTask",    4096, NULL, 1, NULL, 0);

  pushLog("Sistema iniciado.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}