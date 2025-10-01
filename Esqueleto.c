#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// Variáveis globais compartilhadas
int gasLevel = 0;       // ppm do gás
int presence = 0;       // 0 = ausente, 1 = presente
int connected = 1;      // Estado da conexão: 1 = conectado, 0 = desconectado
pthread_mutex_t lock;

// Thread 1: Leitura do sensor de gases
void* thread_gas(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        gasLevel = rand() % 600; // valores aleatórios até 600 ppm
        printf("[Sensor Gás] Nível atual: %d ppm\n", gasLevel);
        pthread_mutex_unlock(&lock);
        sleep(2); // simula tempo entre leituras
    }
    return NULL;
}

// Thread 2: Leitura do sensor de presença
void* thread_presence(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        presence = rand() % 2; // 0 ou 1
        printf("[Sensor Presença] Pessoa detectada: %s\n", presence ? "SIM" : "NÃO");
        pthread_mutex_unlock(&lock);
        sleep(3); // simula tempo entre leituras
    }
    return NULL;
}

// Thread 3: Comunicação e alarme
void* thread_comm_alarm(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        if (gasLevel > 400 && presence == 1) {
            printf("⚠ ALARME: Gás acima de 400 ppm e presença detectada! LED VERMELHO + SOM ⚠\n");
        } else {
            printf("[Rede] Enviando dados: Gás = %d ppm, Presença = %d\n", gasLevel, presence);
        }
        pthread_mutex_unlock(&lock);
        sleep(5); // envia dados periodicamente
    }
    return NULL;
}

// Thread 4: Função que simula envio de dados para o servidor com reconexão automática
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
            pthread_mutex_lock(&lock);
            int data = gasLevel;
            pthread_mutex_unlock(&lock);

            // Simula o envio para o servidor
            printf("[Network] Dados enviados ao servidor: %d\n", data);
        }
    }

    return NULL;
}

int main() {
    srand(time(NULL));
    pthread_t t1, t2, t3, t4;

    pthread_mutex_init(&lock, NULL);

    // Criar threads
    pthread_create(&t1, NULL, thread_gas, NULL);
    pthread_create(&t2, NULL, thread_presence, NULL);
    pthread_create(&t3, NULL, thread_comm_alarm, NULL);
    pthread_create(&t4, NULL, thread_comm_alarm, NULL);

    // Aguardar threads (nesse caso elas rodam infinitamente)
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    pthread_mutex_destroy(&lock);

    return 0;
}
