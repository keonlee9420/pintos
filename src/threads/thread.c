#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif
<<<<<<< HEAD
/* Project1-Thread Implementation */
<<<<<<< HEAD
#include <inttypes.h>
=======
#include "threads/malloc.h"
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
/* Project1-Thread Implementation End */
=======
>>>>>>> ba83a3e... daoate priority by store in donor list

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
/* Project1-Thread Implementation*/
/* List of sleeping threads */
struct list sleeping_list;

static void priority_ready(struct thread* t);
static void priority_new(struct thread* t);
/* Project1-Thread Implementation End */
=======
=======
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
/* Implement Initializer from Project1-Thread */

/* List of sleeping threads */
struct list sleeping_list;

/* End of Project1-Thread Initializer Implementation */
<<<<<<< HEAD
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
/* Project1-Thread Implementation*/
/* List of sleeping threads */
struct list sleeping_list;

static void priority_ready(struct thread* t);
static void priority_new(struct thread* t);
/* Project1-Thread Implementation End */
>>>>>>> 2cf278d... order ready list based on priority

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
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
	/* Project1-Thread Implementation */
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
	list_init(&sleeping_list);
=======
	list_init (&sleeping_list);
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
	list_init (&sleeping_list);
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
	list_init(&sleeping_list);
>>>>>>> ba83a3e... daoate priority by store in donor list
	/* Project1-Thread Implementation End */

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
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
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
<<<<<<< HEAD
<<<<<<< HEAD
	/* Project1-Thread Implementation */
  priority_ready(t);
	t->status = THREAD_READY;
	if(thread_current() != idle_thread && 
			thread_current()->priority < t->priority)
		thread_yield();
	/* Project1-Thread Implementation End */
=======
=======
	/* Project1-Thread Implementation */
>>>>>>> ba83a3e... daoate priority by store in donor list
  priority_ready(t);
	t->status = THREAD_READY;
	if(thread_current() != idle_thread && 
			thread_current()->priority < t->priority)
		thread_yield();
<<<<<<< HEAD
	}
>>>>>>> 2cf278d... order ready list based on priority
=======
	/* Project1-Thread Implementation End */
>>>>>>> ba83a3e... daoate priority by store in donor list
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
 
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
<<<<<<< HEAD
<<<<<<< HEAD
		/* Project1-Thread Implementation */
    priority_ready(cur);
		/* Project1-Thread Implementation End */
=======
    priority_ready(cur);
>>>>>>> 2cf278d... order ready list based on priority
=======
		/* Project1-Thread Implementation */
    priority_ready(cur);
		/* Project1-Thread Implementation End */
>>>>>>> ba83a3e... daoate priority by store in donor list
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
<<<<<<< HEAD
<<<<<<< HEAD
	thread_current ()->initpriority = new_priority;
	priority_new(thread_current());
=======
	thread_current ()->priority = new_priority;
>>>>>>> 2cf278d... order ready list based on priority
=======
	thread_current ()->initpriority = new_priority;
	priority_new(thread_current());
