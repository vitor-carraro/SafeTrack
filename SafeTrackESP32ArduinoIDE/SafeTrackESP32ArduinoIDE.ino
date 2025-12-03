#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>   // Armazenamento local (24h)

// ----------------------
// CONFIGURAÇÕES
// ----------------------
#define PIN_PRESENCA     14   // Sensor PIR ou similar
#define PIN_GAS          34   // Entrada analógica (0-4095)
#define PIN_LED          26
#define PIN_BUZZER       27

const char* WIFI_SSID = "SUA_REDE";
const char* WIFI_PASS = "SUA_SENHA";

const char* IOT_ENDPOINT = "http://seu-servidor.com/api/sensores"; 

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 60000; // 1 minuto

Preferences prefs;  // memória NV

// ----------------------
// Funções auxiliares
// ----------------------
bool sendToServer(int gasPPM, bool presenca) {
    if (WiFi.status() != WL_CONNECTED)
        return false;

    HTTPClient http;
    http.begin(IOT_ENDPOINT);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"gas_ppm\": " + String(gasPPM) + ",";
    json += "\"presenca\": " + String(presenca ? 1 : 0);
    json += "}";

    int code = http.POST(json);
    http.end();

    return (code == 200 || code == 201);
}

int readGasPPM() {
    int raw = analogRead(PIN_GAS);
    float ppm = (raw / 4095.0) * 1000.0;  
    return (int)ppm;
}

void saveLocal(int gas, bool presenca) {
    prefs.putInt("gas", gas);
    prefs.putBool("presenca", presenca);
    prefs.putUInt("timestamp", millis());
}

bool hasLocalData() {
    return prefs.getUInt("timestamp", 0) != 0;
}

void resendLocalData() {
    if (!hasLocalData()) return;

    int gas = prefs.getInt("gas");
    bool presenca = prefs.getBool("presenca");

    if (sendToServer(gas, presenca)) {
        prefs.clear(); // Limpa após envio
    }
}

void alerta(int gasPPM, bool presenca) {
    if (gasPPM > 400 && presenca) {
        digitalWrite(PIN_LED, HIGH);
        tone(PIN_BUZZER, 1000);
    } else {
        digitalWrite(PIN_LED, LOW);
        noTone(PIN_BUZZER);
    }
}

// ----------------------
// SETUP
// ----------------------
void setup() {
    Serial.begin(115200);

    pinMode(PIN_PRESENCA, INPUT);
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    prefs.begin("safet", false);

    // Conecta no WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Conectando no WiFi");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado.");

    resendLocalData();
}

// ----------------------
// LOOP PRINCIPAL
// ----------------------
void loop() {
    bool presenca = digitalRead(PIN_PRESENCA);
    int gasPPM = readGasPPM();

    alerta(gasPPM, presenca);

    unsigned long now = millis();

    if (now - lastSend >= SEND_INTERVAL) {
        lastSend = now;

        bool ok = sendToServer(gasPPM, presenca);

        if (!ok) {
            saveLocal(gasPPM, presenca);
            Serial.println("Falha ao enviar. Salvo localmente.");
        } else {
            Serial.println("Dados enviados: " + String(gasPPM) + " ppm");
        }
    }

    delay(200);
}
