Task4: Task4 is configured for priority 4 (highest of all the application tasks). It remains blocked on a semaphore which is released when a user switch is pressed. Once active, the task runs for about 10 ticks, before again blocking itself until the switch is pressed again. Each time task4 is run, it prints a message “Tsk4-P4 <-“ where, the <- symbol indicates that task4 is running. Just before blocking it prints another message “Tsk4-P4 ->” where, the -> symbol indicates that task4 is about to put itself into a blocked state.



Success Criteria:

GitHub
Create a GitHub account and clone aircable/EFE_project into your repo. Then create a new repo for every assignment

Task Sync Example
Implement the 4 task priority example with the help of ChatGPT or other AI to print out exactly what required.
