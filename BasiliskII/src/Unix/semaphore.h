#ifndef __SEMAPHORE_H
#define __SEMAPHORE_H

#define SEM_VALUE_MAX 64

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif /* c_plusplus || __cplusplus */

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

#if defined(c_plusplus) || defined(__cplusplus)
};
#endif /* c_plusplus || __cplusplus */

#endif /* __SEMAPHORE_H */
