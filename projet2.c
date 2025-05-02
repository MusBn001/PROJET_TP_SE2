/*
 *Projet totalement commenté 
 * Algiers University Benyoucef Benkhedda
 * Faculty of Science
 * Department of Computer Science
 * Course: Operating Systems 2 (SE2)
 * Level: 3rd year Computer Science ISIL
 * Academic year: 2024/2025
 * Projet : Synchronisation des Bus de Transport dans un Tunnel
 * Date limite : 05/02/2025 22:00
 * Total Points: 12
 *
 * Contexte :
 *   - Deux villes X et Y reliées par un tunnel à voie unique.
 *   - X dispose de 5 bus, Y en dispose de 4.
 *   - Règles :
 *     • Pas de croisement (exclusion mutuelle des sens opposés)
 *     • Circulation groupée (plusieurs bus même sens simultanément)
 *     • Équité (aucun sens ne monopolise le tunnel)
 *     • Chaque bus fait 10 allers-retours par jour.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// Identification des directions
#define VILLE_X 1      // Sens X -> Y
#define VILLE_Y 2      // Sens Y -> X

// Paramétrage du nombre de bus et de trajets
#define NB_BUS_X 5     // Nombre de bus partant de X
#define NB_BUS_Y 4     // Nombre de bus partant de Y
#define NB_TRAJETS 10  // Nombre d'allers-retours par bus

// ===== Variables de synchronisation =====
pthread_mutex_t mutex;      // Protège l'accès aux compteurs et files d'attente
sem_t sem_X, sem_Y;         // Sémaphores pour bloquer / réveiller chaque direction
int count_X = 0, count_Y = 0;       // Compteurs de bus actuellement dans le tunnel
int waiting_X = 0, waiting_Y = 0;   // Compteurs de bus en attente à l'entrée
int current_direction = 0; // 0=tunnel libre, 1=X->Y, 2=Y->X
int last_direction = 0;    // Pour assurer l'alternance équitable des sens

// Mutex dédié à l'affichage pour éviter le chevauchement de printf
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure d'argument passé à chaque thread (bus)
typedef struct {
    int id;                // Identifiant unique du bus
    int ville;             // Ville de départ initiale (X ou Y)
    unsigned int seed;     // Graine pour rand_r (sécurité thread-safe)
} BusArg;

// -------- Fonction d'attente aléatoire --------
// Simule la durée de traversée du tunnel (1s à 1,5s)
void attendre(unsigned int *seed) {
    // rand_r génère un entier entre 0 et RAND_MAX
    // on prend modulo 500, puis +1000 pour obtenir [1000,1500] ms
    usleep((rand_r(seed) % 500 + 1000) * 1000);
}

// -------- Entrée dans le tunnel (sens direction) --------
void entrer_tunnel(int ville, unsigned int *seed) {
    pthread_mutex_lock(&mutex);

    if (ville == VILLE_X) {
        waiting_X++;  // Un bus X se met en attente
        // Boucle de blocage tant que l'autre sens est actif
        while (1) {
            // Si tunnel occupé par Y, ou libre mais Y en attente et priorité alternée
            if (current_direction == VILLE_Y ||
               (current_direction == 0 && waiting_Y > 0 && last_direction != VILLE_X)) {
                pthread_mutex_unlock(&mutex);
                sem_wait(&sem_X);   // Blocage jusqu'à sem_post
                pthread_mutex_lock(&mutex);
            } else {
                break; // Autorisation d'entrée
            }
        }
        waiting_X--;    // Quitte la file d'attente
        count_X++;      // Compte le bus dans le tunnel
        current_direction = VILLE_X;
        last_direction = VILLE_X;

    } else {
        // Même logique pour les bus venant de Y
        waiting_Y++;
        while (1) {
            if (current_direction == VILLE_X ||
               (current_direction == 0 && waiting_X > 0 && last_direction != VILLE_Y)) {
                pthread_mutex_unlock(&mutex);
                sem_wait(&sem_Y);
                pthread_mutex_lock(&mutex);
            } else {
                break;
            }
        }
        waiting_Y--;
        count_Y++;
        current_direction = VILLE_Y;
        last_direction = VILLE_Y;
    }

    pthread_mutex_unlock(&mutex);
}

// -------- Sortie du tunnel (libération) --------
void sortir_tunnel(int ville) {
    pthread_mutex_lock(&mutex);

    if (ville == VILLE_X) {
        count_X--;  // Un bus X quitte le tunnel
        if (count_X == 0) {
            // Tunnel désormais libre
            current_direction = 0;
            if (waiting_Y > 0) {
                // Réveiller tous les Y en attente
                for (int i = 0; i < waiting_Y; i++) sem_post(&sem_Y);
            } else if (waiting_X > 0) {
                // Sinon réveiller tous les X
                for (int i = 0; i < waiting_X; i++) sem_post(&sem_X);
            }
        }
    } else {
        // Même logique pour les bus de Y quittant le tunnel
        count_Y--;
        if (count_Y == 0) {
            current_direction = 0;
            if (waiting_X > 0) {
                for (int i = 0; i < waiting_X; i++) sem_post(&sem_X);
            } else if (waiting_Y > 0) {
                for (int i = 0; i < waiting_Y; i++) sem_post(&sem_Y);
            }
        }
    }

    pthread_mutex_unlock(&mutex);
}

// -------- Fonction exécutée par chaque thread Bus --------
void* bus_thread(void* arg) {
    BusArg* bus = (BusArg*)arg;

    for (int i = 1; i <= NB_TRAJETS; i++) {
        // ----- Trajet Aller -----
        entrer_tunnel(bus->ville, &bus->seed);
        pthread_mutex_lock(&print_mutex);
        printf("🚌 Bus %d de %s : %s -> %s (Trajet Aller %d)\n",
              bus->id,
              (bus->ville == VILLE_X) ? "X" : "Y",
              (bus->ville == VILLE_X) ? "X" : "Y",
              (bus->ville == VILLE_X) ? "Y" : "X",
              i);
        pthread_mutex_unlock(&print_mutex);
        attendre(&bus->seed);
        sortir_tunnel(bus->ville);

        // ----- Trajet Retour -----
        int retour_ville = (bus->ville == VILLE_X) ? VILLE_Y : VILLE_X;
        entrer_tunnel(retour_ville, &bus->seed);
        pthread_mutex_lock(&print_mutex);
        printf("🚌 Bus %d de %s : %s -> %s (Trajet Retour %d)\n",
              bus->id,
              (bus->ville == VILLE_X) ? "X" : "Y",
              (retour_ville == VILLE_X) ? "X" : "Y",
              (retour_ville == VILLE_X) ? "Y" : "X",
              i);
        pthread_mutex_unlock(&print_mutex);
        attendre(&bus->seed);
        sortir_tunnel(retour_ville);
    }

    free(bus);   // Libération de la mémoire allouée
    return NULL;
}

int main() {
    pthread_t threads[NB_BUS_X + NB_BUS_Y];

    // Initialisation des primitives
    pthread_mutex_init(&mutex, NULL);
    sem_init(&sem_X, 0, 0);
    sem_init(&sem_Y, 0, 0);

    // Création des threads pour les bus de X
    for (int i = 0; i < NB_BUS_X; i++) {
        BusArg* arg = malloc(sizeof(BusArg));
        arg->id = i + 1;
        arg->ville = VILLE_X;
        arg->seed = time(NULL) ^ (i + 1); // Seed unique par thread
        pthread_create(&threads[i], NULL, bus_thread, arg);
    }

    // Création des threads pour les bus de Y
    for (int i = 0; i < NB_BUS_Y; i++) {
        BusArg* arg = malloc(sizeof(BusArg));
        arg->id = i + 1;
        arg->ville = VILLE_Y;
        arg->seed = time(NULL) ^ (i + 5);
        pthread_create(&threads[NB_BUS_X + i], NULL, bus_thread, arg);
    }

    // Attente de la fin de tous les threads
    for (int i = 0; i < NB_BUS_X + NB_BUS_Y; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destruction des primitives avant la sortie
    pthread_mutex_destroy(&mutex);
    sem_destroy(&sem_X);
    sem_destroy(&sem_Y);

    return 0;
}

