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
	//TODO
	//lock->held_by //though you should not kill the thread!!!

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
	TODO 
	-how can different channels use same spinlock?
		-why could we not lock twice then wait?
	-why do cv_wait etc need to be atomic?

	NOTE:
	-from recitation 2017:
		every cv must have it's own wchan.
		but, it uses the lock passed in.
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

struct rwlock * rwlock_create(const char * name)
{
	struct rwlock * rwlock;

	rwlock = kmalloc(sizeof(*rwlock));
	if (rwlock == NULL) {
		return NULL;
	}

	rwlock->rwlock_name = kstrdup(name);
	if (rwlock->rwlock_name == NULL) {
		kfree(rwlock);
		return NULL;
	}

	rwlock->rwl_lock = lock_create(rwlock->rwlock_name);
	if (rwlock->rwl_lock == NULL) {
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	rwlock->cv_readers = cv_create(rwlock->rwlock_name);
	if (rwlock->cv_readers == NULL) {
		kfree(rwlock->rwl_lock);
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}
	 
	rwlock->cv_writers = cv_create(rwlock->rwlock_name);
	if (rwlock->cv_writers == NULL) {
		kfree(rwlock->cv_readers);
		kfree(rwlock->rwl_lock);
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}

	/* no one holding lock upon creation */
	rwlock->waiting_writers = 0;
	rwlock->running_writers = 0;
	rwlock->waiting_readers = 0;
	rwlock->running_readers = 0;

	return rwlock;
}

void rwlock_destroy(struct rwlock *rwlock)
{
	lock_acquire(rwlock->rwl_lock);

	//ensure no thread holding this rwlock
	KASSERT(rwlock->waiting_writers == 0);
	KASSERT(rwlock->running_writers == 0);
	KASSERT(rwlock->waiting_readers == 0);
	KASSERT(rwlock->running_readers == 0);

	cv_destroy(rwlock->cv_readers);
	cv_destroy(rwlock->cv_writers);
	lock_release(rwlock->rwl_lock); //cv's == NULL -> no one can get rwlock
	lock_destroy(rwlock->rwl_lock);

	kfree(rwlock->rwlock_name);
	kfree(rwlock);
}

/* TODO? A fair yet efficient way would be to place a limit on reader pool size :
this way, all readers can potentially access the resource in parallel without affecting the wait time
of queued writes. A fair limit in the case where there is only one process running on the machine,
would be NPROCESSORS. */

/* TODO COPY OUT DEFINES? */
#define rwl_lock	rwlock->rwl_lock
#define cv_readers	rwlock->cv_readers
#define cv_writers 	rwlock->cv_writers
#define waiting_writers rwlock->waiting_writers
#define running_writers rwlock->running_writers
#define waiting_readers rwlock->waiting_readers
#define running_readers rwlock->running_readers

/* 
//TODO check that these get checked 
KASSERT(rwlock != NULL); 
KASSERT(cv_readers != NULL);
KASSERT(cv_writers != NULL);
KASSERT(rwl_lock != NULL);
KASSERT(curthread->t_in_interrupt == false);
*/

void rwlock_acquire_read(struct rwlock *rwlock)
{	
	lock_acquire(rwl_lock);

	if (waiting_writers != 0 || 
			running_writers != 0){	//sleep on writers waiting or running
		waiting_readers += 1;
		cv_wait(cv_readers,rwl_lock);
		waiting_readers -= 1;
	}
	running_readers += 1;				//readlock acquired on wakeup|immediate
	lock_release(rwl_lock);
}

void rwlock_acquire_write(struct rwlock *rwlock)
{	
	lock_acquire(rwl_lock);
	if (running_readers != 0 
			|| running_writers != 0) {	//sleep on reading or writing
		waiting_writers += 1;
		cv_wait(cv_writers, rwl_lock);
		waiting_writers -= 1;
	}
	running_writers += 1;				//writelock acquired on wakeup|immediate
	KASSERT(running_writers == 1);			//one writer at a time
	lock_release(rwl_lock);
}

void rwlock_release_write(struct rwlock *rwlock)
{	
	lock_acquire(rwl_lock);
	running_writers -= 1;
	KASSERT(running_writers == 0);		//lock not released yet, no writers can be running
	if (waiting_readers != 0) {
		cv_broadcast(cv_readers, rwl_lock);
	} else if (waiting_writers != 0) {
		cv_signal(cv_writers, rwl_lock);
	}
	lock_release(rwl_lock);
}

void rwlock_release_read(struct rwlock *rwlock)
{
	lock_acquire(rwl_lock);
	running_readers -= 1;
	if (running_readers == 0) { // && waiting_writers != 0) {
		cv_signal(cv_writers, rwl_lock); //could have waiting readers at this point
	}
	lock_release(rwl_lock);
}
