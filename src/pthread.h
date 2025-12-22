/*
 * pthread.h stub for BarePD bare metal build
 * Single-threaded environment - all mutex/thread ops are no-ops
 * 
 * Newlib provides the types, we just need the function stubs
 */
#ifndef _PTHREAD_H
#define _PTHREAD_H

#include <stddef.h>
#include <sys/types.h>
#include <sys/time.h>

/* Use newlib's pthread types from sys/_pthreadtypes.h */
/* which is included via sys/types.h */

#define PTHREAD_MUTEX_INITIALIZER 0
#define PTHREAD_COND_INITIALIZER 0

/* Error codes */
#ifndef ETIMEDOUT
#define ETIMEDOUT 110
#endif

/* Mutex operations - no-ops for single threaded */
int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* Condition variable operations */
int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
                           const struct timespec *abstime);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

/* Thread operations */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
int pthread_join(pthread_t thread, void **retval);
pthread_t pthread_self(void);
int pthread_equal(pthread_t t1, pthread_t t2);

/* Thread-local storage */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void *value);

#endif /* _PTHREAD_H */
