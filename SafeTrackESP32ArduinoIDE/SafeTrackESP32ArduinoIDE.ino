#include <Arduino.h>

// ----------------------
// Pinos
// ----------------------
#define PIN_PRESENCA   14   // Sensor PIR (digital)
#define PIN_GAS        34   // Sensor de gás (analógico 0-4095)
#define PIN_LED        26
#define PIN_BUZZER     27

// ----------------------
// Parâmetros
// ----------------------
const TickType_t SENSOR_PERIOD = pdMS_TO_TICKS(200);   // 200 ms
const TickType_t ALERT_PERIOD  = pdMS_TO_TICKS(100);   // 100 ms (resposta rápida)
const TickType_t LOG_PERIOD    = pdMS_TO_TICKS(1000);  // 1 s

// ----------------------
// Estruturas compartilhadas
// ----------------------
typedef struct {
  int gasPPM;
  bool presenca;
  uint32_t timestampMs;
} SensorState_t;

static SensorState_t sharedState;

// Mutex para proteger sharedState
static SemaphoreHandle_t stateMutex = NULL;

// Optional: notificação para AlertTask (binary semaphore)
static SemaphoreHandle_t stateReadySem = NULL;

// ----------------------
// Funções utilitárias
// ----------------------
int readGasPPM() {
  int raw = analogRead(PIN_GAS);
  float ppm = (raw / 4095.0f) * 1000.0f; // conversão aproximada
  return (int)ppm;
}

void applyAlertOutputs(int gasPPM, bool presenca) {
  if (gasPPM > 400 && presenca) {
    digitalWrite(PIN_LED, HIGH);
    tone(PIN_BUZZER, 1000);
  } else {
    digitalWrite(PIN_LED, LOW);
    noTone(PIN_BUZZER);
  }
}

// ----------------------
// Tasks
// ----------------------
void SensorTask(void *pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    // Leitura sensores
    bool pres = !digitalRead(PIN_PRESENCA);
    int gas  = readGasPPM();
    uint32_t ts = millis();

    // Protege e atualiza estado compartilhado
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sharedState.gasPPM = gas;
      sharedState.presenca = pres;
      sharedState.timestampMs = ts;
      xSemaphoreGive(stateMutex);
    }
    // Notifica AlertTask que há novo estado
    xSemaphoreGive(stateReadySem);

    // Periodicidade estável
    vTaskDelayUntil(&lastWake, SENSOR_PERIOD);
  }

  vTaskDelete(NULL);
}

void AlertTask(void *pvParameters) {
  // Pode esperar pela notificação ou rodar periodicamente.
  for (;;) {
    // Espera até ser notificado (timeout para fallback)
    if (xSemaphoreTake(stateReadySem, ALERT_PERIOD) == pdTRUE) {
      // Há novo estado — pega mutex para ler
      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        int gas = sharedState.gasPPM;
        bool pres = sharedState.presenca;
        // opcional: uint32_t ts = sharedState.timestampMs;
        xSemaphoreGive(stateMutex);

        // Decide ação de alerta
        applyAlertOutputs(gas, pres);
      }
    } else {
      // Timeout — pode checar estado mesmo sem notificação
      if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        int gas = sharedState.gasPPM;
        bool pres = sharedState.presenca;
        xSemaphoreGive(stateMutex);
        applyAlertOutputs(gas, pres);
      }
    }
    // pequena espera para dar chance ao scheduler
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  vTaskDelete(NULL);
}

void LogTask(void *pvParameters) {
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    // Lê estado protegido e escreve no Serial
    if (xSemaphoreTake(stateMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      int gas = sharedState.gasPPM;
      bool pres = sharedState.presenca;
      uint32_t ts = sharedState.timestampMs;
      xSemaphoreGive(stateMutex);

      Serial.print("[LOG] ");
      Serial.print("TS=");
      Serial.print(ts);
      Serial.print(" ms | Gas=");
      Serial.print(gas);
      Serial.print(" ppm | Presenca=");
      Serial.println(pres ? "SIM" : "NAO");
    } else {
      Serial.println("[LOG] falha ao obter mutex.");
    }

    vTaskDelayUntil(&lastWake, LOG_PERIOD);
  }

  vTaskDelete(NULL);
}

// ----------------------
// Setup e criação de tasks
// ----------------------
void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(PIN_PRESENCA, INPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_GAS, INPUT);

  // Inicializa shared state
  sharedState.gasPPM = 0;
  sharedState.presenca = false;
  sharedState.timestampMs = millis();

  // Cria mutex e semáforo
  stateMutex = xSemaphoreCreateMutex();
  stateReadySem = xSemaphoreCreateBinary();

  if (stateMutex == NULL || stateReadySem == NULL) {
    Serial.println("Erro ao criar mutex/semáforo. Reinicie.");
    while (1) { delay(1000); }
  }

  // Limpa semáforo (garantia)
  xSemaphoreTake(stateReadySem, 0);

  // Cria tasks (pilha e prioridade ajustáveis)
  BaseType_t ok;
  ok = xTaskCreatePinnedToCore(
    SensorTask, "SensorTask", 3072, NULL, 2, NULL, 1);   // core 1 recomendado para sensores
  if (ok != pdPASS) Serial.println("Falha criar SensorTask");

  ok = xTaskCreatePinnedToCore(
    AlertTask, "AlertTask", 3072, NULL, 3, NULL, 1);     // prioridade maior para alerta
  if (ok != pdPASS) Serial.println("Falha criar AlertTask");

  ok = xTaskCreatePinnedToCore(
    LogTask, "LogTask", 4096, NULL, 1, NULL, 0);         // core 0 para logging/Serial
  if (ok != pdPASS) Serial.println("Falha criar LogTask");

  Serial.println("Tasks criadas. Sistema FreeRTOS ativo.");
}

void loop() {
  // O loop principal pode ficar vazio quando usamos FreeRTOS tasks.
  vTaskDelay(pdMS_TO_TICKS(1000));
}
