/*
 *  posix_sem.cpp - POSIX.4 semaphores "emulation"
 *  Copyright (C) 1999 Orlando Bassotto
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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
 * OK, I had really big problems with SysV semaphores :/
 * I rewrote those one giving a look to the source of linuxthreads
 * with mutex. Seems to be working correctly now.
 */

#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "semaphore.h"

extern "C" {

int sem_init(sem_t* sem, int pshared, unsigned int value)
{
	if(sem==NULL||value>SEM_VALUE_MAX) {
		errno = EINVAL;
		return -1;
	}
	if(pshared) {	
		errno = ENOSYS;
		return -1;
	}
	pthread_mutex_init(&sem->sem_lock, NULL);
	sem->sem_value = value;
	sem->sem_waiting = 0;
	return 0;	
}


int sem_destroy(sem_t* sem)
{
	if(sem==NULL) {
		errno = EINVAL;
		return -1;
	}
	if(sem->sem_waiting) {
		errno = EBUSY;
		return -1;
	}
	pthread_mutex_destroy(&sem->sem_lock);
	sem->sem_waiting = 0;
	sem->sem_value = 0;
	return 0;
}

sem_t sem_open(const char* name, int oflag, ...)
{
	errno = ENOSYS;
	return *(sem_t*)NULL;
}

int sem_close(sem_t* sem)
{
	errno = ENOSYS;
	return -1;
}

int sem_unlink(const char* name)
{
	errno = ENOSYS;
	return -1;
}

int sem_wait(sem_t* sem)
{
	struct timespec req = { 1, 0 };

	if(sem==NULL) {
		errno = EINVAL;
		return -1;
	}
	pthread_mutex_lock(&sem->sem_lock);
	sem->sem_waiting++;
	if(sem->sem_value > 0) {
		--sem->sem_value;
		return 0;
	}
	while(!sem->sem_value) nanosleep(NULL, &req);
	pthread_mutex_unlock(&sem->sem_lock);
	return 0;	
}

int sem_trywait(sem_t* sem)
{
	errno = ENOSYS;
	return -1;
}

int sem_post(sem_t* sem)
{
	if(sem==NULL) {
		errno = EINVAL;
		return -1;
	}
	if(!sem->sem_waiting) {
		if(sem->sem_value >= SEM_VALUE_MAX) {
			errno = ERANGE;
			pthread_mutex_unlock(&sem->sem_lock);
			return -1;
		}
		++sem->sem_value;
		pthread_mutex_unlock(&sem->sem_lock);
	}
	else {
		sem->sem_waiting--;
		++sem->sem_value;
//		pthread_mutex_unlock(&sem->sem_lock);
	}
	return 0;
}

int sem_getvalue(sem_t* sem, int* sval)
{
	errno = ENOSYS;
	return -1;
}

}
