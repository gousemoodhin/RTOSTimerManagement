// Header Files
#include "TimerAPI.h"
#include "TimerMgrHeader.h"
#include "TypeDefines.h"
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*****************************************************
 * Global Variables
 *****************************************************
 */
// Timer pool global variables.
INT8U FreeTmrCount = 0;
RTOS_TMR *FreeTmrListPtr = NULL;

// Tick counter.
INT32U RTOSTmrTickCtr = 0;

// Hash table.
HASH_OBJ hash_table[HASH_TABLE_SIZE];

// Thread variable for timer task.
pthread_t thread;

// Semaphore for signaling the timer task.
sem_t timer_task_sem;

// Mutex for protecting Hash table.
pthread_mutex_t hash_table_mutex;

// Mutex for protecting timer pool.
pthread_mutex_t timer_pool_mutex;

/*****************************************************
 * Timer API Functions
 *****************************************************
 */

/*
  @RTOSTmrCreate().
  Create timer and fill the timer object.
*/
RTOS_TMR *RTOSTmrCreate(INT32U delay, INT32U period, INT8U option,
                        RTOS_TMR_CALLBACK callback, void *callback_arg,
                        INT8 *name, INT8U *err) {

  if (FreeTmrCount == 0) {
    *err = RTOS_ERR_NONE;
    return NULL;
  }
  RTOS_TMR *timer_obj = NULL;
  // Check the input arguments for ERROR.
  if (option == RTOS_TMR_PERIODIC || option == RTOS_TMR_ONE_SHOT) {
    if (option == RTOS_TMR_ONE_SHOT && delay == 0) {
      *err = RTOS_ERR_TMR_INVALID_DLY;
      return NULL;
    }
    if (option == RTOS_TMR_PERIODIC && period == 0) {
      *err = RTOS_ERR_TMR_INVALID_PERIOD;
      return NULL;
    }
    // Allocate timer obj.
    timer_obj = alloc_timer_obj();
    if (timer_obj == NULL) {
      *err = RTOS_ERR_TMR_NON_AVAIL;
      return NULL;
    }

    // Fill up the timer object.
    timer_obj->RTOSTmrCallback = callback;
    timer_obj->RTOSTmrCallbackArg = callback_arg;
    timer_obj->RTOSTmrNext = NULL;
    timer_obj->RTOSTmrPrev = NULL;
    timer_obj->RTOSTmrMatch = 0;
    timer_obj->RTOSTmrDelay = delay;
    timer_obj->RTOSTmrPeriod = period;
    timer_obj->RTOSTmrName = name;
    timer_obj->RTOSTmrOpt = option;
    timer_obj->RTOSTmrState = RTOS_TMR_STATE_STOPPED;
    *err = RTOS_SUCCESS;
  } else {
    *err = RTOS_ERR_TMR_INVALID_OPT;
  }

  return timer_obj;
}

/*
  @ RTOSTmrDel().
  Free timer object according to its state.
*/
INT8U RTOSTmrDel(RTOS_TMR *ptmr, INT8U *perr) {
  // ERROR checking.
  if (ptmr == NULL) {
    fprintf(stdout, "\nTimer pointer is NULL\n");
    *perr = RTOS_ERR_TMR_INVALID;
    return RTOS_FALSE;
  }
  if (ptmr->RTOSTmrType != RTOS_TMR_TYPE) {
    fprintf(stdout, "\nTimer type is not RTOS_TMR_TYPE\n");
    *perr = RTOS_ERR_TMR_INVALID_TYPE;
    return RTOS_FALSE;
  }
  if (ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED) {
    *perr = RTOS_SUCCESS;
    return RTOS_TRUE;
  }

  if (ptmr->RTOSTmrState == RTOS_TMR_STATE_COMPLETED ||
      ptmr->RTOSTmrState == RTOS_TMR_STATE_RUNNING ||
      ptmr->RTOSTmrState == RTOS_TMR_STATE_STOPPED) {
    remove_hash_entry(ptmr);
    free_timer_obj(ptmr);
  } else {
    *perr = RTOS_ERR_TMR_INVALID_STATE;
    fprintf(stdout, "\n %s is not deleted with state = %d\n",
            RTOSTmrNameGet(ptmr, perr), ptmr->RTOSTmrState);
    return RTOS_FALSE;
  }

  *perr = RTOS_SUCCESS;
  return RTOS_TRUE;
}