>>>>>>> ba83a3e... daoate priority by store in donor list
	thread_yield();
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) 
{
  /* Not yet implemented. */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  /* Not yet implemented. */
  return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  /* Not yet implemented. */
  return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

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
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
	/* Project1-Thread Implementation */
	t->initpriority = priority;
<<<<<<< HEAD
<<<<<<< HEAD
	t->waitstatus = 0;
=======
>>>>>>> ba83a3e... daoate priority by store in donor list
=======
	t->waitstatus = 0;
>>>>>>> dd2f104... Store waiting status by adding attribute
	list_init(&t->donor_list);
	/* Project1-Thread Implementation End */
  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
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
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;
	
	/* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
	struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/* Project1-Thread Implementation */
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
=======
=======
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct

struct sleeping_thread
	{
		struct thread* thread;
		int64_t wakeup_tick;
		struct list_elem elem;
	};

/* Compare two thread by their element, then return smaller-valued thread */
static bool 
is_earlier_than(const struct list_elem *a, const struct list_elem* b, void* aux UNUSED)
{
	struct sleeping_thread* st1 = list_entry(a, struct sleeping_thread, elem);
	struct sleeping_thread* st2 = list_entry(b, struct sleeping_thread, elem);
	return st1->wakeup_tick < st2->wakeup_tick;
}

<<<<<<< HEAD
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
>>>>>>> f935b78... implement alarm by in-thread list
=======
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
/* Find place that can insert thread in ascending value of MEMBER value */
#define insert_thread(LIST, ELEM, THREAD, MEMBER, OP)																\
			ASSERT(intr_get_level() == INTR_OFF);																					\
			for(ELEM = list_begin(&LIST); ELEM != list_end(&LIST); ELEM = list_next(e))		\
				if(list_entry(ELEM, struct thread, elem)->MEMBER OP THREAD->MEMBER)					\
					break;																																		\
			list_insert(ELEM, &THREAD->elem);

>>>>>>> 2cf278d... order ready list based on priority
=======
>>>>>>> ba83a3e... daoate priority by store in donor list
/* Insert into sleeping thread list, then blocks current thread */
void 
thread_sleep(int64_t tick)
{
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
	enum intr_level old_level;
	/* Create sleeping thread list element */
	struct thread* cur = thread_current();
	cur->wakeup_tick = tick;
	
	/* Insert into sleeping list, ascending order */
	old_level = intr_disable();
	insert_thread(sleeping_list, cur, elem, wakeup_tick, >);

	/* Block thread */
=======
=======
	struct list_elem* e;
=======
>>>>>>> ba83a3e... daoate priority by store in donor list
	enum intr_level old_level;
>>>>>>> f935b78... implement alarm by in-thread list
	/* Create sleeping thread list element */
	struct thread* cur = thread_current();
	cur->wakeup_tick = tick;
	
<<<<<<< HEAD
/* Block thread */
	enum intr_level old_level = intr_disable();
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
	/* Insert into sleeping list, ascending order */
	old_level = intr_disable();
	insert_thread(sleeping_list, cur, elem, wakeup_tick, >);

	/* Block thread */
>>>>>>> f935b78... implement alarm by in-thread list
=======
	/* Create sleeping thread list element */
	struct sleeping_thread* st = (struct sleeping_thread*)malloc(sizeof(struct sleeping_thread));
	st->thread = thread_current();
	st->wakeup_tick = tick;
	
	/* Insert into sleeping list */
	list_insert_ordered(&sleeping_list, &st->elem, is_earlier_than, NULL);
	
/* Block thread */
	enum intr_level old_level = intr_disable();
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
	thread_block();
	intr_set_level(old_level);
}

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
/* Search for every thread which needs to wake up from beginning of list,
=======
/* Search for every thread which needs to wake up,
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
/* Search for every thread which needs to wake up,
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
/* Search for every thread which needs to wake up from beginning of list,
>>>>>>> 2cf278d... order ready list based on priority
	 remove wakeup thread from sleeping list,
	 then unblock wakeup thread */
void 
thread_wake(int64_t cur_tick)
{
<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
	/* Search from front of sleeping list, which needs to wake up earliest */
	struct list_elem* e;
	enum intr_level old_level = intr_disable();
	for(e = list_begin(&sleeping_list); e != list_end(&sleeping_list); 
			e = list_begin(&sleeping_list))
	{
		struct thread* t = list_entry(e, struct thread, elem);
		/* Stop if time is not enough to wake any thread up */
		if(t->wakeup_tick > cur_tick)
			break;
		/* remove thread from sleeping list, then unblock it */
		list_remove(&t->elem);
		thread_unblock(t);
=======
	enum intr_level old_level = intr_disable();
=======
>>>>>>> f935b78... implement alarm by in-thread list
	/* Search from front of sleeping list, which needs to wake up earliest */
	struct list_elem* e;
	enum intr_level old_level = intr_disable();
	for(e = list_begin(&sleeping_list); e != list_end(&sleeping_list); 
			e = list_begin(&sleeping_list))
	{
		struct thread* t = list_entry(e, struct thread, elem);
		if(t->wakeup_tick > cur_tick)
			break;
<<<<<<< HEAD
<<<<<<< HEAD
=======
	enum intr_level old_level = intr_disable();
	/* Search from front of sleeping list, which needs to wake up earliest */
	struct list_elem* e = list_begin(&sleeping_list);
	while(e != list_end(&sleeping_list)){
		struct sleeping_thread* st = list_entry(e, struct sleeping_thread, elem);
		/* Return if no thread needs to wake up */
		if(st->wakeup_tick > cur_tick)
			break;
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
		/* unblock wakeup thread */
		thread_unblock(st->thread);
		/* remove wakeup thread from sleeping list, 
			 and free sleeping thread list element */
		list_remove(e);
		free(st);
		/* set new front element of sleeping list */
		e = list_begin(&sleeping_list);
<<<<<<< HEAD
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
		list_remove(&t->elem);
		thread_unblock(t);
>>>>>>> 2cf278d... order ready list based on priority
	}
	intr_set_level(old_level);
}

<<<<<<< HEAD
<<<<<<< HEAD
/* Insert thread into ready list, in priority-descending order */
static void 
priority_ready(struct thread* t)
{
	enum intr_level old_level = intr_disable();
	insert_thread(ready_list, t, elem, priority, <);
	intr_set_level(old_level);
}

/* Donate priority of current thread to HOLDER thread.
	 Step 1: Insert current thread into HOLDER's donor list
	 Step 2: Set new priority of HOLDER
	 Step 3: Unblock if HOLDER is blocked for nesting donation,
						or re-insert HOLDER into ready list according to new priority */
void 
priority_donate(struct thread* holder)
{
	enum intr_level old_level = intr_disable();
	/* Step 1: Insert current thread into donor list */
	insert_thread(holder->donor_list, thread_current(), dntelem, priority, <);

	/* Step 2: Set new priority */
	priority_new(holder);

	/* Step 3: Unblock or rearrange HOLDER */
	switch(holder->waitstatus)
	{
		case 0: 	/* Not waiting any synchronization */
			list_remove(&holder->elem);
			priority_ready(holder);
			break;
		case 1:		/* Waiting for lock */
			list_remove(&holder->dntelem);
		case 2:		/* Waiting for sema */
			list_remove(&holder->elem);
			thread_unblock(holder);
			break;
=======
>>>>>>> f935b78... implement alarm by in-thread list
=======
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
	}
	intr_set_level(old_level);
}

<<<<<<< HEAD
<<<<<<< HEAD
/* Remove priority donation from thread T 
	 Step 1: Remove every thread in LIST from donor list of current thread
	 Step 2: Reset priority of current thread */
void 
priority_release(struct list* waiters)
{
	struct list_elem* e;
	enum intr_level old_level = intr_disable();
	/* Step 1: Remove every thread in LIST from donor list */
	for(e = list_begin(waiters); e != list_end(waiters); e = list_next(e))
		list_remove(&list_entry(e, struct thread, elem)->dntelem);
	
	/* Step 2: Reset priority */
	priority_new(thread_current());
	intr_set_level(old_level);
}

/* Set priority of thread T 
	 Compare its initial priority and largest value of donated priority,
	 then set larger one to T's priority */
static void 
priority_new(struct thread* t)
{
	/* Set to initiatl priority if nothing donated */
	if(list_empty(&t->donor_list))
		t->priority = t->initpriority;
	/* Set max(initial priority, max donated priority) to T's priority */
	else
	{
		int donate_priority = list_entry(list_front(&t->donor_list), 
																	struct thread, dntelem)->priority;
		t->priority = t->initpriority < donate_priority ? 
										donate_priority : t->initpriority;
	}
}

/* Set current thread's waiting status 
	 0: Not waiting any synchronization
	 1: Waiting for lock
	 2: Waiting for semaphore
*/

void 
thread_set_waitstat(struct thread* t, uint8_t stat)
{
	t->waitstatus = stat;
}
=======
=======
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct


=======
=======
/* Insert thread into ready list, in priority-descending order */
>>>>>>> ba83a3e... daoate priority by store in donor list
static void 
priority_ready(struct thread* t)
{
	enum intr_level old_level = intr_disable();
	insert_thread(ready_list, t, elem, priority, <);
	intr_set_level(old_level);
}
>>>>>>> 2cf278d... order ready list based on priority

/* Donate priority of current thread to HOLDER thread.
	 Step 1: Insert current thread into HOLDER's donor list
	 Step 2: Set new priority of HOLDER
	 Step 3: Unblock if HOLDER is blocked for nesting donation,
						or re-insert HOLDER into ready list according to new priority */
void 
priority_donate(struct thread* holder)
{
	enum intr_level old_level = intr_disable();
	/* Step 1: Insert current thread into donor list */
	insert_thread(holder->donor_list, thread_current(), dntelem, priority, <);

	/* Step 2: Set new priority */
	priority_new(holder);

	/* Step 3: Unblock or rearrange HOLDER */
	switch(holder->waitstatus)
	{
		case 0: 	/* Not waiting any synchronization */
			list_remove(&holder->elem);
			priority_ready(holder);
			break;
		case 1:		/* Waiting for lock */
			list_remove(&holder->dntelem);
		case 2:		/* Waiting for sema */
			list_remove(&holder->elem);
			thread_unblock(holder);
			break;
	}
	intr_set_level(old_level);
}

/* Remove priority donation from thread T 
	 Step 1: Remove every thread in LIST from donor list of current thread
	 Step 2: Reset priority of current thread */
void 
priority_release(struct list* waiters)
{
	struct list_elem* e;
	enum intr_level old_level = intr_disable();
	/* Step 1: Remove every thread in LIST from donor list */
	for(e = list_begin(waiters); e != list_end(waiters); e = list_next(e))
		list_remove(&list_entry(e, struct thread, elem)->dntelem);
	
	/* Step 2: Reset priority */
	priority_new(thread_current());
	intr_set_level(old_level);
}

<<<<<<< HEAD
<<<<<<< HEAD
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
>>>>>>> e28d30a... Implement alarm by list of speeling_thread struct
=======
/* Set priority of thread T 
	 Compare its initial priority and largest value of donated priority,
	 then set larger one to T's priority */
static void 
priority_new(struct thread* t)
{
	/* Set to initiatl priority if nothing donated */
	if(list_empty(&t->donor_list))
		t->priority = t->initpriority;
	/* Set max(initial priority, max donated priority) to T's priority */
	else
	{
		int donate_priority = list_entry(list_front(&t->donor_list), 
																	struct thread, dntelem)->priority;
		t->priority = t->initpriority < donate_priority ? 
										donate_priority : t->initpriority;
	}
}
>>>>>>> ba83a3e... daoate priority by store in donor list

/* Set current thread's waiting status 
	 0: Not waiting any synchronization
	 1: Waiting for lock
	 2: Waiting for semaphore
*/

void 
thread_set_waitstat(struct thread* t, uint8_t stat)
{
	t->waitstatus = stat;
}

