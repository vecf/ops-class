/*
 * All the contents of this file are overwritten during automated
 * testing. Please consider this before changing anything in this file.
 */

#include <types.h>
#include <lib.h>
#include <clock.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/test161.h>
#include <spinlock.h>

#define NREADERS 100
struct semaphore *test_done;

struct semaphore *reader_done;
struct semaphore *reader_start;
struct semaphore *writer_done;
struct semaphore *writer_start;

struct rwlock *test_rwlock;

void reader_thread(void *, unsigned long);
void writer_thread(void *, unsigned long);
void time_waster(int);


/*
 * Use these stubs to test your reader-writer locks.
 */


int rwtest(int nargs, char **args) {
	(void)nargs;
	(void)args;

	/*	vecf:
	test create rwlock */

	struct rwlock *rwlk;
	rwlk = rwlock_create("testrwtlock");
	if (rwlk == NULL) {
		panic("rwt1 - rwlock create failed\n");
	}

	kprintf_n("rwt1 - test create rwlock\n");
	kprintf_n("rwlock->name: %s\n", rwlk->rwlock_name);
	rwlock_destroy(rwlk);

	success(TEST161_SUCCESS, SECRET, "rwt1");

	return 0;
}

void time_waster(int yields) {
	int i;
	for (i=0; i<yields; ++i) 
		thread_yield();
}

void reader_thread(void *data1, unsigned long num)
{
	(void) data1;
		
	P(reader_start);

	random_yielder(4);
	kprintf_n("+r:%lu\n", num);
	rwlock_acquire_read(test_rwlock);
	kprintf_n("\t^r:%lu\n", num);
	time_waster(1); //time for r/w
	kprintf_n("\t\t-r:%lu\n", num);
	rwlock_release_read(test_rwlock);

	V(reader_done);

}

void writer_thread(void *data1, unsigned long num)
{
	(void) data1;

	P(writer_start);

	random_yielder(4);
	kprintf_n("+w:%lu\n", num);
	rwlock_acquire_write(test_rwlock);
	kprintf_n("\t^w:%lu\n", num);
	time_waster(1); //time for thread to r/w
	kprintf_n("\t\t-w:%lu\n", num);
	rwlock_release_write(test_rwlock);

	V(writer_done);
}


int rwtest2(int nargs, char **args) {
	(void)nargs;
	(void)args;

	/*	vecf:
	Release multiple readers and writers 'simultaneously'.
	Check for sequential writers and batched readers in output.
	*/

	reader_done = sem_create("reader_done\n",0);
	reader_start = sem_create("reader_start\n",0);
	writer_done = sem_create("writer_done\n",0);
	writer_start = sem_create("writer_start\n",0);

	test_rwlock = rwlock_create("testrwlk\n");

	kprintf_n("\nnreaders: %d\n",NREADERS);
	int n,readers,writers,arrivals;
	readers = writers = 0;
	arrivals = 1500; 
	for (n=0; n<NREADERS; ++n) {
		/*since this loop does almost no work, 
		it really needs to yield A LOT in comparison with the threads 
		that are doing some printing */
		readers+=1;
		thread_fork("testthread", NULL, reader_thread, NULL, readers);
		V(reader_start);
		time_waster(arrivals); //wait times between thread arrivals
		if (n%4 == 0){
			writers+=1;
			thread_fork("testthread", NULL, writer_thread, NULL, writers);
			V(writer_start);
			time_waster(arrivals); //wait times between thread arrivals
		}
	}
	for (n=0; n<readers; ++n) { // count completed threads 
		P(reader_done);
	}
	for (n=0; n<writers; ++n) { // count completed threads 
		P(writer_done);
	}
	success(TEST161_SUCCESS, SECRET, "rwt2");

	return 0;
}

int rwtest3(int nargs, char **args) {
	(void)nargs;
	(void)args;

	/*	vecf:
	Release multiple readers 'simultaneously'.
	Check for:
	- no deadlocks
	- batched readers in output no more than num_cpus at a time.
	*/

	reader_done = sem_create("reader_done\n",0);
	reader_start = sem_create("reader_start\n",0);
	writer_done = sem_create("writer_done\n",0);
	writer_start = sem_create("writer_start\n",0);

	test_rwlock = rwlock_create("testrwlk\n");

	kprintf_n("\nnreaders: %d\n",NREADERS);
	int n,readers,writers,arrivals;
	readers = writers = 0;
	arrivals = 1500; 
	for (n=0; n<NREADERS; ++n) {
		readers+=1;
		thread_fork("testthread", NULL, reader_thread, NULL, readers);
		V(reader_start);
		time_waster(arrivals); //wait times between thread arrivals
	}
	for (n=0; n<readers; ++n) { // count completed threads 
		P(reader_done);
	}
	success(TEST161_SUCCESS, SECRET, "rwt3");

	return 0;
}

int rwtest4(int nargs, char **args) {
	(void)nargs;
	(void)args;

	/*	vecf:
	Release multiple writers 'simultaneously'.
	Check for
	-no deadlocks
	-sequential writers
	*/

	writer_done = sem_create("writer_done\n",0);
	writer_start = sem_create("writer_start\n",0);

	test_rwlock = rwlock_create("testrwlk\n");

	kprintf_n("\nnreaders: %d\n",NREADERS);
	int n,writers,arrivals;
	writers = 0;
	arrivals = 1500; 
	for (n=0; n<NREADERS; ++n) {
		writers+=1;
		thread_fork("testthread", NULL, writer_thread, NULL, writers);
		V(writer_start);
		time_waster(arrivals); //wait times between thread arrivals
	}
	for (n=0; n<writers; ++n) { // count completed threads 
		P(writer_done);
	}
	success(TEST161_SUCCESS, SECRET, "rwt4");

	return 0;
}

int rwtest5(int nargs, char **args) {
	(void)nargs;
	(void)args;

	/*	vecf:
	Release multiple readers and writers 'simultaneously'.
	There are more writers than readers.
	Check for 
	-sequential writers 
	-batched readers in output.
	-num readers <= numcpus.
	*/

	reader_done = sem_create("reader_done\n",0);
	reader_start = sem_create("reader_start\n",0);
	writer_done = sem_create("writer_done\n",0);
	writer_start = sem_create("writer_start\n",0);

	test_rwlock = rwlock_create("testrwlk\n");

	kprintf_n("\nnreaders: %d\n",NREADERS);
	int n,readers,writers,arrivals;
	readers = writers = 0;
	arrivals = 1000; 
	for (n=0; n<NREADERS; ++n) {
		writers+=1;
		thread_fork("testthread", NULL, writer_thread, NULL, writers);
		V(writer_start);
		time_waster(arrivals); //wait times between thread arrivals
		if (n%4 == 0){
			readers+=1;
			thread_fork("testthread", NULL, reader_thread, NULL, readers);
			V(reader_start);
			time_waster(arrivals); //wait times between thread arrivals
		}
	}
	for (n=0; n<readers; ++n) { // count completed threads 
		P(reader_done);
	}
	for (n=0; n<writers; ++n) { // count completed threads 
		P(writer_done);
	}
	success(TEST161_SUCCESS, SECRET, "rwt5");

	return 0;
}
