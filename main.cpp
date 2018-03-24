
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <time.h>
#include <queue>

#define TRESHOLD_TIME 4
#define RCV_BUF_SIZE  128

#define BOAT_MSG_TYPE 1
#define BOAT_ENT_RESPONSE_STR   "OK"
#define BOAT_ERR_RESPONSE_STR   "ER"

#define MISSIONARY_REQ_STR  "req:mis"
#define CANNIBAL_REQ_STR    "req:can"

#define MISSIONARY_TYPE   1
#define CANNIBAL_TYPE     2

// If this is n>0, n workers will be randomly
// created (50/50 change of beeing missionary or cannibal)
#define RANDOM_GENERATE_WORKERS       100
#define DEFAULT_CANNIBAL_PROCESSES    10
#define DEFAULT_MISSIONARY_PROCESSES  10

#define PRINT_WORKERS_RCV_CONF    0
#define PRINT_WORKERS_SND_DATA    0
#define PRINT_WORKERS_MISC_INFO   0

#define PRINT_PRODUCER_RCV_CONF   0
#define PRINT_PRODUCER_SND_DATA   0
#define PRINT_PRODUCER_MISC_INFO  0


struct msgbuf_t {
    long int    mtype;     /* message type */
    char        mtext[RCV_BUF_SIZE];  /* message text */
};


typedef struct boat {
  char cannibals[3][20];
  int cannibals_num;
  char missionaries[3][20];
  int missionaries_num;
} boat_t;

/**
  * Initialize boat structure
  */
void boat_init(boat_t *boat){
  //boat->cannibals = (char**) malloc(1);
  //boat->missionaries = (char**) malloc(1);
  boat->cannibals_num = 0;
  boat->missionaries_num = 0;
}

/**
  * Add cannibal to the boat
  */
void add_cannibal(boat_t *boat, char *cannibal){
  //boat->cannibals = (char**) realloc(boat->cannibals, boat->cannibals_num + 1);
  //boat->cannibals[boat->cannibals_num] = (char*) malloc(strlen(cannibal));
  strcpy(boat->cannibals[boat->cannibals_num], cannibal);
  boat->cannibals_num += 1;
}

/**
  * Add missionary to the boat
  */
void add_missionary(boat_t *boat, char *missionary){
  //boat->missionaries = (char**) realloc(boat->missionaries, boat->missionaries_num + 1);
  //boat->missionaries[boat->missionaries_num] = (char*) malloc(strlen(missionary) + 1);
  strcpy(boat->missionaries[boat->missionaries_num], missionary);
  boat->missionaries_num += 1;
}

/**
  * Send boat
  */
int send_boat(boat_t *boat, boat_t *total){
  total->missionaries_num += boat -> missionaries_num;
  total->cannibals_num    += boat -> cannibals_num;

  printf("Sending boat with %d missionaries and %d cannibals [%d, %d]\n",
          boat->missionaries_num,
          boat->cannibals_num,
          total->missionaries_num,
          total->cannibals_num);

  printf("\t missionaries: ");
  for(int i = 0; i < boat->missionaries_num; i++){
    printf("%s ", boat->missionaries[i]);
  }

  printf(", cannibals: ");
  for(int i = 0; i < boat->cannibals_num; i++){
    printf("%s ", boat->cannibals[i]);
  }

  printf("\n");


  boat_init(boat);
}


