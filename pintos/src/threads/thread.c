#include "threads/thread.h"
#include <debug.h>
#include <random.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

static fixed_point_t fixed_PRI_MAX;

static fixed_point_t load_avg;
// stupid
static bool init_finish;
static int num_ready_threads;

/* ready queue struct for advanced schedule*/
// fuck..... their should be pri_max + 1 list.....
// spent 4 hours on weird behaviour by this...
static struct list advanced_ready_queue[PRI_MAX + 1];

// The more advance the queue, the longer the name.
/*
  So this is just n lists.
  queue[MIN] has priority MIN
            .
            .
            .
  queue[MAX] has priority MAX

  And stuff came earlier is before those came later.
  So just pop front should be fine with each list when calling next_thread,
  and put the current thread to the last and update information.

*/

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* List of all sleeping thread.
   When a thread_sleep() is called, push the thread into this,
   pop when timer interruput happens.
   TODO: change to queue maybe <--never do this fucking stupid stuff*/
static struct list thread_bed;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame {
  void *eip;             /* Return address. */
  thread_func *function; /* Function to call. */
  void *aux;             /* Auxiliary data for function. */
};

/* Statistics. */
static long long idle_ticks;   /* # of timer ticks spent idle. */
static long long kernel_ticks; /* # of timer ticks in kernel threads. */
static long long user_ticks;   /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4          /* # of timer ticks to give each thread. */
static unsigned thread_ticks; /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread(thread_func *, void *aux);

static void idle(void *aux UNUSED);
static struct thread *running_thread(void);
static struct thread *next_thread_to_run(void);
static void init_thread(struct thread *, const char *name, int priority,
                        fixed_point_t used_cpu);
