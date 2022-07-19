/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>
#include <test.h>	/* usefull printing during tests */
#include <cpu.h> 	/* rwlock uses num_cpus */

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
	if (sem == NULL) {
		return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	KASSERT(curthread->t_in_interrupt == false);

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	HANGMAN_LOCKABLEINIT(&lock->lk_hangman, lock->lk_name);

	/*	vecf: 
	// ASST1 - add stuff here as needed
	-initialize spinlock 
	-initialize wchan
	-ensure lock not held and mem is initialized
	*/

	lock->lock_wchan = wchan_create(lock->lk_name);
	if (lock->lock_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}
	spinlock_init(&lock->lock_lock);

	lock->held_by = NULL;

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	/*	vecf: 
	// add stuff here as needed
	-lock must not be held, panic otherwise.
	-clean up spinlock 	(expects `struct spinlock`).
	-clean up wchan 	(expects `struct *wchan`).
				wchan_destroy asserts if anyone is waiting on wchan.
	*/

	KASSERT(lock != NULL);

	KASSERT(lock_do_i_hold(lock)==false); 

	spinlock_cleanup(&lock->lock_lock);
	wchan_destroy(lock->lock_wchan);

	//do not free lock->held_by. That kills the thread!

	kfree(lock->lk_name);
	kfree(lock);
}

void lock_release_have_spin(struct lock *lock); /* private helpers? */
void lock_acquire_have_spin(struct lock *lock);

void
lock_release_have_spin(struct lock *lock)
{
	/* helper for lock_release, re-used by others */
	lock->held_by = NULL;
	wchan_wakeone(lock->lock_wchan, &lock->lock_lock);
}

void
lock_acquire_have_spin(struct lock *lock)
{
	/* helper */
	KASSERT(lock_do_i_hold(lock)==false); /* avoid self deadlock */
	while (lock->held_by != NULL)
		wchan_sleep(lock->lock_wchan, &lock->lock_lock);
	lock->held_by = curthread;
}

void
lock_acquire(struct lock *lock)
{
	/*	vecf: 
	// ASST1 - Write this
	- get atomic access to lock
	- if already held, sleep until notified
	- if not held, take lock
	- release atomic access
	(basically imitate semaphore without counter)
	*/

	KASSERT(lock != NULL);
	KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&lock->lock_lock);

	/* Call this (atomically) before waiting for a lock */
	HANGMAN_WAIT(&curthread->t_hangman, &lock->lk_hangman);

	lock_acquire_have_spin(lock);

	/* Call this (atomically) once the lock is acquired */
	HANGMAN_ACQUIRE(&curthread->t_hangman, &lock->lk_hangman);

	spinlock_release(&lock->lock_lock);
}

void
lock_release(struct lock *lock)
{ 
	/*	vecf:
	//ASST1 - write this
	- get atomic access to lock
	- release the lock
	- only holding thread may release: panic otherwise.
	- notify one sleeping thread of realeased lock.
	- release atomic access to the lock
	*/

	KASSERT(lock != NULL);

	spinlock_acquire(&lock->lock_lock);

	KASSERT(lock_do_i_hold(lock) == true);

	lock_release_have_spin(lock);

	/* Call this (atomically) when the lock is released */
	HANGMAN_RELEASE(&curthread->t_hangman, &lock->lk_hangman);

	spinlock_release(&lock->lock_lock);
}

bool
lock_do_i_hold(struct lock *lock)
{
	/*	vecf:
	// ASST1 - Write this
	- Since curthread is never NULL, 
	- ... return true if curthread == lock->held_by.
	*/

	KASSERT(curthread != NULL);

	if (curthread == lock->held_by)
		return true;

	return false;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}

	/*	vecf:
	// add stuff here as needed
	*/

	cv->cv_wchan = wchan_create(cv->cv_name);
	if (cv->cv_wchan == NULL) {
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	/*	vecf:
	// ASST1 - add stuff here as needed
	*/

	wchan_destroy(cv->cv_wchan);
	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	/*	vecf:
	Release supplied lock, go to sleep, and, after 
	waking up again, re-acquire the lock.
	-current thread must hold lock
	*/

	KASSERT(lock_do_i_hold(lock) == true);

	spinlock_acquire(&lock->lock_lock);

	lock_release_have_spin(lock);

	wchan_sleep(cv->cv_wchan, &lock->lock_lock);

	lock_acquire_have_spin(lock);

	spinlock_release(&lock->lock_lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	/*	vecf:
	Wake up one thread that's sleeping on this CV.
	- current thread must hold lock
	*/
	KASSERT(lock_do_i_hold(lock)==true);

	spinlock_acquire(&lock->lock_lock);
	wchan_wakeone(cv->cv_wchan, &lock->lock_lock);
	spinlock_release(&lock->lock_lock);

}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	/*	vecf:
	Wake up all threads sleeping on this CV.
	- current thread must hold lock
	*/
	KASSERT(lock_do_i_hold(lock)==true);

	spinlock_acquire(&lock->lock_lock);
	wchan_wakeall(cv->cv_wchan, &lock->lock_lock);
	spinlock_release(&lock->lock_lock);
}

////////////////////////////////////////////////////////////
//
// RWLOCK