/*
  @ RTOSTmrNameGet().
  Get the name of timer.
*/
INT8 *RTOSTmrNameGet(RTOS_TMR *ptmr, INT8U *perr) {
  // ERROR checking.
  if (ptmr == NULL) {
    fprintf(stdout, "\nTimer pointer is NULL\n");
    *perr = RTOS_ERR_TMR_INVALID;
    return NULL;
  } else if (ptmr->RTOSTmrType != RTOS_TMR_TYPE) {
    fprintf(stdout, "\nTimer type is not RTOS_TMR_TYPE\n");
    *perr = RTOS_ERR_TMR_INVALID_TYPE;
    return RTOS_FALSE;
  } else if (ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED) {
    *perr = RTOS_ERR_TMR_INACTIVE;
    return NULL;
  } else {
    // Return the pointer to the timer string.
    return ptmr->RTOSTmrName;
  }
}

/*
  @ RTOSTmrRemainGet
  Get the number of ticks remaining in time out.
*/
INT32U RTOSTmrRemainGet(RTOS_TMR *ptmr, INT8U *perr) {
  // ERROR checking.
  if (ptmr == NULL) {
    fprintf(stdout, "\nTimer pointer is NULL\n");
    *perr = RTOS_ERR_TMR_INVALID;
    return RTOS_FALSE;
  } else if (ptmr->RTOSTmrType != RTOS_TMR_TYPE) {
    fprintf(stdout, "\nTimer type is not RTOS_TMR_TYPE\n");
    *perr = RTOS_ERR_TMR_INVALID_TYPE;
    return RTOS_FALSE;
  } else if (ptmr->RTOSTmrState == RTOS_TMR_STATE_UNUSED) {
    *perr = RTOS_ERR_TMR_INACTIVE;
    return RTOS_FALSE;
  } else {
    // Return the remaining ticks.
    fprintf(stdout, "\nTimer remaining ticks = %d\n",
            (ptmr->RTOSTmrMatch - RTOSTmrTickCtr));
    return (ptmr->RTOSTmrMatch - RTOSTmrTickCtr);
  }
}

/*
  @ RTOSTmrStateGet().
  Get the state of the timer.
*/
INT8U RTOSTmrStateGet(RTOS_TMR *ptmr, INT8U *perr) {
  // ERROR checking.
  if (ptmr == NULL) {
    fprintf(stdout, "\nTimer pointer is NULL\n");
    *perr = RTOS_ERR_TMR_INVALID;
    return RTOS_FALSE;
  } else if (ptmr->RTOSTmrType != RTOS_TMR_TYPE) {
    fprintf(stdout, "\nTimer type is not RTOS_TMR_TYPE\n");
    *perr = RTOS_ERR_TMR_INVALID_TYPE;
    return RTOS_FALSE;
  } else {
    // Return timer state.
    return ptmr->RTOSTmrState;
  }
}

