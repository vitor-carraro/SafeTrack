#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ----------------------
// Config Real Time
// ----------------------
#define NTP_SERVER      "pool.ntp.org"
#define TIMEZONE_OFFSET -3 * 3600
#define DST_OFFSET      0

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
  String dtLog;
} SensorState_t;

SensorState_t sharedState;

SemaphoreHandle_t mutexState;
SemaphoreHandle_t logQueueSem;
SemaphoreHandle_t backupMutex;

#define LOG_QUEUE_LEN 10
String logQueue[LOG_QUEUE_LEN];
int logWriteIdx = 0;
int logReadIdx  = 0;

String logBackUpQueue[LOG_QUEUE_LEN];
int logWriteBackUpIdx = 0;
int logReadBackUpIdx  = 0;

bool backupQueueIsFull() {
  int next = (logWriteBackUpIdx + 1) % LOG_QUEUE_LEN;
  return (next == logReadBackUpIdx);
}

bool backupQueueHasData() {
  return (logWriteBackUpIdx != logReadBackUpIdx);
}

bool pushBackupLog(const String& msg) {
  int next = (logWriteBackUpIdx + 1) % LOG_QUEUE_LEN;
  if (backupQueueIsFull()) {
    // fila cheia → sobrescreve o mais antigo
    logReadBackUpIdx = (logReadBackUpIdx + 1) % LOG_QUEUE_LEN;
  }
  logBackUpQueue[logWriteBackUpIdx] = msg;
  logWriteBackUpIdx = next;
  return true;
}

bool popBackupLog(String& out) {
  if (!backupQueueHasData()) return false;
  out = logBackUpQueue[logReadBackUpIdx];
  logReadBackUpIdx = (logReadBackUpIdx + 1) % LOG_QUEUE_LEN;
  return true;
}

bool pushLog(const String& msg) {
  int next = (logWriteIdx + 1) % LOG_QUEUE_LEN;
  if (next == logReadIdx) return false;
  logQueue[logWriteIdx] = msg;
  logWriteIdx = next;
  xSemaphoreGive(logQueueSem);
  return true;
}

bool popLog(String& out) {
  if (logReadIdx == logWriteIdx) return false;
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

String getDateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01 00:00:00";
  }

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ----------------------
// Tarefas FreeRTOS
// ----------------------
void WifiTask(void *pv) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if(xSemaphoreTake(backupMutex, portMAX_DELAY)){
        pushBackupLog(getDateTime() + " [LOG] WiFi desconectado. Tentando reconectar...");
        xSemaphoreGive(backupMutex);
      }
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void SensorTask(void *pv) {
  TickType_t last = xTaskGetTickCount();
  for (;;) {
    bool pres = !digitalRead(PIN_PRESENCA);
    int gas   = readGasPPM();
    String dt = getDateTime();

    if (xSemaphoreTake(mutexState, pdMS_TO_TICKS(20))) {
      sharedState.gasPPM = gas;
      sharedState.presenca = pres;
      sharedState.dtLog = dt;
      xSemaphoreGive(mutexState);
    }

    String strLog = getDateTime() + "[LOG] SensorTask: gas=" + String(gas) + " presença=" + String(pres);
    pushLog(strLog);
    Serial.println(strLog);

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
  String strLog;
  String logMsg;
  for (;;) {
    strLog = "";
    if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
      strLog = getDateTime() + " [LOG] MQTT: tentando conectar...";
      pushLog(strLog);
      Serial.println(strLog);
      if (mqtt.connect("SafeTrackDevice")) {
        strLog = getDateTime() + "[LOG] MQTT conectado.";
        pushLog(strLog);
        Serial.println(strLog);
      }
    }

    logMsg = "";
    if (mqtt.connected()) {
      SensorState_t stCurrent;

      if (xSemaphoreTake(mutexState, pdMS_TO_TICKS(20))) {
        stCurrent = sharedState;
        xSemaphoreGive(mutexState);
      }

      String json = "{";
      json += "\"gas_ppm\": " + String(stCurrent.gasPPM) + ",";
      json += "\"presenca\": " + String(stCurrent.presenca ? 1 : 0) + ",";
      json += "\"timestamp\": \"" + stCurrent.dtLog + "\"";
      json += "}";

      mqtt.publish(TOPIC_SENSOR, json.c_str());

      while (popLog(logMsg)) {
        mqtt.publish(TOPIC_LOG, logMsg.c_str());
      }
    }

    // Enviar backup se a fila estiver cheia ou com dados
    if (mqtt.connected() && backupQueueHasData()) {
      String json = "[";  // manda como array JSON
      String item;
      bool first = true;
      
      if(xSemaphoreTake(backupMutex, portMAX_DELAY)){
        while (popBackupLog(item)) {
          if (!first) json += ",";
          json += "\"" + item + "\"";
          first = false;
        }
        xSemaphoreGive(backupMutex);
      }
      
      json += "]";

      mqtt.publish(TOPIC_LOG, json.c_str());

      Serial.println("Backup enviado ao broker: " + json);
    }

    mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ----------------------
// NTP
// ----------------------
void initTime() {
  Serial.print("Sincronizando NTP");
  configTime(TIMEZONE_OFFSET, DST_OFFSET, NTP_SERVER);

  struct tm timeinfo;
  uint32_t start = millis();

  while (!getLocalTime(&timeinfo)) {
    if (millis() - start > 10000) {
      pushBackupLog("[LOG] Falha NTP. Prosseguindo sem RTC");
      Serial.println("\nFalha NTP. Prosseguindo sem RTC.");
      return;
    }
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nHora sincronizada.");
}

// ----------------------
// Setup
// ----------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  //PIN Config
  pinMode(PIN_PRESENCA, INPUT);
  pinMode(PIN_GAS, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  //WiFi config
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi conectado.");

  initTime(); 

  mqtt.setServer(MQTT_HOST, MQTT_PORT);

  //Task config
  mutexState = xSemaphoreCreateMutex();
  logQueueSem = xSemaphoreCreateBinary();
  backupMutex = xSemaphoreCreateMutex();

  xTaskCreatePinnedToCore(WifiTask,   "WifiTask",   4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(SensorTask, "SensorTask", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(AlertTask,  "AlertTask",  4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(MqttTask,   "MqttTask",   4096, NULL, 2, NULL, 1);

  pushLog("Sistema iniciado.");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}