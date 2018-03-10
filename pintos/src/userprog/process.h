#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

bool put_user(uint8_t *, uint8_t);
bool push_pointer(void **, void *);
bool push_string(void **esp, char *string);
int get_user(uint8_t *uaddr);

#endif /* userprog/process.h */
