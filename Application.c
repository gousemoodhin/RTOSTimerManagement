/*
  Author- Gousemoodhin Nadaf.
  - Application file to Demonstrate the usage of Timer Manager.
  - Design a timer management system for the Real Time Operating System.
  - Create 3 timers that register handles "function1", "function2" and "function3", respectively.
    Timer 1 gets invoked every 5 seconds and runs function1 which prints current time.
    Timer 2 gets invoked every 3 seconds and runs function1 which prints current time.
    Timer 3 gets invoked only once 10 seconds after it was created and which prints current time.
*/

// Include header files.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "TypeDefines.h"
#include "TimerMgrHeader.h"
#include "TimerAPI.h"

/*
  @ print_time_msg().
  - Function to print the Time and message in callback function.
  - Function to print the message for each Timer like below,
    "This is Function 2 and UTC time and date: Thu Oct 27 20:53:27 2016"
*/
void print_time_msg(int num) {
  time_t rawtime;
  time(&rawtime);
  fprintf(stdout, "\nThis is Function %d and UTC time and date: %s\n", num, asctime(gmtime(&rawtime)));
}

void print_program_info(void) {
  fprintf(stdout, "\n\n\n\nTimer Manager Project");
  fprintf(stdout, "\n=====================");
  fprintf(stdout, "\n\nCreated by: GOUSEMOODHIN NADAF");
  fprintf(stdout, "\n\n-> This Program will initialize the timers and creates Timer Task as Thread");
  fprintf(stdout, "\n-> It creates 3 Timers");
  fprintf(stdout, "\n\tTimer1 - Periodic 5 second");
  fprintf(stdout, "\n\tTimer2 - Periodic 3 second");
  fprintf(stdout, "\n\tTimer3 - One Shot 10 second");
  fprintf(stdout, "\n\nPress Enter to start the Program...\n");
  getchar();
}

void function1(void *arg) {
  print_time_msg(1);
}

void function2(void *arg) {
  print_time_msg(2);
}

void function3(void *arg) {
  print_time_msg(3);
}

int main(void) {
  INT8U err_val = RTOS_ERR_NONE;
  
  RTOS_TMR *timer_obj1 = NULL;
  RTOS_TMR *timer_obj2 = NULL;
  RTOS_TMR *timer_obj3 = NULL;

  INT8 *timer_name[3] = {"Timer1", "Timer2", "Timer3"};

  // Display the program info.
  print_program_info();

  // Initialize the OS tick.
  OSTickInitialize();

  fprintf(stdout, "OS Tick Initialization completed successfully");

  // Initialize the RTOS timer.
  RTOSTmrInit();

  fprintf(stdout, "\nApplication Started....... :-)\n");

  // ================================================================
  // Timer Creation
  // ================================================================
  // Create Timer1.
  // Provide the required arguments in the function call.
  timer_obj1 = RTOSTmrCreate(0, 50, RTOS_TMR_PERIODIC, function1, NULL, timer_name[0], &err_val);
  // Check the return value and determine if it created successfully or not.
  if (err_val != RTOS_ERR_NONE) {
    fprintf(stdout, "\n Create Timer1 failed, Error: %d timer_obj1 = %p\n", err_val, timer_obj1);
    return 0;
  }

  // Create Timer2.
  // Provide the required arguments in the function call.
  timer_obj2 = RTOSTmrCreate(0, 30, RTOS_TMR_PERIODIC, function2, NULL, timer_name[1], &err_val);
  // Check the return value and determine if it created successfully or not.
  if (err_val != RTOS_ERR_NONE) {
    fprintf(stdout, "\n Create Timer2 failed, Error: %d timer_obj2 = %p\n", err_val, timer_obj2);
    return 0;
  }

  // Create Timer3.
  // Provide the required arguments in the function call.
  timer_obj3 = RTOSTmrCreate(100, 0, RTOS_TMR_ONE_SHOT, function3, NULL, timer_name[2], &err_val);
  // Check the return value and determine if it created successfully or not.
  if (err_val != RTOS_ERR_NONE) {
    fprintf(stdout, "\n Create Timer3 failed, Error: %d timer_obj3 = %p\n", err_val, timer_obj3);
    return 0;
  }

  // ================================================================
  // Starting Timer
  // ================================================================
  // Start Timer1.
  if (timer_obj1) {
    RTOSTmrStart(timer_obj1, &err_val);
    // Check the return value and determine if it started successfully or not.
    if (err_val != RTOS_ERR_NONE) {
      fprintf(stdout, "\n Start Timer1 failed, Error: %d\n", err_val);
      return 0;
    }
  }

  // Start Timer2.
  if (timer_obj2) {
    RTOSTmrStart(timer_obj2, &err_val);
    // Check the return value and determine if it started successfully or not.
    if (err_val != RTOS_ERR_NONE) {
      fprintf(stdout, "\n Start Timer2 failed, Error: %d\n", err_val);
      return 0;
    }
  }

  // Start Timer3.
  if (timer_obj3) {
    RTOSTmrStart(timer_obj3, &err_val);
    // Check the return value and determine if it started successfully or not.
    if (err_val != RTOS_ERR_NONE) {
      fprintf(stdout, "\n Start Timer3 failed, Error: %d\n", err_val);
      return 0;
    }
  }

  fprintf(stdout, "\n\nType 'q' and hit enter to end the program = %c\n", getchar());
  while(getchar() != 'q')

  // Delete Timers
  if (timer_obj1 && (RTOSTmrDel(timer_obj1, &err_val) == RTOS_FALSE || err_val == RTOS_ERR_TMR_INVALID)) {
    fprintf(stdout, "\n Delete Timer1 failed, Error: %d timer_obj1 = %p\n", err_val, timer_obj1);
  }
  if (timer_obj2 && (RTOSTmrDel(timer_obj2, &err_val) == RTOS_FALSE || err_val == RTOS_ERR_TMR_INVALID)) {
    fprintf(stdout, "\n Delete Timer2 failed, Error: %d timer_obj2 = %p\n", err_val, timer_obj2);
  }
  if (timer_obj3 && (RTOSTmrDel(timer_obj3, &err_val) == RTOS_FALSE || err_val == RTOS_ERR_TMR_INVALID)) {
    fprintf(stdout, "\n Delete Timer3 failed, Error: %d timer_obj3 = %p\n", err_val, timer_obj3);
  }

  return 0;
}

