#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

int sensor_data = 0; // Dado lido do sensor
int connected = 1;   // Estado da conexão (1 = conectado, 0 = desconectado)

pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

void* read_sensor(void* arg) {
    while (1) {
        int value = rand() % 101; // Valor entre 0 e 100

        pthread_mutex_lock(&data_mutex);
        sensor_data = value;
        pthread_mutex_unlock(&data_mutex);

        printf("[Sensor] Leitura do sensor: %d\n", value);
        sleep(1); // Leitura a cada 1 segundo
    }
    return NULL;
}

void* send_data_to_server(void* arg) {
    while (1) {
        sleep(2); // Tenta enviar dados a cada 2 segundos

        // Simula falha de conexão (30% de chance)
        int fail = rand() % 10; // 0 a 9

        if (fail < 3) {
            connected = 0;
            printf("[Network] Falha na conexão. Tentando reconectar...\n");
            sleep(2); // Simula tempo para reconectar
            connected = 1;
            printf("[Network] Reconectado com sucesso.\n");
        }

        if (connected) {
            pthread_mutex_lock(&data_mutex);
            int data = sensor_data;
            pthread_mutex_unlock(&data_mutex);

            printf("[Network] Dados enviados ao servidor: %d\n", data);
        }
    }
    return NULL;
}

int main() {
    srand(time(NULL)); // Semente para números aleatórios

    pthread_t sensor_thread, network_thread;

    pthread_create(&sensor_thread, NULL, read_sensor, NULL);
    pthread_create(&network_thread, NULL, send_data_to_server, NULL);

    pthread_join(sensor_thread, NULL);
    pthread_join(network_thread, NULL);

    return 0;
}
