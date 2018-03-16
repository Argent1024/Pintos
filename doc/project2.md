 Design document for proj2

Task1: Argument passing
	1. Create function get_user&put_user to get&write to users stack
	2. Create function argument_phraser to split the command line into parts
	3. Push the arguments to user stack after phraser

Task2: Syscall
	Trivial implement of practice, halt, exit
	
	For syscall wait and exec
	1. Change thread_exit(void) to thread_exit(int) where int saves the return status
	2. Create struct return_data saving the process id (thread id), return status.
	3. Add list to store return_data inside struct thread
		

	When creating a child thread, also malloc a place to hold the return_data inside parent's stack.
	And when a thread is exiting, store the value into the return_data, free the malloc place and tell 
	its children dont change return_data by making thread->father to null.
	(Actually it's better to set child->report which is the return_data of itself to null)
	And using locks to sychronize father and child process, child process hold two locks when created,
 	load_lock and return_lock. load_lock should be released inside start_process() right after load() 
	returns, and return_lock should be released inside thread_exit() after setting the return status.


Task3: File Syscall
	TODO

