#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

// Variáveis globais compartilhadas
int gasLevel = 0;       // ppm do gás
int presence = 0;       // 0 = ausente, 1 = presente
int connected = 1;      // Estado da conexão: 1 = conectado, 0 = desconectado
sem_t semaforo;         // Semáforo para controlar acesso às variáveis

// Thread 1: Leitura do sensor de gases
void* thread_gas(void* arg) {
    while (1) {
        sem_wait(&semaforo);
        gasLevel = rand() % 600; // valores aleatórios até 600 ppm
        printf("[Sensor Gás] Nível atual: %d ppm\n", gasLevel);
        sem_post(&semaforo);
        sleep(2);
    }
    return NULL;
}

// Thread 2: Leitura do sensor de presença
void* thread_presence(void* arg) {
    while (1) {
        sem_wait(&semaforo);
        presence = rand() % 2; // 0 ou 1
        printf("[Sensor Presença] Pessoa detectada: %s\n", presence ? "SIM" : "NÃO");
        sem_post(&semaforo);
        sleep(3);
    }
    return NULL;
}

// Thread 3: Comunicação e alarme
void* thread_comm_alarm(void* arg) {
    while (1) {
        sem_wait(&semaforo);
        if (gasLevel > 400 && presence == 1) {
            printf("⚠ ALARME: Gás acima de 400 ppm e presença detectada! LED VERMELHO + SOM ⚠\n");
        } else {
            printf("[Rede] Enviando dados: Gás = %d ppm, Presença = %d\n", gasLevel, presence);
        }
        sem_post(&semaforo);
        sleep(5);
    }
    return NULL;
}

// Thread 4: Simula envio de dados com reconexão automática
void* send_data_to_server(void* arg) {
    while (1) {
        sleep(2);

        int fail = rand() % 10;
        if (fail < 3) {
            connected = 0;
            printf("[Network] Falha na conexão. Tentando reconectar...\n");
            sleep(2);
            connected = 1;
            printf("[Network] Reconectado com sucesso.\n");
        }

        if (connected) {
            sem_wait(&semaforo);
            int data = gasLevel;
            sem_post(&semaforo);

            printf("[Network] Dados enviados ao servidor: %d\n", data);
        }
    }
    return NULL;
}

int main() {
    srand(time(NULL));
    pthread_t t1, t2, t3, t4;

    sem_init(&semaforo, 0, 1); // inicializa semáforo binário (1 = livre)

    // Cria threads
    pthread_create(&t1, NULL, thread_gas, NULL);
    pthread_create(&t2, NULL, thread_presence, NULL);
    pthread_create(&t3, NULL, thread_comm_alarm, NULL);
    pthread_create(&t4, NULL, send_data_to_server, NULL);

    // Aguardar threads
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    sem_destroy(&semaforo);

    return 0;
}
