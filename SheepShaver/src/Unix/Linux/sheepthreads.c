/*
 *  sheepthreads.c - Minimal pthreads implementation (libpthreads doesn't
 *                   like nonstandard stacks)
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *  NOTES:
 *   - pthread_cancel() kills the thread immediately
 *   - Semaphores are VERY restricted: the only supported use is to have one
 *     thread sem_wait() on the semaphore while other threads sem_post() it
 *     (i.e. to use the semaphore as a signal)
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <pthread.h>
#include <semaphore.h>


/* Thread stack size */
#define STACK_SIZE 65536

/* From asm_linux.S */
extern int atomic_add(int *var, int add);
extern int atomic_and(int *var, int and);
extern int atomic_or(int *var, int or);
extern int test_and_set(int *var, int val);

/* Linux kernel calls */
extern int __clone(int (*fn)(void *), void *, int, void *);

/* struct sem_t */
#define status __status
#define spinlock __spinlock
#define sem_lock __sem_lock
#define sem_value __sem_value
#define sem_waiting __sem_waiting

/* Wait for "clone" children only (Linux 2.4+ specific) */
#ifndef __WCLONE
#define __WCLONE 0
#endif


/*
 *  Return pthread ID of self
 */

pthread_t pthread_self(void)
{
	return getpid();
}


/*
 *  Test whether two pthread IDs are equal
 */

int pthread_equal(pthread_t t1, pthread_t t2)
{
	return t1 == t2;
}


/*
 *  Send signal to thread
 */

int pthread_kill(pthread_t thread, int sig)
{
	if (kill(thread, sig) == -1)
		return errno;
	else
		return 0;
}


/*
 *  Create pthread
 */

struct new_thread {
	void *(*fn)(void *);
	void *arg;
};

static int start_thread(void *arg)
{
	struct new_thread *nt = (struct new_thread *)arg;
	nt->fn(nt->arg);
	return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
	struct new_thread *nt;
	void *stack;
	int pid;

	nt = (struct new_thread *)malloc(sizeof(struct new_thread));
	nt->fn = start_routine;
	nt->arg = arg;
	stack = malloc(STACK_SIZE);

	pid = __clone(start_thread, (char *)stack + STACK_SIZE - 16, CLONE_VM | CLONE_FS | CLONE_FILES, nt);
	if (pid == -1) {
		free(stack);
		free(nt);
		return errno;
	} else {
		*thread = pid;
		return 0;
	}
}


/*
 *  Join pthread
 */

int pthread_join(pthread_t thread, void **ret)
{
	do {
		if (waitpid(thread, NULL, __WCLONE) >= 0);
			break;
	} while (errno == EINTR);
	if (ret)
		*ret = NULL;
	return 0;
}


/*
 *  Cancel thread
 */

int pthread_cancel(pthread_t thread)
{
	kill(thread, SIGINT);
	return 0;
}


/*
 *  Test for cancellation
 */

void pthread_testcancel(void)
{
}


/*
 *  Spinlocks
 */

static int try_acquire_spinlock(int *lock)
{
	return test_and_set(lock, 1) == 0;
}

static void acquire_spinlock(volatile int *lock)
{
	do {
		while (*lock) ;
	} while (test_and_set((int *)lock, 1) != 0);
}

static void release_spinlock(int *lock)
{
	*lock = 0;
}


/*
 *  Initialize mutex
 */

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutex_attr)
{
	// pthread_init_lock
	mutex->__m_lock.__status = 0;
	mutex->__m_lock.__spinlock = 0;

	mutex->__m_kind = mutex_attr ? mutex_attr->__mutexkind : PTHREAD_MUTEX_TIMED_NP;
	mutex->__m_count = 0;
	mutex->__m_owner = NULL;
	return 0;
}


/*
 *  Destroy mutex
 */

int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	switch (mutex->__m_kind) {
	case PTHREAD_MUTEX_TIMED_NP:
		return (mutex->__m_lock.__status != 0) ? EBUSY : 0;
	default:
		return EINVAL;
	}
}


/*
 *  Lock mutex
 */

