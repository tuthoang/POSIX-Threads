#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#include <semaphore.h>

#define MAXSIZE 128 //1 for main thread, 128 for others
#define MAXSEM 20
#define TIMER_PERIOD 50000   // Period in us


static int ptr_mangle();
void signal_handler();
void scheduler();
pthread_t pthread_self();
int pthread_create();
void pthread_exit();
void pthread_exit_wrapper();
void lock();
void unlock();


//set these to current tcb's buff
struct TCB{
  jmp_buf env;
  void *stack;
  void *exit_ptr;
  pthread_t ID;
  pthread_t join_ID;
  int status; //0 = exit, 1 = ready, 2 = running, 3 = blocked
  void *exit_val;
};

struct node{
  int ID;
  struct node *next;
};

struct Semaphore{
  int id;
  int is_init;
  int value;
  struct node *queue;
};

int thread_counter = 1;
int thread_tracker = 0;
int flag = 0;

//able to store a maximum of 128 threads
struct TCB threads[MAXSIZE];

//able to store a maximum of 20 semaphores
struct Semaphore semaphores[MAXSEM];

int sem_tracker = 0;
int SEM_ID = 0;
struct sigaction sig;
sigset_t sigset;


static int ptr_mangle(int p){
    unsigned int ret;
    asm(" movl %1, %%eax;\n"
        " xorl %%gs:0x18, %%eax;"
        " roll $0x9, %%eax;"
        " movl %%eax, %0;"
    : "=r"(ret)
    : "r"(p)
    : "%eax"
    );
    return ret;
}
void pthread_exit_wrapper()
{
  unsigned int res;
  asm("movl %%eax, %0\n":"=r"(res));
  pthread_exit((void *) res);
} 
void timer(){
  sig.sa_handler = scheduler;
  sig.sa_flags = SA_NODEFER;

  sigaction(SIGALRM, &sig, 0);
  ualarm(TIMER_PERIOD, TIMER_PERIOD);
}

void scheduler(){
  //check if the thread is running, set it to ready
  if(threads[thread_tracker].status == 2 )
    threads[thread_tracker].status = 1;                          

  //save current state
  if(setjmp(threads[thread_tracker].env) == 0){

    //get next ready thread
    do{
      thread_tracker++;
      if(thread_tracker > thread_counter) thread_tracker = 0;
    }while(threads[thread_tracker].status != 1);        //get next READY UNBLOCKED threads

    threads[thread_tracker].status = 2;                      //Set next ready thread to running
    longjmp(threads[thread_tracker].env, 1);
  }
  //coming from a long jump, just return
  else{
    return;
  }
}


pthread_t pthread_self(void){
  return threads[thread_tracker].ID;
}

//register[4] = stack ptr
//register[5] = pc
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void 
  *(*start_routine) (void *), void *arg) {

  //For the MAIN thread, only executes ONCE
  if(flag == 0){
    if(!setjmp(threads[0].env)){
      flag++;         
      //initialize MAIN THREAD                   
      threads[0].ID = 0;
      threads[0].status = 2;
      threads[0].stack = NULL;
      threads[0].exit_val = NULL;
      int i;

      //initialize thread statuses of those that arent running
      for(i=2;i<MAXSIZE; i++){
        threads[i].status = 0; //init to exit
      }

      //timer runs every 50ms
      timer();

      // setjmp(threads[0].env);
    }
  }
  if(thread_counter >= MAXSIZE) {
    return -1;
  }

  attr = NULL;
  //initialize thread
  *thread = thread_counter;
  threads[thread_counter].ID = thread_counter;                 
  threads[thread_counter].status = 1;                          
  threads[thread_counter].stack = (void*)malloc(32767); 

  //exit_ptr to used free the stack when finished
  threads[thread_counter].exit_ptr = threads[thread_counter].stack;
  threads[thread_counter].exit_val = NULL;
  threads[thread_counter].join_ID = -1;
  //point to TOP of the stack, and place arguments in right spot
  unsigned long int *s = threads[thread_counter].stack + 32767;
  s--;
  *s = (unsigned long int) arg;
  s--;
  *s = (unsigned long int) pthread_exit_wrapper;

  //save the environment
  // setjmp(threads[thread_counter].env);
  threads[thread_counter].env[0].__jmpbuf[4] = ptr_mangle((unsigned long int) s);
  threads[thread_counter].env[0].__jmpbuf[5] = ptr_mangle((unsigned long int) start_routine);


  thread_counter++; 
  return 0;
  //return 0 success
  //return -1 fail
}

