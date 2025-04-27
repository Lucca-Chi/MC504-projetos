#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 3
#define N_COOKS     2
#define N_CLIENTS   4
#define MIN_PREP_TIME 1
#define MAX_PREP_TIME 4
#define MIN_CONS_TIME 2
#define MAX_CONS_TIME 10

#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Buffer
typedef enum {IDLE, PRODUZINDO, CONSUMINDO} Status;
int buffer[BUFFER_SIZE];
int in = 0, out = 0;

// Sincronização
sem_t empty, full;
pthread_mutex_t mutex, print_mutex;

// Status e prato atual para visualização
Status cook_status[N_COOKS];
Status client_status[N_CLIENTS];
int cook_current[N_COOKS];
int client_current[N_CLIENTS];

void imprime_estado_global() {
    pthread_mutex_lock(&print_mutex);
    printf("\033[2J\033[H");
    printf("🍽️  RESTAURANTE CODEATS - RODÍZIO\n\n");
    // Buffer
    printf("Pratos em espera: ");
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (buffer[i] == -1) printf("[    ] ");
        else               printf("[ P%d ] ", buffer[i]);
    }
    printf("\n\n");
    // Cozinheiros
    printf("Cozinheiros: ");
    for (int i = 0; i < N_COOKS; i++) {
        if (cook_status[i] == PRODUZINDO)
            printf("C%d(cozinhando P%d)  ", i, cook_current[i]);
        else
            printf("C%d(esperando)  ", i);
    }
    printf("\n");
    // Clientes
    printf("Clientes:    ");
    for (int i = 0; i < N_CLIENTS; i++) {
        if (client_status[i] == CONSUMINDO)
            printf("K%d(comendo P%d)  ", i, client_current[i]);
        else
            printf("K%d(esperando)  ", i);
    }
    printf("\n");
    fflush(stdout);
    pthread_mutex_unlock(&print_mutex);
}

int gera_prato() {
    static int next = 0;
    return next++;
}

void *cozinheiro(void *arg) {
    int id = (intptr_t)arg;
    while (1) {
        // Produz o prato
        cook_status[id]   = PRODUZINDO;
        int prato         = gera_prato();
        cook_current[id]  = prato;
        sleep(MIN(MIN_PREP_TIME, (rand() % MAX_PREP_TIME + 1)));  // Tempo de preparo

        // Depois de cozinhar, fica idle enquanto espera espaço no buffer
        cook_status[id]   = IDLE;
        cook_current[id]  = -1;
        imprime_estado_global();

        // Espera por slot vazio
        sem_wait(&empty);

        // Insere no buffer
        pthread_mutex_lock(&mutex);
            buffer[in] = prato;
            in = (in + 1) % BUFFER_SIZE;
            imprime_estado_global();
        pthread_mutex_unlock(&mutex);

        // Acorda os consumidores
        sem_post(&full);
    }
    return NULL;
}

void *cliente(void *arg) {
    int id = (intptr_t)arg;
    while (1) {
        // Espera por prato disponível
        sem_wait(&full);

        pthread_mutex_lock(&mutex);
            int prato = buffer[out];
            buffer[out] = -1;
            out = (out + 1) % BUFFER_SIZE;
            client_status[id] = CONSUMINDO;
            client_current[id] = prato;
        pthread_mutex_unlock(&mutex);
        sem_post(&empty);

        imprime_estado_global();

        // Simula tempo de consumo
        sleep(MIN(MIN_CONS_TIME, (rand() % MAX_CONS_TIME + 1)));

        client_status[id] = IDLE;
        imprime_estado_global();
    }
    return NULL;
}


int main() {
    srand(time(NULL));

    // Inicializa buffer e estados
    for (int i = 0; i < BUFFER_SIZE; i++) buffer[i] = -1;
    for (int i = 0; i < N_COOKS;    i++) { cook_status[i] = IDLE; cook_current[i] = -1; }
    for (int i = 0; i < N_CLIENTS;  i++) { client_status[i] = IDLE; client_current[i] = -1; }

    // Inicializa semáforos e mutexes
    sem_init(&empty, 0, BUFFER_SIZE);
    sem_init(&full,  0, 0);
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);

    // Cria threads cozinheiros e clientes
    pthread_t cooks[N_COOKS], clients[N_CLIENTS];
    for (int i = 0; i < N_COOKS;   i++)
        pthread_create(&cooks[i],   NULL, cozinheiro, (void*)(intptr_t)i);
    for (int i = 0; i < N_CLIENTS; i++)
        pthread_create(&clients[i], NULL, cliente,    (void*)(intptr_t)i);

    // Aguarda threads
    for (int i = 0; i < N_COOKS;   i++) pthread_join(cooks[i],   NULL);
    for (int i = 0; i < N_CLIENTS; i++) pthread_join(clients[i], NULL);

    // // Limpeza (desnecessário se o programa for encerrado só fechando o terminal)
    // sem_destroy(&empty);
    // sem_destroy(&full);
    // pthread_mutex_destroy(&mutex);
    // pthread_mutex_destroy(&print_mutex);

    return 0;
}
