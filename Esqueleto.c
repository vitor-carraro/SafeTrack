#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

// Variáveis globais compartilhadas
int gasLevel = 0;       // ppm do gás
int presence = 0;       // 0 = ausente, 1 = presente
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

int main() {
    srand(time(NULL));
    pthread_t t1, t2, t3;

    pthread_mutex_init(&lock, NULL);

    // Criar threads
    pthread_create(&t1, NULL, thread_gas, NULL);
    pthread_create(&t2, NULL, thread_presence, NULL);
    pthread_create(&t3, NULL, thread_comm_alarm, NULL);

    // Aguardar threads (nesse caso elas rodam infinitamente)
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    pthread_mutex_destroy(&lock);

    return 0;
}
