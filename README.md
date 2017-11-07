# POSIX-Threads
POSIX Thread Implementation

For our second project we were to create a multithreaded system. To do this we created 3 functions:
pthread_create, pthread_exit, pthread_self, and a scheduler.

pthread_create
======
In this function I have a flag that is only used to create the main thread.
Once it creates the main thread, I turn the flag off and initialize all of the threads' status 
to 0 which is equal to EXIT. Also in this thread is where I create all of the other threads.
When they get created, I set their ID to the thread_counter that gets incremented after each 
thread creation. I set their status to READY and also malloc 32767 bytes for their stack.
I have a pointer that points to the top of the stack. I decrement that counter, and set the pointer
equal to the arg and then I decrement again and set the pointer to pthread_exit. I save the threads environment
and then modify its PC and stack pointers.

pthread_exit
======
If the main thread enters the exit function, exit the whole process. Set the current threads process
to 0 (EXIT). Free the stack's memory that was allocated at creation. Call the scheduler to get the next thread.

pthread_self
======
Return the current thread's ID.

scheduler
======
If the current thread is running, set it to ready. We dont want to set a thread that was already exited to 
get set to ready. Check if the thread has been long jumped into, if it hasnt, save the environment and look
for the next READY thread.

Added pthread_join, lock, unlock, sem_init, sem_wait, sem_post, sem_destroy.

pthread_join
======
If a thread calls pthread_join, it will be blocked and remain blocked until the thread it
calls pthread_join on finishes/exits. It will return exit_value of the thread it calls it on.

lock
======
Empties the sigset. Adds a sig alarm to the list named sigset. It will now block all incoming signals
for the thread that calls lock.

sem_init
======
Initializes a semaphore to the passed in value. 

sem_wait
======
If the value of the semaphore is greater than 0, subtract 1 from it. If it is equal to 0,
push the thread that calls sem_wait into the queue and block it.

sem_post
======
Add 1 to the value of the semaphore and unblock the first thread in the queue.

sem_destroy
======
Uninitialize the semaphore and free up the queue.