int boat_process(int msg_id){

  // Loop while there are requests to enter boat,
  // if there are no requests in the timeframe of 4s
  // exit.

  //Prepare receive and send buffers
  struct msgbuf_t msg_rcv, msg_snd;
  std::queue<msgbuf_t> rcv_queue;

  time_t last_request_time = time(NULL);
  int result;

  boat_t boat;
  boat_t total_sent;
  boat_init(&boat);
  boat_init(&total_sent);

  char person_info[10];
  int cannibals_num = 0, missionaries_num = 0;
  int can_enter = 0;
  int response_id = -1;
  while(1){
      if((time(NULL) - last_request_time) >= TRESHOLD_TIME){
        if(boat.cannibals_num == 0 && boat.missionaries_num == 0){
          // There was no requests in the past TRESHOLD_TIME seconds,
          // exit.
          if(rcv_queue.empty()){
            break;
          }
        }else{
          //Send the boat
          send_boat(&boat, &total_sent);
        }
      }

      // Check the current status of the boat, if it is full
      // set it for sail.
      if((boat.cannibals_num + boat.missionaries_num) == 3){
        //Boat is full, send it
        send_boat(&boat, &total_sent);
      }
      can_enter = 0;

      // Check if there are any requests waiting in queue
      if(!rcv_queue.empty()){
        // If there are, get the first request from the queue
        msg_rcv = rcv_queue.front();
        rcv_queue.pop();
        result = 1;
      }else{
        // Else wait for the request from message queue
        result = msgrcv(msg_id, &msg_rcv, RCV_BUF_SIZE, BOAT_MSG_TYPE, IPC_NOWAIT);
      }

      //response_id = msg_rcv.mtype;
      if(result <= 0){
        continue;
      }

      if(PRINT_PRODUCER_RCV_CONF)
        printf("Boat received: %s\n", msg_rcv.mtext);

      //Get the ID of the proccess that sent the request
      int sender_pid = atoi((msg_rcv.mtext + (strlen(MISSIONARY_REQ_STR))));

      if(strncmp(msg_rcv.mtext, MISSIONARY_REQ_STR, strlen(MISSIONARY_REQ_STR)) == 0){
        // Missionary reqested boat entry
        if(boat.cannibals_num == 0 || boat.missionaries_num >= (boat.cannibals_num + 1)){
          // Missionary can enter the boat if:
          // - there are no cannibals currently on the boat OR
          // - the number of missionaries on the boar is GREATER THEN OR EQUAL
          //    to the number of cannibals on the boat.
          sprintf(person_info, "mis%d", sender_pid);
          add_missionary(&boat, person_info);
          can_enter = 1;
        }else{
          can_enter = 0;
        }

      }else if(strncmp(msg_rcv.mtext, CANNIBAL_REQ_STR, strlen(CANNIBAL_REQ_STR)) == 0){
        // Cannibal requested boat entry
        if(boat.missionaries_num == 0 || boat.cannibals_num < boat.missionaries_num){
          // Cannial can enter the boat if:
          // - there are no missionaries currently on the boar OR
          // - the number of cannibals is LOWER THEN the number of missionaries
          sprintf(person_info, "can%d", sender_pid);
          add_cannibal(&boat, person_info);
          can_enter = 1;
        }else{
          // Cannibal can not enter
          can_enter = 0;
        }

      }


      msg_snd.mtype = sender_pid;

      if(can_enter){
        // If the person can enter the boat, send the confirmation response
        strcpy(msg_snd.mtext, BOAT_ENT_RESPONSE_STR);
        if(PRINT_PRODUCER_SND_DATA)
          printf("Boat sending: %lu:%s\n", msg_snd.mtype, msg_snd.mtext );
        result = msgsnd(msg_id, &msg_snd, strlen(BOAT_ENT_RESPONSE_STR), 0);
        if(result == -1){
          printf("Unsuccessfull\n");
        }
        last_request_time = time(NULL);
      }else{
        // If the person can not enter the boat, add their request to the queue
        rcv_queue.push(msg_rcv);
        //strcpy(msg_snd.mtext, BOAT_ERR_RESPONSE_STR);
      }


  }



}

int send_request(int who, int msg_id, int pid){
  struct msgbuf_t msg_snd;
  char pid_s[10];
  int result;
  if(who == MISSIONARY_TYPE){
    strcpy(msg_snd.mtext, MISSIONARY_REQ_STR);
    if(PRINT_WORKERS_SND_DATA)
      printf("Missionary ");
  }else if(who == CANNIBAL_TYPE){
    strcpy(msg_snd.mtext, CANNIBAL_REQ_STR);
    if(PRINT_WORKERS_SND_DATA)
      printf("Cannibal ");
  }
  sprintf(pid_s, "%d", pid);
  strcat(msg_snd.mtext, pid_s);
  msg_snd.mtype = BOAT_MSG_TYPE;
  if(PRINT_WORKERS_SND_DATA)
    printf("sending request: %s\n", msg_snd.mtext);
  result = msgsnd(msg_id, &msg_snd, strlen(msg_snd.mtext)+1, 0);
  return result;
}

