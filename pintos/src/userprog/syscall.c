#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);
static bool check_valid_pointer(uint32_t *esp);

void halt_handler(uint32_t *, struct intr_frame *);
void exec_handler(uint32_t *, struct intr_frame *);
void exit_handler(uint32_t *, struct intr_frame *);
void practice_handler(uint32_t *, struct intr_frame *);

void halt_handler(uint32_t *args UNUSED, struct intr_frame *f UNUSED) {
  shutdown_power_off();
}

void exec_handler(uint32_t *args, struct intr_frame *f UNUSED) {
  tid_t p = process_execute((char *)args[1]);
  f->eax = p;
}

void exit_handler(uint32_t *args, struct intr_frame *f) {
  f->eax = args[1];
  printf("%s: exit(%d)\n", thread_current()->name, args[1]);
  thread_exit();
}

void practice_handler(uint32_t *args, struct intr_frame *f) {
  int t = get_user((uint8_t *)(args + 1));
  t += 1;
  f->eax = t;
}

void syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool check_valid_pointer(uint32_t *esp) {
  return 1;  // esp <= (uint32_t)PHYS_BASE;
}

static void syscall_handler(struct intr_frame *f UNUSED) {
  uint32_t *args = ((uint32_t *)f->esp);
  // printf("System call number: %d\n", args[0]);
  if (args[0] == SYS_EXIT)
    exit_handler(args, f);
  else if (args[0] == SYS_WRITE)
    printf("%s", (char *)args[2]);
  else if (args[0] == SYS_PRACTICE)
    practice_handler(args, f);
  else if (args[0] == SYS_HALT)
    halt_handler(args, f);
  else if (args[0] == SYS_EXEC)
    exec_handler(args, f);
  else
    return;
}
