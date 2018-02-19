Design Document for Project 1: Threads
======================================

## Group Members

* FirstName LastName <email@domain.example>
* FirstName LastName <email@domain.example>
* FirstName LastName <email@domain.example>
* FirstName LastName <email@domain.example>



1. Efficient Alarm Clock
	 Creating a list contains all sleeping threads
	 When a thread calls timer_sleep() , put that 
	 threads into sleeping_list, ordered by the 
	 ticks it should wake up.
	 Inside timer interrupt handler, check the first
	 threads in the sleeping list, wakes it up if
	 needed.

2. Priority Schedule
	Keep ready_list ordered all the time by threads'
	priority. So the next threads going to run will
	always has the highest priority

	TODO