int pthread_mutex_lock(pthread_mutex_t *mutex)
{
	switch (mutex->__m_kind) {
	case PTHREAD_MUTEX_TIMED_NP:
		acquire_spinlock(&mutex->__m_lock.__spinlock);
		return 0;
	default:
		return EINVAL;
	}
}


/*
 *  Try to lock mutex
 */

int pthread_mutex_trylock(pthread_mutex_t *mutex)
{
	switch (mutex->__m_kind) {
	case PTHREAD_MUTEX_TIMED_NP:
		if (!try_acquire_spinlock(&mutex->__m_lock.__spinlock))
			return EBUSY;
		return 0;
	default:
		return EINVAL;
	}
}


/*
 *  Unlock mutex
 */

int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
	switch (mutex->__m_kind) {
	case PTHREAD_MUTEX_TIMED_NP:
		release_spinlock(&mutex->__m_lock.__spinlock);
		return 0;
	default:
		return EINVAL;
	}
}


/*
 *  Create mutex attribute
 */

int pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	attr->__mutexkind = PTHREAD_MUTEX_TIMED_NP;
	return 0;
}


/*
 *  Destroy mutex attribute
 */

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	return 0;
}


/*
 *  Init semaphore
 */

int sem_init(sem_t *sem, int pshared, unsigned int value)
{
	sem->sem_lock.status = 0;
	sem->sem_lock.spinlock = 0;
	sem->sem_value = value;
	sem->sem_waiting = NULL;
	return 0;
}


/*
 *  Delete remaphore
 */

int sem_destroy(sem_t *sem)
{
	return 0;
}


/*
 *  Wait on semaphore
 */

void null_handler(int sig)
{
}

int sem_wait(sem_t *sem)
{
	acquire_spinlock(&sem->sem_lock.spinlock);
	if (sem->sem_value > 0)
		atomic_add((int *)&sem->sem_value, -1);
	else {
		sigset_t mask;
		if (!sem->sem_lock.status) {
			struct sigaction sa;
			sem->sem_lock.status = SIGUSR2;
			sa.sa_handler = null_handler;
			sa.sa_flags = SA_RESTART;
			sigemptyset(&sa.sa_mask);
			sigaction(sem->sem_lock.status, &sa, NULL);
		}
		sem->sem_waiting = (struct _pthread_descr_struct *)getpid();
		sigemptyset(&mask);
		sigsuspend(&mask);
		sem->sem_waiting = NULL;
	}
	release_spinlock(&sem->sem_lock.spinlock);
	return 0;
}


/*
 *  Post semaphore
 */

int sem_post(sem_t *sem)
{
	acquire_spinlock(&sem->sem_lock.spinlock);
	if (sem->sem_waiting == NULL)
		atomic_add((int *)&sem->sem_value, 1);
	else
		kill((pid_t)sem->sem_waiting, sem->sem_lock.status);
	release_spinlock(&sem->sem_lock.spinlock);
	return 0;
}


/*
 *  Simple producer/consumer test program
 */

#ifdef TEST
#include <stdio.h>

static sem_t p_sem, c_sem;
static int data = 0;

static void *producer_func(void *arg)
{
	int i, n = (int)arg;
	for (i = 0; i < n; i++) {
		sem_wait(&p_sem);
		data++;
		sem_post(&c_sem);
	}
	return NULL;
}

static void *consumer_func(void *arg)
{
	int i, n = (int)arg;
	for (i = 0; i < n; i++) {
		sem_wait(&c_sem);
		printf("data: %d\n", data);
		sem_post(&p_sem);
	}
	sleep(1); // for testing pthread_join()
	return NULL;
}

int main(void)
{
	pthread_t producer_thread, consumer_thread;
	static const int N = 5;

	if (sem_init(&c_sem, 0, 0) < 0)
		return 1;
	if (sem_init(&p_sem, 0, 1) < 0)
		return 2;
	if (pthread_create(&producer_thread, NULL, producer_func, (void *)N) != 0)
		return 3;
	if (pthread_create(&consumer_thread, NULL, consumer_func, (void *)N) != 0)
		return 4;
	pthread_join(producer_thread, NULL);
	pthread_join(consumer_thread, NULL);
	sem_destroy(&p_sem);
	sem_destroy(&c_sem);
	if (data != N)
		return 5;
	return 0;
}
#endif
