#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef enum { NO_WIN, CINQUINA, BINGO } winner_e;

typedef struct {
  int numbers[3][5];
  bool found[3][5];
} card_t;

typedef struct {
  card_t *next_card;
  card_t *winner_card;
  int winner_id;
  winner_e winner_type;
  bool exit;
  int last_number;
  sem_t mutex;
} shared_data_t;

typedef struct {
  int id;
  int num_cards;
  shared_data_t *shared_data;
  sem_t read_sem;
  sem_t write_sem;
} thread_data_t;

void printCard(card_t *card) {
  for (int i = 0; i < 3; ++i) {
    printf("(");
    for (int j = 0; j < 5; ++j)
      (j == 4) ? printf("%d", card->numbers[i][j])
               : printf("%d,", card->numbers[i][j]);
    (i == 2) ? printf(")") : printf(") / ");
  }
}

void *thread_function(void *arg) {
  thread_data_t *my_data = (thread_data_t *)arg;
  card_t **my_cards = malloc(sizeof(card_t *) * my_data->num_cards);
  int num;

  // da qui i thread iniziano a prendere
  // le carte messe a disposizione dal dealer

  for (int card = 0; card < my_data->num_cards; ++card) {
    // aspetto il dealer
    sem_wait(&my_data->read_sem);
    sem_wait(&my_data->shared_data->mutex);
    my_cards[card] = malloc(sizeof(card_t));
    memcpy(my_cards[card], my_data->shared_data->next_card, sizeof(card_t));
    printf("P%d: ricevuta card ", my_data->id);
    printCard(my_cards[card]);
    printf("\n");
    sem_post(&my_data->shared_data->mutex);

    // notifico il dealer di aver salvato la carta
    sem_post(&my_data->write_sem);
  }

  while (!my_data->shared_data->exit) {
    // aspetto il dealer per poter leggere l'ultimo numero
    sem_wait(&my_data->read_sem);
    if (my_data->shared_data->exit)
      break;
    sem_wait(&my_data->shared_data->mutex);
    num = my_data->shared_data->last_number;
    winner_e win_type = my_data->shared_data->winner_type;
    sem_post(&my_data->shared_data->mutex);

    // notifico il dealer di aver finito di leggere
    sem_post(&my_data->write_sem);

    int index_winner_card;
    bool win = false;

    // controllo se ho l'ultimo numero uscito tra le mie cards
    for (int card = 0; card < my_data->num_cards; ++card)
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 5; ++j) {
          if (my_cards[card]->numbers[i][j] == num) {
            my_cards[card]->found[i][j] = true;
          }
        }

    // controllo se ho fatto cinquina
    if (win_type == NO_WIN) {
      for (int card = 0; card < my_data->num_cards; ++card)
        for (int i = 0; i < 3; ++i) {
          if (my_cards[card]->found[i][0] && my_cards[card]->found[i][1] &&
              my_cards[card]->found[i][2] && my_cards[card]->found[i][3] &&
              my_cards[card]->found[i][4]) {
            win = true;
            index_winner_card = card;
            break;
          }
        }
    }

    // se qualcuno ha già fatto cinquina controllo di aver fatto bingo
    else if (win_type == CINQUINA) {
      for (int card = 0; card < my_data->num_cards; ++card) {
        win = true;
        for (int i = 0; i < 3; ++i)
          for (int j = 0; j < 5; ++j)
            if (!my_cards[card]->found[i][j])
              win = false;

        if (win)
          index_winner_card = card;
      }
    }

    // aspetto che il dealer mi dia la possibilità di segnalare la mia eventuale
    // vittoria
    sem_wait(&my_data->write_sem);
    if (win) {

      sem_wait(&my_data->shared_data->mutex);
      my_data->shared_data->winner_id = my_data->id;
      my_data->shared_data->winner_card = my_cards[index_winner_card];
      switch (win_type) {
      case (NO_WIN):
        printf("P%d: card con cinquina: ", my_data->id);
        printCard(my_cards[index_winner_card]);
        printf("\n");
        my_data->shared_data->winner_type = CINQUINA;
        break;
      case (CINQUINA):
        printf("P%d: card con Bingo: ", my_data->id);
        printCard(my_cards[index_winner_card]);
        printf("\n");
        my_data->shared_data->winner_type = BINGO;
        break;
      case (BINGO):
        break;
      }
      sem_post(&my_data->shared_data->mutex);
    }

    // notifico il dealer
    sem_post(&my_data->read_sem);
  }

  // deallocazione carte
  for (int i = 0; i < my_data->num_cards; ++i) {
    if (my_cards[i] != my_data->shared_data->winner_card)
      free(my_cards[i]);
  }
  free(my_cards);

  return NULL;
}

bool isIn(int *arr, int dim, int num) {
  for (int i = 0; i < dim; ++i)
    if (arr[i] == num)
      return true;
  return false;
}

