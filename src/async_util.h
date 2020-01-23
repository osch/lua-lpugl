#include "base.h"

#ifndef LPUGL_ASYNC_UTIL_H
#define LPUGL_ASYNC_UTIL_H

/* -------------------------------------------------------------------------------------------- */

#define async_util_abort lpugl_async_util_abort
bool async_util_abort(int rc, int line); /* function does not return */

/* -------------------------------------------------------------------------------------------- */

#if defined(LPUGL_ASYNC_USE_WIN32)
typedef LONG  AtomicCounter;
typedef PVOID AtomicPtr;
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
typedef atomic_int      AtomicCounter;
typedef atomic_intptr_t AtomicPtr;
#elif defined(LPUGL_ASYNC_USE_GNU)
typedef int   AtomicCounter;
typedef void* AtomicPtr;
#endif

/* -------------------------------------------------------------------------------------------- */

static inline bool atomic_set_ptr_if_equal(AtomicPtr* ptr, void* oldPtr, void* newPtr)
{
#if defined(LPUGL_ASYNC_USE_WIN32)
    return oldPtr == InterlockedCompareExchangePointer(ptr, newPtr, oldPtr);
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
    intptr_t old = (intptr_t) oldPtr;
    return atomic_compare_exchange_strong(ptr, &old, (intptr_t)newPtr);
#elif defined(LPUGL_ASYNC_USE_GNU)
    return __sync_bool_compare_and_swap(ptr, oldPtr, newPtr);
#endif
}

static inline void* atomic_get_ptr(AtomicPtr* ptr)
{
#if defined(LPUGL_ASYNC_USE_WIN32)
    return InterlockedCompareExchangePointer(ptr, 0, 0);
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
    return (void*)atomic_load(ptr);
#elif defined(LPUGL_ASYNC_USE_GNU)
    return __sync_add_and_fetch(ptr, 0);
#endif
}

/* -------------------------------------------------------------------------------------------- */

static inline int atomic_inc(AtomicCounter* value)
{
#if defined(LPUGL_ASYNC_USE_WIN32)
    return InterlockedIncrement(value);
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
    return atomic_fetch_add(value, 1) + 1;
#elif defined(LPUGL_ASYNC_USE_GNU)
    return __sync_add_and_fetch(value, 1);
#endif
}

static inline int atomic_dec(AtomicCounter* value)
{
#if defined(LPUGL_ASYNC_USE_WIN32)
    return InterlockedDecrement(value);
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
    return atomic_fetch_sub(value, 1) - 1;
#elif defined(LPUGL_ASYNC_USE_GNU)
    return __sync_sub_and_fetch(value, 1);
#endif
}

static inline bool atomic_set_if_equal(AtomicCounter* value, int oldValue, int newValue)
{
#if defined(LPUGL_ASYNC_USE_WIN32)
    return oldValue == InterlockedCompareExchange(value, newValue, oldValue);
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
    return atomic_compare_exchange_strong(value, &oldValue, newValue);
#elif defined(LPUGL_ASYNC_USE_GNU)
    return __sync_bool_compare_and_swap(value, oldValue, newValue);
#endif
}

static inline int atomic_get(AtomicCounter* value)
{
#if defined(LPUGL_ASYNC_USE_WIN32)
    return InterlockedCompareExchange(value, 0, 0);
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
    return atomic_load(value);
#elif defined(LPUGL_ASYNC_USE_GNU)
    return __sync_add_and_fetch(value, 0);
#endif
}

static inline int atomic_set(AtomicCounter* value, int newValue)
{
#if defined(LPUGL_ASYNC_USE_WIN32)
    return InterlockedExchange(value, newValue);
#elif defined(LPUGL_ASYNC_USE_STDATOMIC)
    return atomic_exchange(value, newValue);
#elif defined(LPUGL_ASYNC_USE_GNU)
    int rslt = __sync_lock_test_and_set(value, newValue);
    __sync_synchronize();
    return rslt;
#endif
}

/* -------------------------------------------------------------------------------------------- */


typedef struct
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)
    pthread_mutexattr_t  attr;
    pthread_mutex_t      lock;

#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    CRITICAL_SECTION      lock;

#elif defined (LPUGL_ASYNC_USE_STDTHREAD)
    mtx_t                 lock;
#endif
} Lock;


/* -------------------------------------------------------------------------------------------- */

#define async_lock_init lpugl_async_lock_init
void async_lock_init(Lock* lock);

/* -------------------------------------------------------------------------------------------- */

#define async_lock_destruct lpugl_async_lock_destruct
void async_lock_destruct(Lock* lock);

/* -------------------------------------------------------------------------------------------- */

