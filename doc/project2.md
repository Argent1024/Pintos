 Design document for proj2

Task1: Argument passing
	1. Create function get_user&put_user to get&write to users stack
	2. Create function argument_phraser to split the command line into parts
	3. Push the arguments to user stack after phraser

Task2: Syscall
	1. Trivial implement of practice, halt, exit
	
	For syscall wait
	2. Change thread_exit(void) to thread_exit(int) where int saves the return status
	3. Create struct return_data saving the process id (thread id), return status.
	4. Add list to store return_data inside struct thread
	
	When creating a child thread, also malloc a place to hold the return_data inside parent's stack.
	And when a thread is exiting, store the value into the return_data, free the malloc place and tell 
	its children dont change return_data by making thread->father to null.

Task3: File Syscall
	TODO