int receive_response(int msg_id, int pid){
  struct msgbuf_t msg_rcv;
  int result;
  //Wait for the response
  result = msgrcv(msg_id, &msg_rcv, RCV_BUF_SIZE, pid, 0);
  if(result > 0){
    if(strcmp(msg_rcv.mtext, BOAT_ENT_RESPONSE_STR) == 0){
      return 0;
    }
  }

  return 1;

}

int cannibal_process(int msg_id, int id){
  int result;
  int sleep_time;
  if(PRINT_WORKERS_MISC_INFO)
    printf("Cannibal %d created\n", id);

  srand(id + time(NULL));
  while(1){
    //Wait for random amount of time
    sleep_time = ((rand() % 2) + 2);
    sleep(sleep_time);
    //Send request
    send_request(CANNIBAL_TYPE, msg_id, id);

    do{
      //Wait for the response
      result = receive_response(msg_id, id);
      if(PRINT_WORKERS_RCV_CONF)
        printf("Cannibal %d received %d\n", id, result);

    }while(result != 0);

    if(result == 0){
      break;
    }

  }
  if(PRINT_WORKERS_MISC_INFO)
    printf("Cannibal %d exit\n", id);

}

int missionary_process(int msg_id, int id){

  int result;
  useconds_t sleep_time;

  if(PRINT_WORKERS_MISC_INFO)
    printf("Missionary %d created\n", id);

  srand(id + time(NULL));
  while(1){
    //Wait for random amount of time
    sleep_time = ((rand() % 2) + 2);
    sleep(sleep_time);
    //Send request
    send_request(MISSIONARY_TYPE, msg_id, id);

    do{
      //Wait for the response
      result = receive_response(msg_id, id);
      if(PRINT_WORKERS_RCV_CONF)
        printf("Missionary %d received %d\n", id, result);

    }while(result != 0);

    if(result == 0){
      break;
    }

  }

  if(PRINT_WORKERS_MISC_INFO)
    printf("Missionary %d exit\n", id);

}

int main(int argc, char *argv[]){

  //Create central boat process
  int msg_id;
  int key = 10;
  msg_id = msgget(key, IPC_CREAT | IPC_EXCL | 0666);
  if(msg_id == -1){
    printf("Creating new msg queue!\n");
    msgctl(msg_id, IPC_RMID, NULL);
    msg_id = msgget(key, IPC_CREAT | 0666);
    if(msg_id == -1){
      exit(1);
    }
  }

  srand(time(NULL));
  int can_generated = 0, mis_generated = 0;
  if(RANDOM_GENERATE_WORKERS > 0){
    for(int i = 0; i < RANDOM_GENERATE_WORKERS; i++){
      int r = rand() % 10;
      if(r % 2 == 0){
        mis_generated++;
        if(fork() == 0){
          missionary_process(msg_id, i+3);
          exit(0);
        }
      }else{
        can_generated++;
        if(fork() == 0){
          cannibal_process(msg_id, i+3);
          exit(0);
        }
      }

    }

    printf("Generated %d missionaries and %d cannibals\n",  mis_generated,
                                                            can_generated);

  }else{
    int i, j;
    for(i=0; i < DEFAULT_CANNIBAL_PROCESSES; i++){
      if(fork() == 0){
        //Child process

        cannibal_process(msg_id, i+3);

        exit(0);
      }
    }

    for(j=0; j < DEFAULT_MISSIONARY_PROCESSES; j++){
      if(fork() == 0){
        //Child process

        missionary_process(msg_id, j + i + 3);

        exit(0);
      }
    }

  }


  printf("Starting boat process!\n");
  boat_process(msg_id);

  exit(0);
}
