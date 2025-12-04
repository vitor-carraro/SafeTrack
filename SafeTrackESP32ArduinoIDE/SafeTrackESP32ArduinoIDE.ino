#include <Arduino.h>

// ----------------------
// CONFIGURAÇÕES FÍSICAS
// ----------------------
#define PIN_PRESENCA   14   // Sensor PIR
#define PIN_GAS        34   // Sensor de gás (entrada analógica)
#define PIN_LED        26
#define PIN_BUZZER     27

// ----------------------
// Funções auxiliares
// ----------------------

int readGasPPM() {
    int raw = analogRead(PIN_GAS);
    float ppm = (raw / 4095.0) * 1000.0;   // Conversão aproximada
    return (int)ppm;
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

    Serial.println("Sistema embarcado — modo protótipo (sem WiFi).");
}

// ----------------------
// LOOP PRINCIPAL
// ----------------------
void loop() {
    bool presenca = ! digitalRead(PIN_PRESENCA);
    int gasPPM = readGasPPM();

    alerta(gasPPM, presenca);

    Serial.print("Gás: ");
    Serial.print(gasPPM);
    Serial.print(" ppm | Presença: ");
    Serial.println(presenca ? "SIM" : "NAO");

    delay(300);
}