card_t *generateCard() {
  int seen_numbers[75];
  int dim = 0;
  card_t *ret = malloc(sizeof(card_t));

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 5; ++j) {
      int num;
      do {
        num = 1 + (rand() % 76);
      } while (isIn(seen_numbers, dim, num));
      ret->numbers[i][j] = num;
      ret->found[i][j] = false;
      seen_numbers[dim++] = num;
    }
  }

  return ret;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Uso: %s <n> <m>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  srand(time(NULL));

  int num_players = atoi(argv[1]);
  int num_cards = num_players * atoi(argv[2]);

  shared_data_t *data = malloc(sizeof(shared_data_t));
  if (sem_init(&data->mutex, 0, 1) < 0) {
    perror("Errore nell'inizializzazione del semaforo mutex");
    exit(EXIT_FAILURE);
  }
  data->exit = false;
  data->winner_type = NO_WIN;
  data->winner_id = -1;
  data->winner_card = NULL;
  data->last_number = 0;

  // creo gli argomenti da passare ai thread
  thread_data_t *arguments[num_players];

  for (int i = 0; i < num_players; ++i) {
    arguments[i] = malloc(sizeof(thread_data_t));
    arguments[i]->id = i + 1;
    arguments[i]->shared_data = data;
    arguments[i]->num_cards = atoi(argv[2]);
    sem_init(&arguments[i]->read_sem, 0, 0);
    sem_init(&arguments[i]->write_sem, 0, 0);
  }

  // creo i thread
  pthread_t threads[num_players];
  for (int i = 0; i < num_players; ++i) {
    if (pthread_create(&threads[i], NULL, thread_function, arguments[i]) < 0) {
      perror("Errore nella creazione dei thread");
      exit(EXIT_FAILURE);
    }
  }

  // inizio a distribuire le carte ai threads
  for (int i = 0; i < num_cards; ++i) {
    card_t *new_card = generateCard();
    printf("D: genero e distribuisco la card n.%d: ", i + 1);
    printCard(new_card);
    printf("\n");
    sem_wait(&data->mutex);
    if (data->next_card != NULL)
      free(data->next_card);
    data->next_card = new_card;
    int curr_player = i % num_players;

    // notifico il thread "in fila" (curr_player) che è disponibile una carta
    sem_post(&arguments[curr_player]->read_sem);
    sem_post(&data->mutex);

    // aspetto che il thread abbia finito
    sem_wait(&arguments[curr_player]->write_sem);
  }

  printf("D: fine della distribuzione delle card e inizio di estrazione dei "
         "numeri.\n");

  int seen_numbers[75];
  int dim = 0;

  // inizio a distribuire i numeri
  winner_e win_type = NO_WIN;
  do {
    int next_number;

    // riprovo fino a quando il numero non è già stato visto
    do {
      next_number = 1 + (rand() % 76);
    } while (isIn(seen_numbers, dim, next_number));
    seen_numbers[dim++] = next_number;

    printf("D: estrazione del prossimo numero: %d\n", next_number);

    sem_wait(&data->mutex);
    data->last_number = next_number;
    sem_post(&data->mutex);

    for (int i = 0; i < num_players; ++i) {
      // notifico tutti i threads del nuovo numero
      sem_post(&arguments[i]->read_sem);

      // aspetto che i thread abbiano finito di leggere il nuovo numero
      sem_wait(&arguments[i]->write_sem);

      // notifico i thread che possono comunicarmi se hanno vinto
      sem_post(&arguments[i]->write_sem);

      // aspetto una notifica dei thread
      sem_wait(&arguments[i]->read_sem);

      sem_wait(&data->mutex);

      // controllo se c'è un vincitore
      if (win_type != data->winner_type) {
        win_type = data->winner_type;
        if (win_type == CINQUINA) {
          printf("D: il giocatore n.%d ha vinto la cinquina con la scheda ",
                 data->winner_id);
          printCard(data->winner_card);
          printf("\n");
        } else {
          printf("D: il giocatore n.%d ha vinto il bingo con la scheda ",
                 data->winner_id);
          printCard(data->winner_card);
          printf("\n");
        }
      }
      sem_post(&data->mutex);
    }

  } while (win_type != BINGO);

  sem_wait(&data->mutex);
  data->exit = true;
  sem_post(&data->mutex);

  printf("D: fine del gioco\n");

  // fine del programma e deallocazione memoria
  for (int i = 0; i < num_players; ++i) {

    // risveglio tutti i thread per far terminare la loro esecuzione
    sem_post(&arguments[i]->read_sem);
    if (pthread_join(threads[i], NULL) < 0) {
      perror("Errore nella join dei thread");
      exit(EXIT_FAILURE);
    }

    sem_destroy(&arguments[i]->read_sem);
    sem_destroy(&arguments[i]->write_sem);
    free(arguments[i]);
  }
  sem_destroy(&data->mutex);
  if (data->next_card != NULL)
    free(data->next_card);
  if (data->winner_card != NULL)
    free(data->winner_card);
  free(data);

  return EXIT_SUCCESS;
}
