#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "process.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"

struct fileHandle {
  int fd;
  struct file *file;
  struct list_elem elem;
};

static struct list file_table;
int file_descriptor_num = 3;

int fileHandle_create(char *);
struct file *fileHandle_find(int fd);
void fileHandle_remove(int fd);

static void syscall_handler(struct intr_frame *);

void halt_handler(uint32_t *, struct intr_frame *);
void exec_handler(uint32_t *, struct intr_frame *);
void exit_handler(uint32_t *, struct intr_frame *);
void practice_handler(uint32_t *, struct intr_frame *);
void wait_handler(uint32_t *, struct intr_frame *);

// file syscall
void open_handler(uint32_t *, struct intr_frame *);
void create_handler(uint32_t *, struct intr_frame *);
void remove_handler(uint32_t *, struct intr_frame *);
void filesize_handler(uint32_t *, struct intr_frame *);
void read_handler(uint32_t *, struct intr_frame *);
void write_handler(uint32_t *, struct intr_frame *);

// helper mehods for matain file table
int fileHandle_create(char *name) {
  struct file *f = filesys_open(name);
  if (!f) return -1;
  struct fileHandle *h = malloc(sizeof(struct fileHandle));
  h->file = f;
  h->fd = file_descriptor_num;
  file_descriptor_num += 1;
  list_push_back(&file_table, &h->elem);
  return h->fd;
}

struct file *fileHandle_find(int fd) {
  if (list_size(&file_table) == 0) return NULL;
  struct list_elem *e;
  struct fileHandle *h;
  for (e = list_begin(&file_table); e != list_end(&file_table);
       e = list_next(e)) {
    h = list_entry(e, struct fileHandle, elem);
    if (h->fd == fd) break;
  }
  if (e != list_end(&file_table)) {
    return h->file;
  } else {
    return NULL;
  }
}

void halt_handler(uint32_t *args UNUSED, struct intr_frame *f UNUSED) {
  shutdown_power_off();
}

void exec_handler(uint32_t *args, struct intr_frame *f UNUSED) {
  check_vaild_pointer(f->esp, f->esp + 4);
  tid_t p = process_execute((char *)args[1]);
  f->eax = p;
}

void exit_handler(uint32_t *args, struct intr_frame *f) {
  check_vaild_pointer(f->esp, f->esp + 4);
  f->eax = args[1];
  printf("%s: exit(%d)\n", thread_current()->name, args[1]);
  thread_exit(args[1]);
}

void practice_handler(uint32_t *args, struct intr_frame *f) {
  check_vaild_pointer(f->esp, f->esp + 4);
  int t = get_user((uint8_t *)(args + 1));
  t += 1;
  f->eax = t;
}

void wait_handler(uint32_t *args, struct intr_frame *f) {
  check_vaild_pointer(f->esp, f->esp + 4);
  f->eax = process_wait(args[1]);
}

void create_handler(uint32_t *args, struct intr_frame *f) {
  // bool create (const char *file, unsigned initial size)
  char *file = args[1];
  off_t size = args[2];
  f->eax = filesys_create(file, size);
}

void remove_handler(uint32_t *args, struct intr_frame *f) {
  // bool remove (const char *file)
  bool ans;
  char *file = args[1];
  f->eax = filesys_remove(file);
}

void open_handler(uint32_t *args, struct intr_frame *f UNUSED) {
  // int open (const char *file)
  char *name = args[1];
  f->eax = fileHandle_create(name);
}

void filesize_handler(uint32_t *args, struct intr_frame *f) {
  // int filesize (int fd)
  struct file *file = fileHandle_find(args[1]);
  if (f != NULL) {
    f->eax = file_length(file);
  } else {
    f->eax = -1;
  }
}

void read_handler(uint32_t *args, struct intr_frame *f) {
  // int read (int fd, void *buffer, unsigned size)
  struct file *file = fileHandle_find(args[1]);
  void *buffer = args[2];
  unsigned int size = args[3];
  f->eax = file_read(file, buffer, size);
}

void write_handler(uint32_t *args, struct intr_frame *f) {
  // int write (int fd, const void *buffer, unsigned size)
  int fd = args[1];
  void *buffer = args[2];
  uint32_t size = (uint32_t)args[3];

  if (size >= 128) {
    // deny writing size bigger than 128
    f->eax = 0;
    return;
  }
  if (fd == 0) {
    return;
  } else if (fd == 1) {
    // write to console
    putbuf(buffer, size);
    f->eax = size;
  } else {
    struct file *file = fileHandle_find(fd);
    f->eax = file_write(file, buffer, size);
  }
}

void syscall_init(void) {
  list_init(&file_table);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler(struct intr_frame *f UNUSED) {
  uint32_t *args = ((uint32_t *)f->esp);
  check_vaild_pointer(f->esp, f->esp);
  // printf("System call number: %d\n", args[0]);
  if (args[0] == SYS_EXIT)
    exit_handler(args, f);
  else if (args[0] == SYS_PRACTICE)
    practice_handler(args, f);
  else if (args[0] == SYS_HALT)
    halt_handler(args, f);
  else if (args[0] == SYS_EXEC)
    exec_handler(args, f);
  else if (args[0] == SYS_WAIT)
    wait_handler(args, f);
  else if (args[0] == SYS_CREATE)
    create_handler(args, f);
  else if (args[0] == SYS_REMOVE)
    remove_handler(args, f);
  else if (args[0] == SYS_OPEN)
    open_handler(args, f);
  else if (args[0] == SYS_FILESIZE)
    filesize_handler(args, f);
  else if (args[0] == SYS_READ)
    read_handler(args, f);
  else if (args[0] == SYS_WRITE)
    write_handler(args, f);
  else if (args[0] == SYS_SEEK)
    return;
  else if (args[0] == SYS_TELL)
    return;
  else if (args[0] == SYS_CLOSE)
    return;
  else
    return;
}
