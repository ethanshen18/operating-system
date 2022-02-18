/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

#define N_LORD_FLOWERKILLER 8
#define NROPES 16
static volatile int ropes_left;
static int dandelion_exit;
static int marigold_exit;
static int balloon_exit;
static volatile int flowerkiller_exit;

/* Data structures for rope mappings */

struct rope {
	volatile int hook, stake;
	volatile bool connected;
	struct lock *rope_lock;
};

struct rope *ropes[NROPES];

/* Synchronization primitives */

static struct lock *print_lock;
static struct lock *count_lock;

/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 */

static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	lock_acquire(print_lock);
	kprintf("Dandelion thread starting\n");
	lock_release(print_lock);

	// attempt to sever when there are still ropes left
	while (ropes_left > 0) {

		// select random rope
		int i = random() % NROPES;
		lock_acquire(ropes[i]->rope_lock);

		// attempt to sever when the rope is connected
		if (ropes[i]->connected) {
			lock_acquire(count_lock);
			ropes[i]->connected = false;
			ropes_left--;
			lock_acquire(print_lock);
			kprintf("Dandelion severed rope %d\n", ropes[i]->hook);
			lock_release(print_lock);
			lock_release(count_lock);
		}
		lock_release(ropes[i]->rope_lock);
		thread_yield();
	}

	lock_acquire(print_lock);
	kprintf("Dandelion thread done\n");
	lock_release(print_lock);

	// track thread exit
	dandelion_exit++;
}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	lock_acquire(print_lock);
	kprintf("Marigold thread starting\n");
	lock_release(print_lock);

	// attempt to sever when there are still ropes left
	while (ropes_left > 0) {

		// select random rope
		int i = random() % NROPES;
		lock_acquire(ropes[i]->rope_lock);

		// attempt to sever when the rope is connected
		if (ropes[i]->connected) {
			lock_acquire(count_lock);
			ropes[i]->connected = false;
			ropes_left--;
			lock_acquire(print_lock);
			kprintf("Marigold severed rope %d from stake %d\n", ropes[i]->hook, ropes[i]->stake);
			lock_release(print_lock);
			lock_release(count_lock);
		}
		lock_release(ropes[i]->rope_lock);
		thread_yield();
	}

	lock_acquire(print_lock);
	kprintf("Marigold thread done\n");
	lock_release(print_lock);

	// track thread exit
	marigold_exit++;
}

static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	lock_acquire(print_lock);
	kprintf("Lord FlowerKiller thread starting\n");
	lock_release(print_lock);

	// attempt to swap when there are more than one rope left
	while (ropes_left > 1) {

		// select two random ropes
		int i = random() % NROPES;
		int j = random() % NROPES;

		// do not swap the same rope with itself
		if (i == j) continue;

		// acquire the lower valued rope first to avoid deadlocks
		if (i > j) {
			int temp = i;
			i = j;
			j = temp;
		}

		// acquire the two rope locks
		lock_acquire(ropes[i]->rope_lock);
		lock_acquire(ropes[j]->rope_lock);

		// only swap when both ropes are still connected
		if (ropes[i]->connected && ropes[j]->connected) {

			// swap stake number inside the rope struct
			int temp = ropes[i]->stake;
			ropes[i]->stake = ropes[j]->stake;
			ropes[j]->stake = temp;

			// print messages for both ropes
			lock_acquire(print_lock);
			kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", 
				ropes[i]->hook, 
				ropes[j]->stake, 
				ropes[i]->stake);
			kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", 
				ropes[j]->hook, 
				ropes[i]->stake, 
				ropes[j]->stake);
			lock_release(print_lock);
		}
		lock_release(ropes[i]->rope_lock);
		lock_release(ropes[j]->rope_lock);
		thread_yield();
	}

	lock_acquire(print_lock);
	kprintf("Lord FlowerKiller thread done\n");
	lock_release(print_lock);

	// track thread exit
	flowerkiller_exit++;
}

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	lock_acquire(print_lock);
	kprintf("Balloon thread starting\n");
	lock_release(print_lock);

	// exit thread when all ropes are severed
	while (true) {
		lock_acquire(count_lock);
		int count = ropes_left;
		lock_release(count_lock);
		if (count == 0) break;
		thread_yield();
	}

	lock_acquire(print_lock);
	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");
	lock_release(print_lock);

	// track thread exit
	balloon_exit++;
}

// Change this function as necessary
int
airballoon(int nargs, char **args)
{

	int err = 0;

	(void)nargs;
	(void)args;
	(void)ropes_left;

	// reset exit counters
	ropes_left = NROPES;
	dandelion_exit = 0;
	marigold_exit = 0;
	balloon_exit = 0;
	flowerkiller_exit = 0;

	// initialize print and counter lock
	print_lock = lock_create("Print lock");
	count_lock = lock_create("Count lock");

	// initialize ropes
	for (int i = 0; i < NROPES; i++) {
		ropes[i] = kmalloc(sizeof(struct rope));
		ropes[i]->stake = i;
		ropes[i]->hook = i;
		ropes[i]->connected = true;
		ropes[i]->rope_lock = lock_create("Rope lock");
	}

	// start Marigold thread
	err = thread_fork("Marigold Thread", NULL, marigold, NULL, 0);
	if (err) goto panic;

	// start Dandelion thread
	err = thread_fork("Dandelion Thread", NULL, dandelion, NULL, 0);
	if (err) goto panic;

	// start Lord FlowerKiller threads
	for (int i = 0; i < N_LORD_FLOWERKILLER; i++) {
		err = thread_fork("Lord FlowerKiller Thread", NULL, flowerkiller, NULL, 0);
		if (err) goto panic;
	}

	// start Balloon thread
	err = thread_fork("Air Balloon", NULL, balloon, NULL, 0);
	if (err) goto panic;

	while (dandelion_exit < 1 || marigold_exit < 1 || balloon_exit < 1 || flowerkiller_exit < N_LORD_FLOWERKILLER) {
		thread_yield();
	}

	// cleanup print and count locks
	lock_destroy(print_lock);
	lock_destroy(count_lock);

	// cleanup rope locks
	for (int i = 0; i < NROPES; i++) {
		lock_destroy(ropes[i]->rope_lock);
		kfree(ropes[i]);
	}

	// exit main thread
	kprintf("Main thread done\n");
	goto done;

panic:
	panic("airballoon: thread_fork failed: %s)\n", strerror(err));

done:
	return 0;
}