/*
  @ RTOSTmrStart().
  Based on the timer state, update the RTOSTmrMatch using RTOSTmrTickCtr,
  RTOSTmrDelay and RTOSTmrPeriod.
*/
INT8U RTOSTmrStart(RTOS_TMR *timer, INT8U *perr) {
  // ERROR checking.
  if (timer == NULL) {
    fprintf(stdout, "\nTimer pointer is NULL\n");
    *perr = RTOS_ERR_TMR_INVALID;
    return RTOS_FALSE;
  } else if (timer->RTOSTmrType != RTOS_TMR_TYPE) {
    fprintf(stdout, "\nTimer type is not RTOS_TMR_TYPE\n");
    *perr = RTOS_ERR_TMR_INVALID_TYPE;
    return RTOS_FALSE;
  } else {
    // .
    timer->RTOSTmrState = RTOS_TMR_STATE_RUNNING;
    printf("\nnadaf RTOSTmrTickCtr = %d timer->RTOSTmrDelay = %d\n",
           RTOSTmrTickCtr, timer->RTOSTmrDelay);
    timer->RTOSTmrMatch = RTOSTmrTickCtr + timer->RTOSTmrDelay;
    // You may use the Hash table to insert the running timer obj.
    insert_hash_entry(timer);
    return RTOS_TRUE;
  }
}

/*
  @ RTOSTmrStop().
  Function to stop the timer, and remove the timer from the Hash table list.
*/
INT8U RTOSTmrStop(RTOS_TMR *ptmr, INT8U opt, void *callback_arg, INT8U *perr) {
  // ERROR checking.
  if (ptmr == NULL) {
    fprintf(stdout, "\nTimer pointer is NULL\n");
    *perr = RTOS_ERR_TMR_INVALID;
    return RTOS_FALSE;
  }
  if (ptmr->RTOSTmrType != RTOS_TMR_TYPE) {
    fprintf(stdout, "\nTimer type is not RTOS_TMR_TYPE\n");
    *perr = RTOS_ERR_TMR_INVALID_TYPE;
    return RTOS_FALSE;
  }
  if (ptmr->RTOSTmrState == RTOS_TMR_STATE_STOPPED) {
    fprintf(stdout, "\nTimer state is STOPPED\n");
    *perr = RTOS_ERR_TMR_STOPPED;
    return RTOS_FALSE;
  }

  remove_hash_entry(ptmr);

  // Change timer state to STOPPED.
  ptmr->RTOSTmrState = RTOS_TMR_STATE_STOPPED;

  // Call callback function if required.
  if (ptmr->RTOSTmrCallback != NULL) {
    if (opt == RTOS_TMR_OPT_NONE) {
      fprintf(stdout, "\nTimer callback option = %d\n", opt);
    } else if (opt == RTOS_TMR_OPT_CALLBACK) {
      fprintf(stdout, "\nTimer callback option = %d\n", opt);
      ptmr->RTOSTmrCallback(ptmr->RTOSTmrCallbackArg);
    } else if (opt == RTOS_TMR_OPT_CALLBACK_ARG) {
      fprintf(stdout, "\nTimer callback option = %d\n", opt);
      ptmr->RTOSTmrCallback(callback_arg);
    } else {
      fprintf(stdout, "\nTimer callback option = %d\n", opt);
    }
  } else {
    if (ptmr->RTOSTmrCallback == NULL) {
      *perr = RTOS_ERR_TMR_NO_CALLBACK;
      return RTOS_FALSE;
    } else {
      return RTOS_FALSE;
    }
  }
  return RTOS_TRUE;
}

/*
  @ RTOSTmrSignal().
  Function called when OS tick Interrupt occurs which will signal the
  RTOSTmrTask() to update the timers.
*/
void RTOSTmrSignal(int signum) {
  // Send the signal to timer task using Semaphore.
  sem_post(&timer_task_sem);
}

/*
  @ GetRTOSTimer().
  Create RTOS timer, and return its pointer.
*/
RTOS_TMR *GetRTOSTimer() {
  RTOS_TMR *ptr;
  ptr = (RTOS_TMR *)malloc(sizeof(RTOS_TMR));
  ptr->RTOSTmrPrev = NULL;
  ptr->RTOSTmrNext = NULL;
  ptr->RTOSTmrType = RTOS_TMR_TYPE;
  ptr->RTOSTmrState = RTOS_TMR_STATE_UNUSED;
  return ptr;
}

/*****************************************************
 * Internal Functions
 *****************************************************
 */
