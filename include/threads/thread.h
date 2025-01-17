#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h" /*** GrilledSalmon ***/
#include "threads/interrupt.h"
#ifdef VM
#include "vm/vm.h"
#endif
#ifdef FILESYS
#include "filesys/directory.h"
#endif

/* States in a thread's life cycle. */
enum thread_status
{
	THREAD_RUNNING, /* Running thread. */
	THREAD_READY,	/* Not running but ready to run. */
	THREAD_BLOCKED, /* Waiting for an event to trigger. */
	THREAD_DYING	/* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t)-1) /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0	   /* Lowest priority. */
#define PRI_DEFAULT 31 /* Default priority. */
#define PRI_MAX 63	   /* Highest priority. */

/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread 
{
	/* Owned by thread.c. */
	tid_t tid;				   /* Thread identifier. */
	enum thread_status status; /* Thread state. */
	char name[16];			   /* Name (for debugging purposes). */
	int original_priority;	   /* Original priority before receive donation. */ /*** GrilledSalmon ***/
	int priority;			   /* Priority. */
	int64_t wakeup_tick;	   /* tick to wake up */
	struct lock *wait_on_lock; /* thread가 기다리고 있는 lock의 포인터 */ /*** GrilledSalmon ***/

	/* Shared between thread.c and synch.c. */
	struct list_elem elem;		/* List element. */

	/*** GrilledSalmon ***/
	struct list donator_list; 	/* priority donator가 저장되는 리스트 */
	struct list_elem d_elem; 	/* List element. */

    /*** hyeRexx ***/
    int nice;                       // nice value
    int recent_cpu;                 // recent cpu consumption
    struct list_elem i_elem;        // integtated list elements
	
#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4; /* Page map level 4 */ /* Page Table! */

	/*** team 8 : phase 2 ***/
	struct file **fdt;				// file descriptor table
	int fd_edge;					// file descriptor edge num
    
    /*** team 8 : phase 3 ***/
	struct thread *parent;
    struct list child_list;
    struct list_elem c_elem;
    int fork_flag;
    struct semaphore fork_sema;
    int exit_status;
    struct semaphore exit_sema;
    int is_exit; // likes thread_status

	/*** Deny Write on Executables ***/
	struct file *running_file;
#endif 

#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
	uintptr_t if_rsp; // eleshock
#endif

#ifdef FILESYS
	/* Jack */
	struct dir *working_dir;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf; /* Information for switching */
	unsigned magic;		  /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init(void);
void thread_start(void);

void thread_tick(void);
void thread_print_stats(void);

typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *);

void thread_block(void);
void thread_unblock(struct thread *);

struct thread *thread_current(void);
tid_t thread_tid(void);
const char *thread_name(void);

/* static variables and fuctions for sleeping and awakening */ /*** Jack ***/
void update_next_tick_to_awake(int64_t ticks);
int64_t get_next_tick_to_awake(void);

void thread_exit(void) NO_RETURN;
void thread_yield(void);
void thread_sleep(int64_t awake_ticks); /*** GrilledSalmon ***/
void thread_awake(int64_t ticks); /*** hyeRexx ***/

int thread_get_priority(void);
void thread_set_priority(int);
void refresh_priority(void);

int thread_get_nice(void);
void thread_set_nice(int);
int thread_get_recent_cpu(void);
int thread_get_load_avg(void);

void test_max_priority(void); /*** GrilledSalmon ***/

void do_iret(struct intr_frame *tf);

#endif /* threads/thread.h */

/*** JACK ***/
bool cmp_priority(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/*** Jack ***/
void donate_priority(void);
void refresh_donator_list(struct lock *lock);

#define NESTED_MAX_DEPTH 8

/*** GrilledSalmon ***/
void mlfqs_load_avg(void);
void mlfqs_increment(void);

/*** hyeRexx ***/
void mlfqs_recalc(void);
void mlfqs_priority(struct thread *t);

/*** Jack ***/
void mlfqs_recent_cpu(struct thread *t);
