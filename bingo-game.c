#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <time.h>

typedef enum {
  NO_WIN,
  CINQUINA,
  BINGO
} winner_e;

typedef struct {
  int numbers[3][5];
  bool found[3][5];
} card_t;

typedef struct {
  card_t* next_card;
  card_t* winner_card;
  int winner_id;
  winner_e winner_type;
  bool exit;
  int last_number;
  sem_t mutex;
} shared_data_t;

typedef struct {
  int id;
  int num_cards;
  shared_data_t* shared_data;
  sem_t read_sem;
  sem_t write_sem;
} thread_data_t;

void printCard(card_t* card){
  for (int i = 0; i < 3; ++i){
      printf("(");
      for (int j = 0; j < 5; ++j) (j == 4) ? printf("%d", card->numbers[i][j]) : printf("%d,", card->numbers[i][j]);
      (i == 2) ? printf(")") : printf(") / ");
    }
}

void* thread_function(void* arg){
  thread_data_t* my_data = (thread_data_t*) arg;
  card_t** my_cards = malloc(sizeof(card_t*) * my_data->num_cards);
  int num;
  // printf("P%d: Thread inizializzato con %d cards possibili.\n", my_data->id, my_data->num_cards); 
  for (int card = 0; card < my_data->num_cards; ++card){
    sem_wait(&my_data->read_sem);
    sem_wait(&my_data->shared_data->mutex);
    my_cards[card] = malloc(sizeof(card_t)); 
    memcpy(my_cards[card], my_data->shared_data->next_card, sizeof(card_t));
    //free(my_data->shared_data->next_card);
    printf("P%d: ricevuta card ", my_data->id);
    printCard(my_cards[card]);
   // printf("\nP%d: ho in totale %d cards.", my_data->id, card+1);
    printf("\n");
    sem_post(&my_data->shared_data->mutex);
    sem_post(&my_data->write_sem);
  }
 
  while (!my_data->shared_data->exit){
    sem_wait(&my_data->read_sem);
    if (my_data->shared_data->exit) break;
    sem_wait(&my_data->shared_data->mutex);
    num = my_data->shared_data->last_number;
    winner_e win_type = my_data->shared_data->winner_type;
    sem_post(&my_data->shared_data->mutex);
    sem_post(&my_data->write_sem);

    int index_winner_card;
    bool win = false;
    
    for (int card = 0; card < my_data->num_cards; ++card)
      for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 5; ++j){
          if (my_cards[card]->numbers[i][j] == num){
            my_cards[card]->found[i][j] = true;
          }
        }

    if (win_type == NO_WIN){
      for (int card = 0; card < my_data->num_cards; ++card)
        for (int i = 0; i < 3; ++i){
          if (my_cards[card]->found[i][0] && my_cards[card]->found[i][1] && my_cards[card]->found[i][2] && my_cards[card]->found[i][3] && my_cards[card]->found[i][4]) {
            win = true;
            index_winner_card = card;
            break;
          }
        }
    }

    else if (win_type == CINQUINA){
      for (int card = 0; card < my_data->num_cards; ++card){
          win = true;
          for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 5; ++j)
              if (!my_cards[card]->found[i][j]) win = false;

          if (win) index_winner_card = card;
      }
    }

    sem_wait(&my_data->write_sem);
    if (win){

      sem_wait(&my_data->shared_data->mutex);
      my_data->shared_data->winner_id = my_data->id;
      my_data->shared_data->winner_card = my_cards[index_winner_card];
      switch (win_type){
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
        case (BINGO): break;
      }
      sem_post(&my_data->shared_data->mutex);
    }
    sem_post(&my_data->read_sem);
  }

  for (int i = 0; i < my_data->num_cards; ++i) {
   if (my_cards[i] != my_data->shared_data->winner_card) free(my_cards[i]);
  }
  free(my_cards);
  
  return NULL;
}

bool isIn(int* arr, int dim, int num){
  for (int i = 0; i < dim; ++i) if (arr[i] == num) return true;
  return false;
}