/*	vecf:
The main ideas behind the algorithm:
-Enforce exclusive access for writers, multiple access for readers. 
-If there are no writers, readers get immediate access.
-Readers stop getting immediate access when there are waiting writers or a write is in progress.
-Readers pool up when they cannot run straight away, later, the pool is released all at once.
-Threads can run in parallel on different processors, thus limiting pool
size to to num_cpus maximizes progress.
-For fairness, readers and writers take turns by:
	-Writers generally release pools of readers when done.
	-Reader pools generally release a single reader when the pool is done.
-When there are only readers or only writers, readers awaken readers and writers awaken writers.
*/

struct rwlock * rwlock_create(const char * name)
{
	struct rwlock *rwl;

	rwl = kmalloc(sizeof(*rwl));
	if (rwl == NULL) {
		return NULL;
	}

	rwl->rwlock_name = kstrdup(name);
	if (rwl->rwlock_name == NULL) {
		kfree(rwl);
		return NULL;
	}

	rwl->readers = wchan_create(rwl->rwlock_name);
	if (rwl->readers == NULL) {
		kfree(rwl->rwlock_name);
		kfree(rwl);
		return NULL;
	}
	 
	rwl->writers = wchan_create(rwl->rwlock_name);
	if (rwl->writers == NULL) {
		kfree(rwl->readers);
		kfree(rwl->rwlock_name);
		kfree(rwl);
		return NULL;
	}

	spinlock_init(&rwl->lock);

	// no one holding lock upon creation
	rwl->writes_wait = 0;
	rwl->writes_run = 0;
	rwl->reads_wait = 0;
	rwl->reads_run = 0;

	return rwl;
}

void rwlock_destroy(struct rwlock *rwl)
{
	KASSERT(rwl != NULL);

	KASSERT(rwl->writes_wait == 0);
	KASSERT(rwl->writes_run == 0);
	KASSERT(rwl->reads_wait == 0);
	KASSERT(rwl->reads_run == 0);

	spinlock_cleanup(&rwl->lock);

	wchan_destroy(rwl->readers);
	wchan_destroy(rwl->writers);

	kfree(rwl->rwlock_name);
	kfree(rwl);
}

void rwlock_acquire_read(struct rwlock *rwl)
{	
	KASSERT(rwl != NULL); 

	//no blocking in interrupt handlers
	KASSERT(curthread->t_in_interrupt == false);	

	spinlock_acquire(&rwl->lock);

	//sleep when 
	//1) writers are waiting... 
	if (rwl->writes_wait != 0)
		goto bed;

	//2) ...or too many readers running
	//3) ...or a write is in progress
	while (rwl->reads_run >= num_cpus ||
			rwl->writes_run != 0){
		//protected against spurious wakeups
		bed:
			rwl->reads_wait += 1;
			wchan_sleep(rwl->readers,&rwl->lock);
			rwl->reads_wait -= 1;
	}

	//readlock acquired on wakeup|immediate
	rwl->reads_run += 1;

	//no writers when reading
	KASSERT(rwl->writes_run == 0);

	//kprintf_n("\t\t\trunR:%d\n", rwl->reads_run);
	KASSERT(rwl->writes_run <= num_cpus);

	spinlock_release(&rwl->lock);
}

void rwlock_acquire_write(struct rwlock *rwl)
{	
	KASSERT(rwl != NULL); 

	//no blocking in interrupt handlers
	KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&rwl->lock);

	//sleep on read/write in progress
	while (rwl->reads_run != 0
			|| rwl->writes_run != 0) {
		//protected against spurious wakeups
		rwl->writes_wait += 1;
		wchan_sleep(rwl->writers, &rwl->lock);
		rwl->writes_wait -= 1;
	}
	//writelock acquired on wakeup|immediate
	rwl->writes_run += 1;
	
	//no readers when write acquired
	KASSERT(rwl->reads_run == 0);	
	//one writer at a time
	KASSERT(rwl->writes_run == 1);

	spinlock_release(&rwl->lock);
}

void rwlock_release_write(struct rwlock *rwl)
{	
	KASSERT(rwl != NULL); 

	spinlock_acquire(&rwl->lock);
	rwl->writes_run -= 1;

	//lock held, no writers running
	KASSERT(rwl->writes_run == 0);			

	if (rwl->reads_wait != 0) {
		//wake max number of readers
		for (unsigned i = 0; i < num_cpus; ++i)
			wchan_wakeone(rwl->readers, &rwl->lock);
	} else if (rwl->writes_wait != 0) {
		//...otherwise wake next writer
		wchan_wakeone(rwl->writers, &rwl->lock);
	}
	spinlock_release(&rwl->lock);
}

void rwlock_release_read(struct rwlock *rwl)
{
	KASSERT(rwl != NULL); 

	spinlock_acquire(&rwl->lock);

	rwl->reads_run -= 1;

	if (rwl->reads_run == 0 && rwl->writes_wait != 0) {
		//reader batch done, yield to writer
		wchan_wakeone(rwl->writers, &rwl->lock);
	} else if (rwl->writes_wait == 0) {
		//finishing reader signals another
		wchan_wakeone(rwl->readers, &rwl->lock);
	}

	spinlock_release(&rwl->lock);
}