void pthread_exit(void* value_ptr){
  //exit PROCESS if main thread enters
  if(thread_tracker == 0) exit(0);

  //set current thread to exit
  threads[thread_tracker].status = 0;

  //store the input value_ptr to the current threads exit value
  threads[thread_tracker].exit_val = value_ptr;

  if(threads[thread_tracker].join_ID != -1){
    //unblock  the thread that this was joined on
    threads[threads[thread_tracker].join_ID].status = 1; 
  }
  //free the stack
  free(threads[thread_tracker].exit_ptr);

  //call scheduler to get next ready thread
  scheduler();
}

int pthread_join(pthread_t thread, void **value_ptr){
  //cant join on own thread
  if(thread == thread_tracker) return -1;

  //check if thread is not EXITED
  if(threads[(int)thread].status != 0){
    threads[thread_tracker].status = 3; //Blocked
    threads[(int)thread].join_ID = thread_tracker;
  }
  scheduler();
  
  if(value_ptr != NULL){
    *value_ptr = threads[(int)thread].exit_val;
  }
  // free(threads[(int)thread].exit_ptr);

  return 0;
}

int sem_init(sem_t *sem, int pshared, unsigned value){
  //max of 20 semaphores
  if(sem_tracker < MAXSEM){
    semaphores[sem_tracker].is_init = 1;
    semaphores[sem_tracker].value = value;
    semaphores[sem_tracker].queue = malloc(sizeof(struct node));
    semaphores[sem_tracker].id = sem_tracker;

    //__align holds a reference to this new semaphore
    sem -> __align = (long int)sem_tracker;
    sem_tracker++;
  }else{
    return -1;
  }
  return 0;
}

int sem_wait(sem_t *sem){
  //semaphore must be initialized to be used
  if(semaphores[sem -> __align].is_init){

    //if semaphore value is 0, BLOCK the thread that wants access to the critical section
    if(semaphores[sem -> __align].value <= 0){
      threads[thread_tracker].status = 3; //block the thread

      //create a node with current thread ID to push to the queue 
      struct node *blocked_thread = (struct node*)malloc(sizeof(struct node));
      blocked_thread->ID = thread_tracker;
      blocked_thread->next = NULL;

      //if queue is empty, set the head to this blocked thread
      if(semaphores[sem -> __align].queue == NULL){
        semaphores[sem -> __align].queue = blocked_thread;
      }
      else{
        //else iterate through the queue, append blocked thread at end
        struct node *temp = semaphores[sem -> __align].queue;
        while(temp->next != NULL){
          temp = temp->next;
        }
        temp->next = blocked_thread;
      }
    }else{
      //only decrement the semaphore value if it is greater than 0
      (semaphores[sem -> __align].value)--;
    }
  }else return -1;
  return 0;
}

int sem_post(sem_t *sem){
  if(semaphores[sem -> __align].is_init){
    //there is a queue of waiting threads
    //Unblock one at a time
    if(semaphores[sem -> __align].queue != NULL){
      struct node *temp = semaphores[sem -> __align].queue;
      threads[(int)temp->ID].status = 1; //READY
      // if(semaphores[sem -> __align].queue->next)
        semaphores[sem -> __align].queue = semaphores[sem -> __align].queue->next;
        // free(temp);
      // else
      //   semaphores[sem -> __align].queue = NULL;

    }
    (semaphores[sem -> __align].value)++;
  } else return -1;
  return 0;
}

int sem_destroy(sem_t *sem){
  semaphores[sem -> __align].is_init = 0;
  // if(semaphores[sem -> __align].queue != NULL){
  //   while(semaphores[sem -> __align].queue->next){
  //     struct node *temp = semaphores[sem -> __align].queue;
  //     semaphores[sem -> __align].queue = semaphores[sem -> __align].queue->next;
  //     free(temp);
  //   }
  // //   free(semaphores[sem -> __align].queue);
  // }
  return 0;
}

void lock(){
  sigemptyset(&sigset);                       //Initialize and empty sigset
  sigaddset(&sigset, SIGALRM);                //Add SIGALRM to list sigset
  sigprocmask(SIG_BLOCK, &sigset, NULL);      //Change signal mask of calling thread to blocked
}

void unlock(){
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGALRM);
  sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