card_t* generateCard(){
  int seen_numbers[75];
  int dim = 0;
  card_t* ret = malloc(sizeof(card_t));

  for (int i = 0; i < 3; ++i){
    for (int j = 0; j < 5; ++j){
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

int main(int argc, char* argv[]){
  if (argc != 3){
    printf("Uso: %s <n> <m>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  srand(time(NULL));

  int num_players = atoi(argv[1]);
  int num_cards = num_players * atoi(argv[2]);

  shared_data_t* data = malloc(sizeof(shared_data_t));
  if (sem_init(&data->mutex, 0, 1) < 0){
    perror("Errore nell'inizializzazione del semaforo mutex");
    exit(EXIT_FAILURE);
  }
  data->exit = false;
  data->winner_type = NO_WIN;
  data->winner_id = -1;
  data->winner_card = NULL;
  data->last_number = 0;
  
 // printf("D: memoria condivisa allocata correttamente\n");

  thread_data_t* arguments[num_players];

  for (int i = 0; i < num_players; ++i){
    arguments[i] = malloc(sizeof(thread_data_t));
    arguments[i]->id = i + 1;
    arguments[i]->shared_data = data;
    arguments[i]->num_cards = atoi(argv[2]);
    sem_init(&arguments[i]->read_sem, 0, 0);
    sem_init(&arguments[i]->write_sem, 0, 0);
  }
  
  //printf("D: argomenti allocati correttamente\n");

  pthread_t threads[num_players];
  for (int i = 0; i < num_players; ++i){
    if (pthread_create(&threads[i], NULL, thread_function, arguments[i]) < 0){
      perror("Errore nella creazione dei thread");
      exit(EXIT_FAILURE);
    }
  }

 // printf("D: thread avviati correttamente\n");
  //printf("D: creo e distribuisco un totale di %d cards.\n", num_cards);
  for (int i = 0; i < num_cards; ++i){
    card_t* new_card = generateCard();
    printf("D: genero e distribuisco la card n.%d: ", i + 1);
    printCard(new_card);
    printf("\n");
    sem_wait(&data->mutex);
    if (data->next_card != NULL) free(data->next_card);
    data->next_card = new_card;
    int curr_player = i % num_players;
   // printf("D: do la card al player %d.\n", curr_player + 1);
    sem_post(&arguments[curr_player]->read_sem);
    sem_post(&data->mutex);
    sem_wait(&arguments[curr_player]->write_sem);
  }

  printf("D: fine della distribuzione delle card e inizio di estrazione dei numeri.\n");

  int seen_numbers[75];
  int dim = 0;

  winner_e win_type = NO_WIN;
  do {
    int next_number;
    do {
      next_number = 1 + (rand() % 76);
    } while (isIn(seen_numbers, dim, next_number));
    seen_numbers[dim++] = next_number;
    printf("D: estrazione del prossimo numero: %d\n", next_number);
    sem_wait(&data->mutex);
    data->last_number = next_number;
    sem_post(&data->mutex);
  //  printf("D: ho scritto il numero nella memoria condivisa.\n");

    for (int i = 0; i < num_players; ++i){
      sem_post(&arguments[i]->read_sem);
      sem_wait(&arguments[i]->write_sem);
      sem_post(&arguments[i]->write_sem);
      sem_wait(&arguments[i]->read_sem);
      sem_wait(&data->mutex);
      if (win_type != data->winner_type) {
        win_type = data->winner_type;
        if (win_type == CINQUINA){
          printf("D: il giocatore n.%d ha vinto la cinquina con la scheda ", data->winner_id);
          printCard(data->winner_card);
          printf("\n");
        }
        else {
          printf("D: il giocatore n.%d ha vinto il bingo con la scheda ", data->winner_id);
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

  for (int i = 0; i < num_players; ++i){
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
  if (data->next_card != NULL) free(data->next_card);
  if (data->winner_card != NULL) free(data->winner_card);
  free(data);

  return EXIT_SUCCESS;
}
