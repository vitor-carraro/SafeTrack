#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// Variável global para armazenar o valor do sensor
int sensor_data = 0;

// Estado da conexão: 1 = conectado, 0 = desconectado
int connected = 1;

// Mutex para proteger o acesso à variável sensor_data
pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

// Função que simula a leitura de um sensor
void* read_sensor(void* arg) {
    while (1) {
        int value = rand() % 101; // Número aleatório entre 0 e 100

        // Bloqueia o mutex antes de atualizar a variável compartilhada
        pthread_mutex_lock(&data_mutex);
        sensor_data = value;
        pthread_mutex_unlock(&data_mutex); // Libera o mutex

        printf("[Sensor] Leitura do sensor: %d\n", value);
        sleep(1); // Aguarda 1 segundo
    }

    return NULL;
}

// Função que simula envio de dados para o servidor com reconexão automática
void* send_data_to_server(void* arg) {
    while (1) {
        sleep(2); // Aguarda 2 segundos entre os envios

        // Simula 30% de chance de falha de conexão
        int fail = rand() % 10;
        if (fail < 3) {
            connected = 0;
            printf("[Network] Falha na conexão. Tentando reconectar...\n");
            sleep(2); // Tempo de reconexão
            connected = 1;
            printf("[Network] Reconectado com sucesso.\n");
        }

        if (connected) {
            // Acessa o valor do sensor de forma segura com mutex
            pthread_mutex_lock(&data_mutex);
            int data = sensor_data;
            pthread_mutex_unlock(&data_mutex);

            // Simula o envio para o servidor
            printf("[Network] Dados enviados ao servidor: %d\n", data);
        }
    }

    return NULL;
}

int main() {
    srand(time(NULL)); // Inicializa o gerador de números aleatórios

    pthread_t sensor_thread, network_thread;

    // Criação das threads
    pthread_create(&sensor_thread, NULL, read_sensor, NULL);
    pthread_create(&network_thread, NULL, send_data_to_server, NULL);

    // Espera as threads terminarem (nunca ocorrerá pois os loops são infinitos)
    pthread_join(sensor_thread, NULL);
    pthread_join(network_thread, NULL);

    // o mutex (não será alcançado neste exemplo)
    pthread_mutex_destroy(&data_mutex);

    return 0;
}
