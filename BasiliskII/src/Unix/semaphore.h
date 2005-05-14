#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H

#define SEM_VALUE_MAX 64

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif /* c_plusplus || __cplusplus */

/* MacOS X doesn't implement unnamed POSIX semaphores, event though
   the libc defines them! */
#if (defined(__MACH__) && defined(__APPLE__))
#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/semaphore.h>

#define sem_t						semaphore_t
#define sem_init(SEM,UNUSED,VALUE)	semaphore_create(current_task(), (SEM), SYNC_POLICY_FIFO, (VALUE))
#define sem_destroy(SEM)			semaphore_destroy(current_task(), *(SEM))
#define sem_wait(SEM)				semaphore_wait(*(SEM))
#define sem_post(SEM)				semaphore_signal(*(SEM))
#else
typedef struct psem {
	pthread_mutex_t sem_lock;
	int sem_value;
	int sem_waiting;
} sem_t;

int sem_init(sem_t* sem, int pshared, unsigned int value);
int sem_destroy(sem_t* sem);
sem_t sem_open(const char* name, int oflag, ...);
int sem_close(sem_t* sem);
int sem_unlink(const char* name);
int sem_wait(sem_t* sem);
int sem_trywait(sem_t* sem);
int sem_post(sem_t* sem);
int sem_getvalue(sem_t* sem, int* sval);
#endif

#if defined(c_plusplus) || defined(__cplusplus)
};
#endif /* c_plusplus || __cplusplus */

#endif /* __SEMAPHORE_H */