static inline void async_lock_acquire(Lock* lock) 
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)

    int rc = pthread_mutex_lock(&lock->lock);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    EnterCriticalSection(&lock->lock);

#elif defined(LPUGL_ASYNC_USE_STDTHREAD)
    int rc = mtx_lock(&lock->lock);
    if (rc != thrd_success)  { async_util_abort(rc, __LINE__); }
#endif
}

/* -------------------------------------------------------------------------------------------- */

static inline void async_lock_release(Lock* lock)
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)

    int rc = pthread_mutex_unlock(&lock->lock);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    LeaveCriticalSection(&lock->lock);

#elif defined(LPUGL_ASYNC_USE_STDTHREAD)
    int rc = mtx_unlock(&lock->lock);
    if (rc != thrd_success)  { async_util_abort(rc, __LINE__); }
#endif
}

/* -------------------------------------------------------------------------------------------- */

typedef struct
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)
    pthread_mutexattr_t   attr;
    pthread_mutex_t       mutex;
    pthread_cond_t        condition;

#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    CRITICAL_SECTION      mutex;
    volatile int          waitingCounter;
    HANDLE                event;

#elif defined(LPUGL_ASYNC_USE_STDTHREAD)
    mtx_t                 mutex;
    cnd_t                 condition;
#endif
} Mutex;


/* -------------------------------------------------------------------------------------------- */

#define async_mutex_init lpugl_async_mutex_init
void async_mutex_init(Mutex* mutex);

/* -------------------------------------------------------------------------------------------- */

#define async_mutex_destruct lpugl_async_mutex_destruct
void async_mutex_destruct(Mutex* mutex);

/* -------------------------------------------------------------------------------------------- */

static inline void async_mutex_lock(Mutex* mutex) 
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)

    int rc = pthread_mutex_lock(&mutex->mutex);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    EnterCriticalSection(&mutex->mutex);

#elif defined(LPUGL_ASYNC_USE_STDTHREAD)
    int rc = mtx_lock(&mutex->mutex);
    if (rc != thrd_success)  { async_util_abort(rc, __LINE__); }
#endif
}

/* -------------------------------------------------------------------------------------------- */

static inline bool async_mutex_trylock(Mutex* mutex) 
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)
    int rc = pthread_mutex_trylock(&mutex->mutex);
    if (rc == 0 || rc == EBUSY) {
        return (rc == 0);
    } else {
        return async_util_abort(rc, __LINE__);
    }
#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    return TryEnterCriticalSection(&mutex->mutex) != 0;

#elif defined(LPUGL_ASYNC_USE_STDTHREAD)
    int rc = mtx_trylock(&mutex->mutex);
    if (rc == thrd_success || rc == thrd_busy) {
        return rc == thrd_success;
    } else {
        return async_util_abort(rc, __LINE__); 
    }
#endif
}

/* -------------------------------------------------------------------------------------------- */

static inline void async_mutex_unlock(Mutex* mutex) 
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)

    int rc = pthread_mutex_unlock(&mutex->mutex);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    LeaveCriticalSection(&mutex->mutex);

#elif defined(LPUGL_ASYNC_USE_STDTHREAD)
    int rc = mtx_unlock(&mutex->mutex);
    if (rc != thrd_success)  { async_util_abort(rc, __LINE__); }
#endif
}

/* -------------------------------------------------------------------------------------------- */

#define async_mutex_wait lpugl_async_mutex_wait
void async_mutex_wait(Mutex* mutex);

/* -------------------------------------------------------------------------------------------- */

#define async_mutex_wait_millis lpugl_async_mutex_wait_millis
bool async_mutex_wait_millis(Mutex* mutex, int timeoutMillis);

/* -------------------------------------------------------------------------------------------- */

static inline void async_mutex_notify(Mutex* mutex) 
{
#if defined(LPUGL_ASYNC_USE_PTHREAD)

    int rc = pthread_cond_signal(&mutex->condition);
    if (rc != 0) { async_util_abort(rc, __LINE__); }

#elif defined(LPUGL_ASYNC_USE_WINTHREAD)
    if (mutex->waitingCounter > 0) {
        BOOL wasOk = SetEvent(mutex->event);
        if (!wasOk) {
            async_util_abort(GetLastError(), __LINE__);
        }
    }
#elif defined(LPUGL_ASYNC_USE_STDTHREAD)
    int rc = cnd_signal(&mutex->condition);
    if (rc != thrd_success)  { async_util_abort(rc, __LINE__); }
#endif
}

/* -------------------------------------------------------------------------------------------- */

#endif /* LPUGL_ASYNC_UTIL_H */