static bool is_thread(struct thread *) UNUSED;
static void *alloc_frame(struct thread *, size_t size);
static void schedule(void);
void thread_schedule_tail(struct thread *prev);
static tid_t allocate_tid(void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void thread_init(void) {
  ASSERT(intr_get_level() == INTR_OFF);
  init_finish = 0;

  lock_init(&tid_lock);
  list_init(&ready_list);
  list_init(&all_list);
  list_init(&thread_bed);
  if (thread_mlfqs) {
    fixed_PRI_MAX = fix_int(PRI_MAX);
    num_ready_threads = 0;

    int i;  // gcc tells me not to init i inside for emmm
    for (i = PRI_MIN; i <= PRI_MAX; i++) {
      list_init(&advanced_ready_queue[i]);
    }
    load_avg = fix_int(0);
  }

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread();
  init_thread(initial_thread, "main", PRI_DEFAULT, fix_int(0));
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void thread_start(void) {
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init(&idle_started, 0);
  thread_create("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable();
  init_finish = 1;
  /* Wait for the idle thread to initialize idle_thread. */
  sema_down(&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void thread_tick(void) {
  struct thread *t = thread_current();

  /* Update statistics. */
  if (t == idle_thread) idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE) intr_yield_on_return();
}

/* Prints thread statistics. */
void thread_print_stats(void) {
  printf("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
         idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t thread_create(const char *name, int priority, thread_func *function,
                    void *aux) {
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT(function != NULL);

  /* Allocate thread. */
  t = palloc_get_page(PAL_ZERO);
  if (t == NULL) return TID_ERROR;

  /* Initialize thread. */
  init_thread(t, name, priority, thread_current()->recent_cpu);
  tid = t->tid = allocate_tid();

  /* Push child into father's list and remember father process
      And init the return data for child.
  */

#ifdef USERPROG
  enum intr_level old_level = intr_disable();
  struct thread *father = thread_current();
  struct return_data *rd = malloc(sizeof(struct return_data));
  rd->tid = t->tid;
  list_push_back(&father->child_return, &rd->elem);

  t->father = father;
  old_level = intr_disable();
  list_push_back(&father->child_process, &t->child_process_elem);
  list_push_back(&father->child_return, &rd->elem);
  intr_set_level(old_level);
#endif

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame(t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame(t, sizeof *ef);
  ef->eip = (void (*)(void))kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame(t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  num_ready_threads += 1;
  thread_unblock(t);
  thread_yield();
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_block(void) {
  ASSERT(!intr_context());
  ASSERT(intr_get_level() == INTR_OFF);
  thread_current()->status = THREAD_BLOCKED;
  schedule();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void thread_unblock(struct thread *t) {
  ASSERT(is_thread(t));
  ASSERT(t->status == THREAD_BLOCKED);
  // enum intr_level old_level;
  /*old_level = intr_disable();
  list_insert_ordered(&ready_list, &t->elem, less_priority_thread, NULL);
  t->status = THREAD_READY;
  intr_set_level(old_level);
  */
  add_ready_queue(t);
}

/* Returns the name of the running thread. */
const char *thread_name(void) { return thread_current()->name; }

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *thread_current(void) {
  struct thread *t = running_thread();

  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT(is_thread(t));
  ASSERT(t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t thread_tid(void) { return thread_current()->tid; }

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit(int return_status) {
  ASSERT(!intr_context());
  num_ready_threads -= 1;
  //#ifdef USERPROG
  struct thread *child = thread_current();

  struct list_elem *e;
  struct list *l;
  struct return_data *rd;
  // when father is not null, meaning the father is still exisiting
  enum intr_level old_level = intr_disable();
  struct thread *father = child->father;
  if (father != NULL) {
    // remove this thread inside father
    list_remove(&child->child_process_elem);
    l = &father->child_return;

    // put the return status
    for (e = list_begin(l); e != list_end(l); e = list_next(e)) {
      rd = list_entry(e, struct return_data, elem);
      if (rd->tid == child->tid) {
        rd->status = return_status;
        break;
      }
    }
    if (child->call_father) thread_unblock(father);
  }

  // remove children's father pointer
  struct thread *t;
  l = &child->child_process;
  for (e = list_begin(l); e != list_end(l); e = list_next(e)) {
    t = list_entry(e, struct thread, child_process_elem);
    t->father = NULL;
  }

  // free return_data in side self->child_return
  l = &child->child_return;
  while(!list_empty(l)) {
   e = list_pop_front(l);
   rd = list_entry(e, struct return_data, elem);
   free(rd);
  }


  intr_set_level(old_level);
  process_exit();
  //#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable();
  list_remove(&thread_current()->allelem);
  thread_current()->status = THREAD_DYING;
  schedule();
  NOT_REACHED();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void thread_yield(void) {
  struct thread *cur = thread_current();
  enum intr_level old_level;

  ASSERT(!intr_context());

  old_level = intr_disable();
  if (cur != idle_thread) add_ready_queue(cur);
  // list_insert_ordered(&ready_list, &cur->elem, less_priority_thread, NULL);
  cur->status = THREAD_READY;
  schedule();
  intr_set_level(old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void thread_foreach(thread_action_func *func, void *aux) {
  struct list_elem *e;

  ASSERT(intr_get_level() == INTR_OFF);

  for (e = list_begin(&all_list); e != list_end(&all_list); e = list_next(e)) {
    struct thread *t = list_entry(e, struct thread, allelem);
    func(t, aux);
  }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void thread_set_priority(int new_priority) {
  struct thread *t = thread_current();
  int old_priority = t->priority;

  if (old_priority == t->true_priority) {
    // there is no donation
    t->priority = new_priority;
    t->true_priority = new_priority;
    thread_yield();
  } else {
    t->true_priority = new_priority;
  }
}

/* Returns the current thread's priority. */
int thread_get_priority(void) {
  if (thread_mlfqs) {
    return thread_get_advanced_priority(thread_current());
  }
  return thread_current()->priority;
}

/* Sets the current thread's nice value to NICE. */
void thread_set_nice(int nice) { thread_current()->nice = nice; }

/* Returns the current thread's nice value. */
int thread_get_nice(void) { return thread_current()->nice; }

/* Returns 100 times the system load average. */
int thread_get_load_avg(void) { return fix_round(fix_scale(load_avg, 100)); }

void update_cpu_recent(struct thread *thread, void *aux UNUSED) {
  fixed_point_t t, s;
  t = fix_scale(load_avg, 2);
  s = fix_add(t, fix_int(1));
  t = fix_div(t, s);
  thread->recent_cpu =
      fix_add(fix_mul(t, thread->recent_cpu), fix_int(thread->nice));
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu(void) {
  /* Not yet implemented. */
  return fix_round(fix_scale(thread_current()->recent_cpu, 100));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void idle(void *idle_started_ UNUSED) {
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current();
  sema_up(idle_started);

  for (;;) {
    /* Let someone else run. */
    intr_disable();
    thread_block();

    /* Re-enable interrupts and wait for the next one.

       The `sti' instruction disables interrupts until the
       completion of the next instruction, so these two
       instructions are executed atomically.  This atomicity is
       important; otherwise, an interrupt could be handled
       between re-enabling interrupts and waiting for the next
       one to occur, wasting as much as one clock tick worth of
       time.

       See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
       7.11.1 "HLT Instruction". */
    asm volatile("sti; hlt" : : : "memory");
  }
}

/* Function used as the basis for a kernel thread. */
static void kernel_thread(thread_func *function, void *aux) {
  ASSERT(function != NULL);

  intr_enable();  /* The scheduler runs with interrupts off. */
  function(aux);  /* Execute the thread function. */
  thread_exit(0); /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *running_thread(void) {
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm("mov %%esp, %0" : "=g"(esp));
  return pg_round_down(esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool is_thread(struct thread *t) {
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void init_thread(struct thread *t, const char *name, int priority,
                        fixed_point_t used_cpu) {
  enum intr_level old_level;

  ASSERT(t != NULL);
  ASSERT(PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT(name != NULL);

  memset(t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy(t->name, name, sizeof t->name);
  t->stack = (uint8_t *)t + PGSIZE;
  t->priority = priority;
  t->true_priority = priority;
  t->magic = THREAD_MAGIC;
  t->ticksToWake = 0;
  t->recent_cpu = used_cpu;

  // init locks list
  list_init(&t->locks);

// init child process list
#ifdef USERPROG
  list_init(&t->child_process);
  list_init(&t->child_return);
#endif
  old_level = intr_disable();
  list_push_back(&all_list, &t->allelem);
  intr_set_level(old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *alloc_frame(struct thread *t, size_t size) {
  /* Stack data is always allocated in word-size units. */
  ASSERT(is_thread(t));
  ASSERT(size % sizeof(uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *next_thread_to_run(void) {
  if (thread_mlfqs) {
    int i;
    for (i = PRI_MAX; i >= PRI_MIN; i--) {
      struct list *l = &advanced_ready_queue[i];
      if (!list_empty(l)) {
        struct thread *t = list_entry(list_begin(l), struct thread, elem);
        list_remove(&t->elem);
        return t;
      }
    }
    return idle_thread;
  } else {
    if (list_empty(&ready_list))
      return idle_thread;
    else
      return list_entry(list_pop_front(&ready_list), struct thread, elem);
  }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void thread_schedule_tail(struct thread *prev) {
  struct thread *cur = running_thread();

  ASSERT(intr_get_level() == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) {
    ASSERT(prev != cur);
    palloc_free_page(prev);
  }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void schedule(void) {
  struct thread *cur = running_thread();
  struct thread *next = next_thread_to_run();
  struct thread *prev = NULL;

  ASSERT(intr_get_level() == INTR_OFF);
  ASSERT(cur->status != THREAD_RUNNING);
  ASSERT(is_thread(next));

  if (cur != next) prev = switch_threads(cur, next);
  thread_schedule_tail(prev);
}

/* Returns a tid to use for a new thread. */
static tid_t allocate_tid(void) {
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire(&tid_lock);
  tid = next_tid++;
  lock_release(&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof(struct thread, stack);

/* Less function for list_insert_ordered
   Used to compare two thread's priority in ready_list*/
bool less_priority_thread(const struct list_elem *a, const struct list_elem *b,
                          void *aux UNUSED) {
  struct thread *A = list_entry(a, struct thread, elem);
  struct thread *B = list_entry(b, struct thread, elem);
  // higher priority should be put in the front
  // it seems when equal the first come first run
  // And why A->priority >= B->priority will make error..???
  return A->priority > B->priority;
}

/* Less function for list_insert_ordered
    Used to compare two sleeping thread in thread_bed*/
bool less_sleeping_thread(const struct list_elem *a, const struct list_elem *b,
                          void *aux UNUSED) {
  struct thread *A = list_entry(a, struct thread, bedelem);
  struct thread *B = list_entry(b, struct thread, bedelem);
  return A->ticksToWake < B->ticksToWake;
}

void thread_goto_sleep(int64_t ticks, int64_t start) {
  if (ticks <= 0) {
    return;
  }
  struct thread *t = thread_current();
  ASSERT(t->status == THREAD_RUNNING);
  /* go to bed -- take the thread off ready list
     ONLY RUNNING THREAD ARE ALLOWED TO CALL THIS
  */

  // put stuff in sleeping_thread list
  t->ticksToWake = start + ticks;

  enum intr_level old_level = intr_disable();
  // reduce num_threads_ready
  num_ready_threads -= 1;
  t->status = THREAD_BLOCKED;
  if (list_empty(&thread_bed)) {
    list_push_front(&thread_bed, &t->bedelem);
    schedule();

  } else {
    list_insert_ordered(&thread_bed, &t->bedelem, less_sleeping_thread, NULL);
    schedule();
  }
  intr_set_level(old_level);
}

void wake_up_thread(int64_t ticks) {
  struct list_elem *e;
  struct thread *t;

  if (list_empty(&thread_bed)) {
    return;
  }

  enum intr_level old_level = intr_disable();
  for (e = list_begin(&thread_bed); e != list_end(&thread_bed);
       e = list_next(e)) {
    t = list_entry(e, struct thread, bedelem);
    if (t->ticksToWake <= ticks) {
      list_remove(e);
      /* // put thread into ready queue again
       t->status = THREAD_READY;
       list_insert_ordered(&ready_list, &t->elem, less_priority_thread, NULL);
       intr_set_level(old_level);*/
      add_ready_queue(t);
      num_ready_threads += 1;

    } else {
      break;
    }
  }
  intr_set_level(old_level);
}

bool donate_priority(struct thread *t, int priority) {
  // must donate when two's priority are equal to make sure
  // t be executed before cur!!
  if (t == NULL || priority < t->priority) {
    return 0;
  }
  enum intr_level old_level = intr_disable();
  t->priority = priority;

  /*reorder ready queue since priority has changed
    remove t and insert it back into ready queue,
    since current thread is at running state, t will be executed before cur
    */

  list_remove(&t->elem);
  list_insert_ordered(&ready_list, &t->elem, less_priority_thread, NULL);
  intr_set_level(old_level);
  return 1;
  // don't need to yield since it's going to yield just after return
}

void thread_lock_release(struct lock *lock) {
  struct thread *cur = thread_current();

  // remove (the) lock from thread's locks list
  enum intr_level old_level = intr_disable();
  list_remove(&lock->elem);

  // change back priority
  int new_priority = cur->true_priority;

  // check whether there are donaters waiting other locks
  struct list_elem *e;
  for (e = list_begin(&cur->locks); e != list_end(&cur->locks);
       e = list_next(e)) {
    struct lock *temp = list_entry(e, struct lock, elem);
    if (new_priority < temp->donater_priority) {
      new_priority = temp->donater_priority;
    }
  }
  if (cur->priority < new_priority) {
    // I think it shouldn't reach here.... but whatever
    cur->priority = -1;
  } else {
    cur->priority = new_priority;
  }
  intr_set_level(old_level);
}

// TODO: change every insert into and remove from ready_list

// put the thread into ready queue
void add_ready_queue(struct thread *thread) {
  if (thread_mlfqs) {
    enum intr_level old_level = intr_disable();
    int priority = thread_get_advanced_priority(thread);
    list_push_back(&advanced_ready_queue[priority], &thread->elem);
    thread->status = THREAD_READY;
    intr_set_level(old_level);
  } else {
    enum intr_level old_level = intr_disable();
    list_insert_ordered(&ready_list, &thread->elem, less_priority_thread, NULL);
    thread->status = THREAD_READY;
    intr_set_level(old_level);
  }
}

// get the specific thread out of the ready queue
void pop_ready_queue(struct thread *thread) {
  if (thread_mlfqs) {
    // should be called with interrupt off
    enum intr_level old_level = intr_disable();
    if (thread->status == THREAD_READY) list_remove(&thread->elem);
    intr_set_level(old_level);
  } else {
    enum intr_level old_level = intr_disable();
    list_remove(&thread->elem);
    intr_set_level(old_level);
  }
}

// just like the hash funciton =.=
int thread_get_advanced_priority(struct thread *t) {
  // return PRI_MAX - (recent_cpu / 4) - (nice / 2);
  fixed_point_t p;
  p = fix_int(t->nice);
  p = fix_add(fix_unscale(t->recent_cpu, 4), fix_scale(p, 2));
  int ans = fix_round(fix_sub(fixed_PRI_MAX, p));
  if (ans > PRI_MAX) {
    ans = PRI_MAX;
  } else if (ans < PRI_MIN) {
    ans = PRI_MIN;
  }
  return ans;
}

void ticks_update(bool update) {
  // update load_avg and recent_cpu
  struct thread *cur = thread_current();
  cur->recent_cpu = fix_add(cur->recent_cpu, fix_int(1));

  if (update && thread_mlfqs) {
    enum intr_level old_level = intr_disable();
    // update load_avg
    fixed_point_t t1, t2;
    /*
      fancy cheating condition
      change order of scale and unscale to make update perciser
      (since fixed-point is just stupid integer)
    */

    if (load_avg.f < 1000) {
      t1 = fix_unscale(load_avg, 60);
      t1 = fix_scale(t1, 59);
      t2 = fix_frac(num_ready_threads, 60);
      load_avg = fix_add(t1, t2);
    } else {
      t1 = fix_scale(load_avg, 59);
      t1 = fix_unscale(t1, 60);
      t2 = fix_frac(num_ready_threads, 60);
      load_avg = fix_add(t1, t2);
    }

    // update thread status, put threads change priority into ready_list
    // (just use this list instead of create a new one)
    int priority;
    struct list *l;
    struct list_elem *e;
    struct thread *thread = thread_current();

    // TODO delete this stupid for loop to make this function faster..
    // since it checks every list...

    // update the running thread
    update_cpu_recent(thread, NULL);
    for (priority = PRI_MAX; priority >= PRI_MIN; priority--) {
      l = &advanced_ready_queue[priority];
      for (e = list_begin(l); e != list_end(l);) {
        thread = list_entry(e, struct thread, elem);
        update_cpu_recent(thread, NULL);
        if (thread_get_advanced_priority(thread) != priority) {
          // 1hour bug emmmmm list_remove will crash list_next(e)
          // !!!!!!!!!!!!!!!!!!!!!!!
          e = list_next(e);
          // remove thread from ready queue
          list_remove(&thread->elem);  // suppose to be useless
          // put every thread need to move into ready list
          list_push_back(&ready_list, &thread->elem);
          continue;
        }
        e = list_next(e);
      }
    }
    // update sleeping threads
    for (e = list_begin(&thread_bed); e != list_end(&thread_bed);
         e = list_next(e)) {
      thread = list_entry(e, struct thread, bedelem);
      update_cpu_recent(thread, NULL);
    }

    l = &ready_list;
    for (e = list_begin(l); e != list_end(l);) {
      thread = list_entry(e, struct thread, elem);
      e = list_next(e);
      list_remove(&thread->elem);  // suppose to be useless
      add_ready_queue(thread);
    }
    intr_set_level(old_level);
  }
}