/*
  @ Create_Timer_Pool().
  - Create pool of timers.
  - Create the timer pool using dynamic memory allocation.
  - We can magine of linkedList creation for timer obj.
*/
INT8U Create_Timer_Pool(INT32U timer_count) {
  printf("nadaf Create_Timer_Pool start\n");
  printf("nadaf Create_Timer_Pool timer_count = %d FreeTmrCount = %d\n",
         timer_count, FreeTmrCount);
  if (timer_count == 0) {
    fprintf(stdout, "\nTimer count is zero\n");
    return RTOS_MALLOC_ERR;
  }
  FreeTmrCount = timer_count;
  FreeTmrListPtr = GetRTOSTimer();
  RTOS_TMR *tempptr = FreeTmrListPtr;
  for (int i = 1; i < timer_count; i++) {
    tempptr->RTOSTmrNext = GetRTOSTimer();
    tempptr->RTOSTmrNext->RTOSTmrPrev = tempptr;
    tempptr = tempptr->RTOSTmrNext;
  }
  tempptr->RTOSTmrNext = NULL;
  printf("nadaf Create_Timer_Pool end\n");
  return RTOS_SUCCESS;
}

/*
   @ init_hash_table().
   Initialize the Hash table.
*/
void init_hash_table(void) {
  printf("nadaf init_hash_table start\n");
  printf("nadaf init_hash_table HASH_TABLE_SIZE = %d\n", HASH_TABLE_SIZE);
  for (int i = 0; i < HASH_TABLE_SIZE; i++) {
    hash_table[i].timer_count = 0;
    hash_table[i].list_ptr = NULL;
  }
  printf("nadaf init_hash_table end\n");
}

/*
  @ insert_hash_entry().
  Insert timer object in the Hash table.
*/
void insert_hash_entry(RTOS_TMR *timer_obj) {
  // Calculate the index using Hash function.
  int index = timer_obj->RTOSTmrMatch % 10;

  // Lock the resources.
  pthread_mutex_lock(&hash_table_mutex);

  // Add the entry.
  if (hash_table[index].timer_count == 0) {
    hash_table[index].list_ptr = timer_obj;
    timer_obj->RTOSTmrNext = NULL;
  } else {
    if ((hash_table[index].list_ptr->RTOSTmrMatch >= timer_obj->RTOSTmrMatch)) {
      hash_table[index].list_ptr->RTOSTmrPrev = timer_obj;
      timer_obj->RTOSTmrNext = hash_table[index].list_ptr;
      hash_table[index].list_ptr = timer_obj;
    } else {
      RTOS_TMR *tempTmr = hash_table[index].list_ptr;
      while (tempTmr->RTOSTmrNext != NULL &&
             tempTmr->RTOSTmrNext->RTOSTmrMatch < timer_obj->RTOSTmrMatch)
        tempTmr = tempTmr->RTOSTmrNext;
      if (tempTmr->RTOSTmrNext != NULL)
        tempTmr->RTOSTmrNext->RTOSTmrPrev = timer_obj;
      timer_obj->RTOSTmrNext = tempTmr->RTOSTmrNext;
      timer_obj->RTOSTmrPrev = tempTmr;
      tempTmr->RTOSTmrNext = timer_obj;
    }
  }

  hash_table[index].timer_count++;
  // Unlock resources.
  pthread_mutex_unlock(&hash_table_mutex);
}

