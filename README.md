BROKEN PINTOS

======================

This repository contains code for CS162 group projects.

TODO list:





0. Check whether stack pointer is vaild
1. Change thread_block&unblock in process wait to a lock, also modify design document
2. Implement Exec
3. Let gcc don't complain about free(return_data)
4. Modify argument_pharser(rewrite it to make it looks better)

INF. Fix conflicts beetween user-proj and priority lock, schedule will
       crash if the thread is using virtual memory. See lock_release. 






**Design documents**

* [Project 1: Threads](doc/project1.md)
* [Project 2: User Programs](doc/project2.md)
* [Project 3: File System](doc/project3.md)

**Final reports**

* [Project 1: Threads](reports/project1.md)
* [Project 2: User Programs](reports/project2.md)
* [Project 3: File System](reports/project3.md)

