#include "audio/RtGuard.h"

#if IHUB_RT_GUARD && IHUB_RT_GUARD_WRAP_LOCKS && defined(__linux__) &&                             \
    (defined(__GNUC__) || defined(__clang__))
#define IHUB_RT_HAS_LOCK_WRAP 1
#include <pthread.h>
#else
#define IHUB_RT_HAS_LOCK_WRAP 0
#endif

#if IHUB_RT_HAS_LOCK_WRAP

extern "C"
{
    int __real_pthread_mutex_lock (pthread_mutex_t*);
    int __wrap_pthread_mutex_lock (pthread_mutex_t* mutex)
    {
        AudioEngine::RtGuard::noteLock ("pthread_mutex_lock");
        return __real_pthread_mutex_lock (mutex);
    }

    int __real_pthread_mutex_trylock (pthread_mutex_t*);
    int __wrap_pthread_mutex_trylock (pthread_mutex_t* mutex)
    {
        AudioEngine::RtGuard::noteLock ("pthread_mutex_trylock");
        return __real_pthread_mutex_trylock (mutex);
    }

    int __real_pthread_mutex_timedlock (pthread_mutex_t*, const struct timespec*);
    int __wrap_pthread_mutex_timedlock (pthread_mutex_t* mutex, const struct timespec* timeout)
    {
        AudioEngine::RtGuard::noteLock ("pthread_mutex_timedlock");
        return __real_pthread_mutex_timedlock (mutex, timeout);
    }

    int __real_pthread_spin_lock (pthread_spinlock_t*);
    int __wrap_pthread_spin_lock (pthread_spinlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_spin_lock");
        return __real_pthread_spin_lock (lock);
    }

    int __real_pthread_spin_trylock (pthread_spinlock_t*);
    int __wrap_pthread_spin_trylock (pthread_spinlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_spin_trylock");
        return __real_pthread_spin_trylock (lock);
    }

    int __real_pthread_rwlock_rdlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_rdlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_rdlock");
        return __real_pthread_rwlock_rdlock (lock);
    }

    int __real_pthread_rwlock_wrlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_wrlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_wrlock");
        return __real_pthread_rwlock_wrlock (lock);
    }

    int __real_pthread_rwlock_tryrdlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_tryrdlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_tryrdlock");
        return __real_pthread_rwlock_tryrdlock (lock);
    }

    int __real_pthread_rwlock_trywrlock (pthread_rwlock_t*);
    int __wrap_pthread_rwlock_trywrlock (pthread_rwlock_t* lock)
    {
        AudioEngine::RtGuard::noteLock ("pthread_rwlock_trywrlock");
        return __real_pthread_rwlock_trywrlock (lock);
    }
}

#endif