/*
  @ remove_hash_entry().
  Remove the timer object entry from the Hash table.
*/
void remove_hash_entry(RTOS_TMR *timer_obj) {
  // Calculate the index using Hash function.
  int index = timer_obj->RTOSTmrMatch % 10;
  if (hash_table[index].timer_count == 0) {
    fprintf(stdout, "\nNo timers in hash table at index = %d\n", index);
    return;
  }
  // Lock resources.
  pthread_mutex_lock(&hash_table_mutex);
  // Remove the timer obj.
  RTOS_TMR *tempTmr = hash_table[index].list_ptr;
  while (tempTmr != timer_obj && tempTmr != NULL) {
    tempTmr = tempTmr->RTOSTmrNext;
  }
  // remove first obj.
  if (tempTmr->RTOSTmrPrev == NULL) {
    hash_table[index].list_ptr = tempTmr->RTOSTmrNext;
    if (tempTmr->RTOSTmrNext) { // it is not last obj.
      tempTmr->RTOSTmrNext->RTOSTmrPrev = tempTmr->RTOSTmrPrev;
    }
  } else {
    if (tempTmr->RTOSTmrNext) { // it is the middle obj.
      tempTmr->RTOSTmrNext->RTOSTmrPrev = tempTmr->RTOSTmrPrev;
    }
    tempTmr->RTOSTmrPrev->RTOSTmrNext = tempTmr->RTOSTmrNext;
  }
  timer_obj->RTOSTmrNext = NULL;
  timer_obj->RTOSTmrPrev = NULL;

  hash_table[index].timer_count--;
  // Unlock resources.
  pthread_mutex_unlock(&hash_table_mutex);
}

/*
  @ RTOSTmrTask().
  - Timer task to manage the running timers.
  - If the Timer is completed, call its Callback Function, remove the entry from
  Hash table.
  - If the Timer is Periodic then again insert it in the hash table change the
  state.
*/
void *RTOSTmrTask(void *temp) {

  while (1) {
    // Wait for signal from RTOSTmrSignal(), Once get the signal, increment the
    // timer tick counter.
    sem_wait(&timer_task_sem);

    // Check the whole list associated with the index of the Hash table.
    int index = RTOSTmrTickCtr % 10;
    // Compare each obj of linked list for timer completion.
    RTOS_TMR *tempTmr = hash_table[index].list_ptr;
    while (tempTmr != NULL) {
      RTOS_TMR *timer = tempTmr;
      tempTmr = tempTmr->RTOSTmrNext;
      if (timer->RTOSTmrMatch == RTOSTmrTickCtr) {
        timer->RTOSTmrCallback(timer->RTOSTmrCallbackArg);
        timer->RTOSTmrState = RTOS_TMR_STATE_COMPLETED;
        remove_hash_entry(timer);
        if (timer->RTOSTmrOpt == RTOS_TMR_ONE_SHOT) {
          free_timer_obj(timer);
        } else {
          timer->RTOSTmrMatch = RTOSTmrTickCtr + timer->RTOSTmrPeriod;
          timer->RTOSTmrState = RTOS_TMR_STATE_RUNNING;
          insert_hash_entry(timer);
        }
        continue;
      } else {
        break;
      }
    }
    RTOSTmrTickCtr++;
  }
  return temp;
}

/*
  @ RTOSTmrInit().
  Initialize the all timer attributes.
*/
void RTOSTmrInit(void) {
  INT8U retVal;
  INT32U timer_count = 0;
  pthread_attr_t attr;

  fprintf(
      stdout,
      "\n\nPlease Enter the number of Timers required in the Pool for the OS ");
  scanf("%d", &timer_count);

  // Create timer pool.
  retVal = Create_Timer_Pool(timer_count);

  // Check the return value.
  if (retVal != RTOS_SUCCESS) {
    fprintf(stdout, "\nTimer Creation failed Error = %d\n", retVal);
    return;
  }
  // Initialize Hash table.
  init_hash_table();
  fprintf(stdout, "\n\nHash Table Initialized Successfully\n");

  // Initialize Semaphore for timer task.
  sem_init(&timer_task_sem, 0, 0);

  // Initialize Mutex if any
  pthread_mutex_init(&timer_pool_mutex, NULL);

  // Create any thread if required for timer task.
  pthread_create(&thread, NULL, RTOSTmrTask, NULL);
  fprintf(stdout, "\nRTOS Initialization Done...\n");
}

