#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "process.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler(struct intr_frame *);
static bool check_valid_pointer(uint32_t *esp);

void syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static bool check_valid_pointer(uint32_t *esp) {
  return 1;//esp <= (uint32_t)PHYS_BASE;
}

static void syscall_handler(struct intr_frame *f UNUSED) {
  uint32_t *args = ((uint32_t *)f->esp);
  // printf("System call number: %d\n", args[0]);
  if (args[0] == SYS_EXIT) {
    f->eax = args[1];
    printf("%s: exit(%d)\n", thread_current()->name, args[1]);
    thread_exit();
  } else if (args[0] == SYS_WRITE) {
    // write(args[1], (void *)args[2], args[3]);
    printf("%s", (char*)args[2]);
  } else if (args[0] == SYS_PRACTICE) {
    check_valid_pointer(args + 1);  // since we need to use args[2]
    int t = get_user((uint8_t*)(args + 1));
    t += 1;
    f->eax = t;
  }
}
