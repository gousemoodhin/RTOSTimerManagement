RTOS Timer
==========
Code is compiled and tested in WINDOWS SUBSYSTEM FOR LINUX (UBUNTU).

Steps to Compile and Run code.
------------------------------
1) cd <project_folder>
2) make clean
3) make
4) ./TimerMgr


- Timer 1 gets invoked every 5 seconds and runs function1 which prints <print current time>
- Timer 2 gets invoked every 3 seconds and runs function1 which prints <print current time>
- Timer 3 gets invoked only once 10 seconds after it was created and runs function3 which prints <print current time>

NOTES:
------
1) The tick ISR occurs and assumes interrupts are enabled and executes.
2) The tick ISR signals the tick task that it is time for it to update timers.
3) The tick ISR terminates, however there might be higher priority tasks that need to execute (assuming the timer task has a lower  priority). Therefore, OS runs the higher priority task(s).
4) When all higher priority tasks have executed, OS switches to the timer task and determines that there are three timers that expired.
5) The callback for the first timer is executed.
6) The callback for the second expired timer is executed.
7) The callback for the third expired timer is executed.

- Execution of the callback functions is performed within the context of the timer task. 
  This means that the application code will need to make sure there is sufficient stack
  space for the timer task to handle these callbacks.
- The callback functions are executed one after the other based on the order they are found
  in the timer list.
- The execution time of the timer task greatly depends on how many timers expire and how long
  each of the callback functions takes to execute. Since the callbacks are provided by the
  application code they have a large influence on the execution time of the timer task.
- The timer callback functions must never wait on events because this would delay the timer task
  for excessive amounts of time, if not forever.
- Callbacks should execute as quickly as possible.