/*
  @ alloc_timer_obj().
  Allocate a timer object from free timer pool.
*/
RTOS_TMR *alloc_timer_obj(void) {
  printf("nadaf alloc_timer_obj start");
  RTOS_TMR *tempTmr = NULL;
  // Lock resources.
  pthread_mutex_lock(&timer_pool_mutex);
  // Check for availability of timers.
  // Assign the timer object.
  printf("nadaf alloc_timer_obj FreeTmrCount = %d\n", FreeTmrCount);
  if (FreeTmrCount != 0) {
    printf("nadaf FreeTmrListPtr = %p\n", FreeTmrListPtr);
    tempTmr = FreeTmrListPtr;
    printf("nadaf FreeTmrListPtr->RTOSTmrNext = %p\n",
           FreeTmrListPtr->RTOSTmrNext);
    FreeTmrListPtr = FreeTmrListPtr->RTOSTmrNext;
    tempTmr->RTOSTmrNext = NULL;
    if (FreeTmrCount != 1)
      FreeTmrListPtr->RTOSTmrPrev = NULL;
    FreeTmrCount--;
  }
  // Unlock resources.
  pthread_mutex_unlock(&timer_pool_mutex);
  printf("nadaf alloc_timer_obj end");
  return tempTmr;
}

/*
  @ free_timer_obj().
  Free the allocated timer object and put it back into free pool.
*/
void free_timer_obj(RTOS_TMR *ptmr) {
  printf("nadaf free_timer_obj start ptmr = %p\n", ptmr);
  printf("nadaf free_timer_obj FreeTmrCount = %d\n", FreeTmrCount);
  // Lock resources.
  pthread_mutex_lock(&timer_pool_mutex);
  // Clear timer fields.
  ptmr->RTOSTmrCallback = NULL;
  ptmr->RTOSTmrCallbackArg = NULL;
  ptmr->RTOSTmrPrev = NULL;
  ptmr->RTOSTmrMatch = 0;
  ptmr->RTOSTmrDelay = 0;
  ptmr->RTOSTmrPeriod = 0;
  ptmr->RTOSTmrName = NULL;
  ptmr->RTOSTmrOpt = 0;
  ptmr->RTOSTmrNext = FreeTmrListPtr;
  // Change the state.
  ptmr->RTOSTmrState = RTOS_TMR_STATE_UNUSED;
  // Return the timer to free timer pool.
  FreeTmrListPtr = ptmr;
  FreeTmrCount++;
  // Unlock resources.
  pthread_mutex_unlock(&timer_pool_mutex);
  printf("nadaf free_timer_obj end\n");
}

/*
  @ OSTickInitialize().
  - Function to setup the Linux timer which will provide the clock tick
  interrupt to the timer manager module.
  - struct itimerspec:
    it_value specifies the initial amount of time before the timer expires.
    it_interval member specifies the interval after which the timer will expire
  again. A nonzero value for the it_interval member specifies a periodic time.
  - SIGALRM is an asynchronous signal. The SIGALRM signal is raised when a time
  interval specified in a call to the alarm or alarmd function expires.
  - The CLOCK_REALTIME clock measures the amount of time that has elapsed since
    00:00:00 January 1, 1970 Greenwich Mean Time (GMT).
*/
void OSTickInitialize(void) {
  printf("nadaf OSTickInitialize start\n");
  timer_t timer_id;
  struct itimerspec time_value;

  // To arm a timer to execute 1 second from now and then at
  // RTOS_CFG_TMR_TASK_RATE second intervals.
  time_value.it_value.tv_sec = 1;
  time_value.it_value.tv_nsec = 0;
  time_value.it_interval.tv_sec = 0;
  time_value.it_interval.tv_nsec = RTOS_CFG_TMR_TASK_RATE;

  signal(SIGALRM, &RTOSTmrSignal);

  // Create the timer object.
  timer_create(CLOCK_REALTIME, NULL, &timer_id);

  // Start timer.
  timer_settime(timer_id, 0, &time_value, NULL);
  printf("nadaf OSTickInitialize end\n");
}
