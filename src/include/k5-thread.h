/*
 * include/k5-thread.h
 *
 * Copyright 2004 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * Preliminary thread support.
 */

#ifndef K5_THREAD_H
#define K5_THREAD_H

#include "autoconf.h"

/* Interface (tentative):

   Mutex support:

   // Between these two, we should be able to do pure compile-time
   // and pure run-time initialization.
   //   POSIX:   partial initializer is PTHREAD_MUTEX_INITIALIZER,
   //            finish does nothing
   //   Windows: partial initializer is an invalid handle,
   //            finish does the real initialization work
   //   debug:   partial initializer sets one magic value,
   //            finish verifies and sets a new magic value for
   //              lock/unlock to check
   k5_mutex_t foo_mutex = K5_MUTEX_PARTIAL_INITIALIZER;
   int k5_mutex_finish_init(k5_mutex_t *);
   // for dynamic allocation
   int k5_mutex_init(k5_mutex_t *);
   // Must work for both kinds of alloc, even if it means adding flags.
   int k5_mutex_destroy(k5_mutex_t *);

   // As before.
   int k5_mutex_lock(k5_mutex_t *);
   int k5_mutex_unlock(k5_mutex_t *);

   In each library, one new function to finish the static mutex init,
   and any other library-wide initialization that might be desired.
   On POSIX, this function would be called via the second support
   function (see below).  On Windows, it would be called at library
   load time.  These functions, or functions they calls, should be the
   only places that k5_mutex_finish_init gets called.

   A second function or macro called at various possible "first" entry
   points which either calls pthread_once on the first function
   (POSIX), or checks some flag set by the first function (Windows,
   debug support), and possibly returns an error.  (In the
   non-threaded case, a simple flag can be used to avoid multiple
   invocations, and the mutexes don't need run-time initialization
   anyways.)

   A third function for library termination calls mutex_destroy on
   each mutex for the library.  This function would be called
   automatically at library unload time.  If it turns out to be needed
   at exit time for libraries that don't get unloaded, perhaps we
   should also use atexit().  Any static mutexes should be cleaned up
   with k5_mutex_destroy here.

   How does that second support function invoke the first support
   function only once?  Through something modelled on pthread_once
   that I haven't written up yet.  Probably:

   k5_once_t foo_once = K5_ONCE_INIT;
   k5_once(k5_once_t *, void (*)(void));

   For POSIX: Map onto pthread_once facility.
   For non-threaded case: A simple flag.
   For Windows: Not needed; library init code takes care of it.

   XXX: A general k5_once mechanism isn't possible for Windows,
   without faking it through named mutexes or mutexes initialized at
   startup.  I was only using it in one place outside these headers,
   so I'm dropping the general scheme.  Eventually the existing uses
   in k5-thread.h and k5-platform.h will be converted to pthread_once
   or static variables.


   Thread-specific data:

   // TSD keys are limited in number in gssapi/krb5/com_err; enumerate
   // them all.  This allows support code init to allocate the
   // necessary storage for pointers all at once, and avoids any
   // possible error in key creation.
   enum { ... } k5_key_t;
   // Register destructor function.  Called in library init code.
   int k5_key_register(k5_key_t, void (*destructor)(void *));
   // Returns NULL or data.
   void *k5_getspecific(k5_key_t);
   // Returns error if key out of bounds, or the pointer table can't
   // be allocated.  A call to k5_key_register must have happened first.
   // This may trigger the calling of pthread_setspecific on POSIX.
   int k5_setspecific(k5_key_t, void *);
   // Called in library termination code.
   // Trashes data in all threads, calling the registered destructor
   // (but calling it from the current thread).
   int k5_key_delete(k5_key_t);

   For the non-threaded version, the support code will have a static
   array indexed by k5_key_t values, and get/setspecific simply access
   the array elements.

   The TSD destructor table is global state, protected by a mutex if
   threads are enabled.

   Debug support: Not much.  Might check if k5_key_register has been
   called and abort if not.


   Any actual external symbols will use the krb5int_ prefix.  The k5_
   names will be simple macros or inline functions to rename the
   external symbols, or slightly more complex ones to expand the
   implementation inline (e.g., map to POSIX versions and/or debug
   code using __FILE__ and the like).


   More to be added, perhaps.  */

#define DEBUG_THREADS
#define DEBUG_THREADS_LOC
#define DEBUG_THREADS_SLOW /* permit debugging stuff that'll slow things down */
#undef DEBUG_THREADS_STATS

#include <assert.h>

/* For tracking locations, of (e.g.) last lock or unlock of mutex.  */
#ifdef DEBUG_THREADS_LOC
typedef struct {
    const char *filename;
    short lineno;
} k5_debug_loc;
#define K5_DEBUG_LOC_INIT	{ __FILE__, __LINE__ }
#if __GNUC__ >= 2
#define K5_DEBUG_LOC		(__extension__ (k5_debug_loc)K5_DEBUG_LOC_INIT)
#else
static inline k5_debug_loc k5_debug_make_loc(const char *file, short line)
{
    k5_debug_loc l;
    l.filename = file;
    l.lineno = line;
    return l;
}
#define K5_DEBUG_LOC		(k5_debug_make_loc(__FILE__,__LINE__))
#endif
#else /* ! DEBUG_THREADS_LOC */
typedef char k5_debug_loc;
#define K5_DEBUG_LOC_INIT	0
#define K5_DEBUG_LOC		0
#endif

#define k5_debug_update_loc(L)	((L) = K5_DEBUG_LOC)



/* Statistics gathering:

   Currently incomplete, don't try enabling it.

   Eventually: Report number of times locked, total and standard
   deviation of the time the lock was held, total and std dev time
   spent waiting for the lock.  "Report" will probably mean "write a
   line to a file if a magic environment variable is set."  */

#ifdef DEBUG_THREADS_STATS

#if HAVE_TIME_H && (!defined(HAVE_SYS_TIME_H) || defined(TIME_WITH_SYS_TIME))
# include <time.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#include <inttypes.h>
typedef uint64_t k5_debug_timediff_t;
typedef struct timeval k5_debug_time_t;
static inline k5_debug_timediff_t
timediff(k5_debug_time_t t2, k5_debug_time_t t1)
{
    return (t2.tv_sec - t1.tv_sec) * 1000000 + (t2.tv_usec - t1.tv_usec);
}
struct k5_timediff_stats {
    k5_debug_timediff_t valmin, valmax, valsum, valsqsum;
};
typedef struct {
    int count;
    k5_debug_time_t time_acquired, time_created;
    struct k5_timediff_stats lockwait, lockheld;
} k5_debug_mutex_stats;
#define k5_mutex_init_stats(S) \
	(memset((S), 0, sizeof(struct k5_debug_mutex_stats)), 0)
#define k5_mutex_finish_init_stats(S) 	(0)
#define K5_MUTEX_STATS_INIT	{ 0, {0}, {0}, {0}, {0} }

#else

typedef char k5_debug_mutex_stats;
#define k5_mutex_init_stats(S)		(*(S) = 's', 0)
#define k5_mutex_finish_init_stats(S)	(0)
#define K5_MUTEX_STATS_INIT		's'

#endif



/* Define the OS mutex bit.  */

/* First, if we're not actually doing multiple threads, do we
   want the debug support or not?  */

#ifdef DEBUG_THREADS

enum k5_mutex_init_states {
    K5_MUTEX_DEBUG_PARTLY_INITIALIZED = 0x12,
    K5_MUTEX_DEBUG_INITIALIZED,
    K5_MUTEX_DEBUG_DESTROYED
};
enum k5_mutex_flag_states {
    K5_MUTEX_DEBUG_UNLOCKED = 0x23,
    K5_MUTEX_DEBUG_LOCKED
};

typedef struct {
    enum k5_mutex_init_states initialized;
    enum k5_mutex_flag_states locked;
} k5_os_nothread_mutex;

# define K5_OS_NOTHREAD_MUTEX_PARTIAL_INITIALIZER \
	{ K5_MUTEX_DEBUG_PARTLY_INITIALIZED, K5_MUTEX_DEBUG_UNLOCKED }

# define k5_os_nothread_mutex_finish_init(M)				\
	(assert((M)->initialized != K5_MUTEX_DEBUG_INITIALIZED),	\
	 assert((M)->initialized == K5_MUTEX_DEBUG_PARTLY_INITIALIZED),	\
	 assert((M)->locked == K5_MUTEX_DEBUG_UNLOCKED),		\
	 (M)->initialized = K5_MUTEX_DEBUG_INITIALIZED, 0)
# define k5_os_nothread_mutex_init(M)			\
	((M)->initialized = K5_MUTEX_DEBUG_INITIALIZED,	\
	 (M)->locked = K5_MUTEX_DEBUG_UNLOCKED, 0)
# define k5_os_nothread_mutex_destroy(M)				\
	(assert((M)->initialized == K5_MUTEX_DEBUG_INITIALIZED),	\
	 (M)->initialized = K5_MUTEX_DEBUG_DESTROYED, 0)

# define k5_os_nothread_mutex_lock(M)			\
	(k5_os_nothread_mutex_assert_unlocked(M),	\
	 (M)->locked = K5_MUTEX_DEBUG_LOCKED, 0)
# define k5_os_nothread_mutex_unlock(M)			\
	(k5_os_nothread_mutex_assert_locked(M),		\
	 (M)->locked = K5_MUTEX_DEBUG_UNLOCKED, 0)

# define k5_os_nothread_mutex_assert_locked(M)				\
	(assert((M)->initialized == K5_MUTEX_DEBUG_INITIALIZED),	\
	 assert((M)->locked != K5_MUTEX_DEBUG_UNLOCKED),		\
	 assert((M)->locked == K5_MUTEX_DEBUG_LOCKED), 0)
# define k5_os_nothread_mutex_assert_unlocked(M)			\
	(assert((M)->initialized == K5_MUTEX_DEBUG_INITIALIZED),	\
	 assert((M)->locked != K5_MUTEX_DEBUG_LOCKED),			\
	 assert((M)->locked == K5_MUTEX_DEBUG_UNLOCKED), 0)

#else /* threads disabled and not debugging */

typedef char k5_os_nothread_mutex;
# define K5_OS_NOTHREAD_MUTEX_PARTIAL_INITIALIZER	0
/* Empty inline functions avoid the "statement with no effect"
   warnings, and do better type-checking than functions that don't use
   their arguments.  */
static inline int k5_os_nothread_mutex_finish_init(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_init(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_destroy(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_lock(k5_os_nothread_mutex *m) {
    return 0;
}
static inline int k5_os_nothread_mutex_unlock(k5_os_nothread_mutex *m) {
    return 0;
}
# define k5_os_nothread_mutex_assert_locked(M)		(0)
# define k5_os_nothread_mutex_assert_unlocked(M)	(0)

#endif

/* Values:
   2 - function has not been run
   3 - function has been run
   4 - function is being run -- deadlock detected */
typedef unsigned char k5_os_nothread_once_t;
# define K5_OS_NOTHREAD_ONCE_INIT	2
# define k5_os_nothread_once(O,F)					\
	(*(O) == 3 ? 0							\
	 : *(O) == 2 ? (*(O) = 4, (F)(), *(O) = 3, 0)			\
	 : (assert(*(O) != 4), assert(*(O) == 2 || *(O) == 3), 0))



#ifndef ENABLE_THREADS

typedef k5_os_nothread_mutex k5_os_mutex;
# define K5_OS_MUTEX_PARTIAL_INITIALIZER	\
		K5_OS_NOTHREAD_MUTEX_PARTIAL_INITIALIZER
# define k5_os_mutex_finish_init	k5_os_nothread_mutex_finish_init
# define k5_os_mutex_init		k5_os_nothread_mutex_init
# define k5_os_mutex_destroy		k5_os_nothread_mutex_destroy
# define k5_os_mutex_lock		k5_os_nothread_mutex_lock
# define k5_os_mutex_unlock		k5_os_nothread_mutex_unlock
# define k5_os_mutex_assert_locked	k5_os_nothread_mutex_assert_locked
# define k5_os_mutex_assert_unlocked	k5_os_nothread_mutex_assert_unlocked

# define k5_once_t			k5_os_nothread_once_t
# define K5_ONCE_INIT			K5_OS_NOTHREAD_ONCE_INIT
# define k5_once			k5_os_nothread_once

#elif HAVE_PTHREAD

# include <pthread.h>

/* Weak reference support, etc.

   Linux: Stub mutex routines exist, but pthread_once does not.

   Solaris: In libc there's a pthread_once that doesn't seem
   to do anything.  Bleah.  But pthread_mutexattr_setrobust_np
   is defined only in libpthread.

   IRIX 6.5 stub pthread support in libc is really annoying.  The
   pthread_mutex_lock function returns ENOSYS for a program not linked
   against -lpthread.  No link-time failure, no weak symbols, etc.
   The C library doesn't provide pthread_once; we can use weak
   reference support for that.

   If weak references are not available, then for now, we assume that
   the pthread support routines will always be available -- either the
   real thing, or functional stubs that merely prohibit creating
   threads.

   If we find a platform with non-functional stubs and no weak
   references, we may have to resort to some hack like dlsym on the
   symbol tables of the current process.  */
#ifdef HAVE_PRAGMA_WEAK_REF
# pragma weak pthread_once
# pragma weak pthread_mutex_lock
# pragma weak pthread_mutex_unlock
# pragma weak pthread_mutex_destroy
# pragma weak pthread_mutex_init
# ifdef HAVE_PTHREAD_MUTEXATTR_SETROBUST_NP_IN_THREAD_LIB
#  pragma weak pthread_mutexattr_setrobust_np
# endif
# if !defined HAVE_PTHREAD_ONCE
#  define K5_PTHREADS_LOADED	(&pthread_once != 0)
# elif !defined HAVE_PTHREAD_MUTEXATTR_SETROBUST_NP \
	&& defined HAVE_PTHREAD_MUTEXATTR_SETROBUST_NP_IN_THREAD_LIB
#  define K5_PTHREADS_LOADED	(&pthread_mutexattr_setrobust_np != 0)
# else
#  define K5_PTHREADS_LOADED	(1)
# endif
#else
/* no pragma weak support */
# define K5_PTHREADS_LOADED	(1)
#endif

#if defined(__mips) && defined(__sgi) && (defined(_SYSTYPE_SVR4) || defined(__SYSTYPE_SVR4__))
/* IRIX 6.5 stub pthread support in libc is really annoying.  The
   pthread_mutex_lock function returns ENOSYS for a program not linked
   against -lpthread.  No link-time failure, no weak reference tests,
   etc.

   The C library doesn't provide pthread_once; we can use weak
   reference support for that.  */
# ifndef HAVE_PRAGMA_WEAK_REF
#  if defined(__GNUC__) && __GNUC__ < 3
#   error "Please update to a newer gcc with weak symbol support, or switch to native cc, reconfigure and recompile."
#  else
#   error "Weak reference support is required"
#  endif
# endif
# define USE_PTHREAD_LOCK_ONLY_IF_LOADED
#endif

#if !defined(HAVE_PTHREAD_MUTEX_LOCK) && !defined(USE_PTHREAD_LOCK_ONLY_IF_LOADED)
# define USE_PTHREAD_LOCK_ONLY_IF_LOADED
#endif

#ifdef HAVE_PRAGMA_WEAK_REF
/* Can't rely on useful stubs -- see above regarding Solaris.  */
typedef struct {
    pthread_once_t o;
    k5_os_nothread_once_t n;
} k5_once_t;
# define K5_ONCE_INIT	{ PTHREAD_ONCE_INIT, K5_OS_NOTHREAD_ONCE_INIT }
# define k5_once(O,F)	(K5_PTHREADS_LOADED			\
			 ? pthread_once(&(O)->o,F)		\
			 : k5_os_nothread_once(&(O)->n,F))
#else
typedef pthread_once_t k5_once_t;
# define K5_ONCE_INIT	PTHREAD_ONCE_INIT
# define k5_once	pthread_once
#endif

typedef struct {
    pthread_mutex_t p;
#ifdef USE_PTHREAD_LOCK_ONLY_IF_LOADED
    k5_os_nothread_mutex n;
#endif
} k5_os_mutex;

/* Define as functions to:
   (1) eliminate "statement with no effect" warnings for "0"
   (2) encourage type-checking in calling code  */

static inline void k5_pthread_assert_unlocked(pthread_mutex_t *m) { }
static inline void k5_pthread_assert_locked(pthread_mutex_t *m) { }

#if defined(DEBUG_THREADS_SLOW) && HAVE_SCHED_H && (HAVE_SCHED_YIELD || HAVE_PRAGMA_WEAK_REF)
# include <sched.h>
# if !HAVE_SCHED_YIELD
#  pragma weak sched_yield
#  define MAYBE_SCHED_YIELD()	((void)((&sched_yield != NULL) ? sched_yield() : 0))
# else
#  define MAYBE_SCHED_YIELD()	((void)sched_yield())
# endif
#else
# define MAYBE_SCHED_YIELD()	((void)0)
#endif

/* It may not be obvious why this function is desirable.

   I want to call pthread_mutex_lock, then sched_yield, then look at
   the return code from pthread_mutex_lock.  That can't be implemented
   in a macro without a temporary variable, or GNU C extensions.

   There used to be an inline function which did it, with both
   functions called from the inline function.  But that messes with
   the debug information on a lot of configurations, and you can't
   tell where the inline function was called from.  (Typically, gdb
   gives you the name of the function from which the inline function
   was called, and a line number within the inline function itself.)

   With this auxiliary function, pthread_mutex_lock can be called at
   the invoking site via a macro; once it returns, the inline function
   is called (with messed-up line-number info for gdb hopefully
   localized to just that call).  */
static inline int return_after_yield(int r)
{
    MAYBE_SCHED_YIELD();
    return r;
}

#ifdef USE_PTHREAD_LOCK_ONLY_IF_LOADED

# define K5_OS_MUTEX_PARTIAL_INITIALIZER \
	{ PTHREAD_MUTEX_INITIALIZER, K5_OS_NOTHREAD_MUTEX_PARTIAL_INITIALIZER }

# define k5_os_mutex_finish_init(M)		\
	k5_os_nothread_mutex_finish_init(&(M)->n)
# define k5_os_mutex_init(M)			\
	(k5_os_nothread_mutex_init(&(M)->n),	\
	 (K5_PTHREADS_LOADED			\
	  ? pthread_mutex_init(&(M)->p, 0)	\
	  : 0))
# define k5_os_mutex_destroy(M)			\
	(k5_os_nothread_mutex_destroy(&(M)->n),	\
	 (K5_PTHREADS_LOADED			\
	  ? pthread_mutex_destroy(&(M)->p)	\
	  : 0))

# define k5_os_mutex_lock(M)						\
	return_after_yield(K5_PTHREADS_LOADED				\
			   ? pthread_mutex_lock(&(M)->p)		\
			   : k5_os_nothread_mutex_lock(&(M)->n))
# define k5_os_mutex_unlock(M)				\
	(MAYBE_SCHED_YIELD(),				\
	 (K5_PTHREADS_LOADED				\
	  ? pthread_mutex_unlock(&(M)->p)		\
	  : k5_os_nothread_mutex_unlock(&(M)->n)))

# define k5_os_mutex_assert_unlocked(M)			\
	(K5_PTHREADS_LOADED				\
	 ? k5_pthread_assert_unlocked(&(M)->p)		\
	 : k5_os_nothread_mutex_assert_unlocked(&(M)->n))
# define k5_os_mutex_assert_locked(M)			\
	(K5_PTHREADS_LOADED				\
	 ? k5_pthread_assert_locked(&(M)->p)		\
	 : k5_os_nothread_mutex_assert_locked(&(M)->n))

#else

# define K5_OS_MUTEX_PARTIAL_INITIALIZER \
	{ PTHREAD_MUTEX_INITIALIZER }

static inline int k5_os_mutex_finish_init(k5_os_mutex *m) { return 0; }
# define k5_os_mutex_init(M)		pthread_mutex_init(&(M)->p, 0)
# define k5_os_mutex_destroy(M)		pthread_mutex_destroy(&(M)->p)
# define k5_os_mutex_lock(M)	return_after_yield(pthread_mutex_lock(&(M)->p))
# define k5_os_mutex_unlock(M)		(MAYBE_SCHED_YIELD(),pthread_mutex_unlock(&(M)->p))

# define k5_os_mutex_assert_unlocked(M)	k5_pthread_assert_unlocked(&(M)->p)
# define k5_os_mutex_assert_locked(M)	k5_pthread_assert_locked(&(M)->p)

#endif /* is pthreads always available? */

#elif defined _WIN32

typedef struct {
    HANDLE h;
    int is_locked;
} k5_os_mutex;

# define K5_OS_MUTEX_PARTIAL_INITIALIZER { INVALID_HANDLE_VALUE, 0 }

# define k5_os_mutex_finish_init(M)					 \
	(assert((M)->h == INVALID_HANDLE_VALUE),			 \
	 ((M)->h = CreateMutex(NULL, FALSE, NULL)) ? 0 : GetLastError())
# define k5_os_mutex_init(M)						 \
	((M)->is_locked = 0,						 \
	 ((M)->h = CreateMutex(NULL, FALSE, NULL)) ? 0 : GetLastError())
# define k5_os_mutex_destroy(M)		\
	(CloseHandle((M)->h) ? ((M)->h = 0, 0) : GetLastError())

static inline int k5_os_mutex_lock(k5_os_mutex *m)
{
    DWORD res;
    res = WaitForSingleObject(m->h, INFINITE);
    if (res == WAIT_FAILED)
	return GetLastError();
    /* Eventually these should be turned into some reasonable error
       code.  */
    assert(res != WAIT_TIMEOUT);
    assert(res != WAIT_ABANDONED);
    assert(res == WAIT_OBJECT_0);
    /* Avoid locking twice.  */
    assert(m->is_locked == 0);
    m->is_locked = 1;
    return 0;
}

# define k5_os_mutex_unlock(M)				\
	(assert((M)->is_locked == 1),			\
	 (M)->is_locked = 0,				\
	 ReleaseMutex((M)->h) ? 0 : GetLastError())

# define k5_os_mutex_assert_unlocked(M)	(0)
# define k5_os_mutex_assert_locked(M)	(0)

#else

# error "Thread support enabled, but thread system unknown"

#endif




typedef struct {
    k5_debug_loc loc_last, loc_created;
    k5_os_mutex os;
    k5_debug_mutex_stats stats;
} k5_mutex_t;
#define K5_MUTEX_PARTIAL_INITIALIZER		\
	{ K5_DEBUG_LOC_INIT, K5_DEBUG_LOC_INIT,	\
	  K5_OS_MUTEX_PARTIAL_INITIALIZER, K5_MUTEX_STATS_INIT }
static inline int k5_mutex_init_1(k5_mutex_t *m, k5_debug_loc l)
{
    int err = k5_os_mutex_init(&m->os);
    if (err) return err;
    m->loc_created = m->loc_last = l;
    err = k5_mutex_init_stats(&m->stats);
    assert(err == 0);
    return 0;
}
#define k5_mutex_init(M)	k5_mutex_init_1((M), K5_DEBUG_LOC)
static inline int k5_mutex_finish_init_1(k5_mutex_t *m, k5_debug_loc l)
{
    int err = k5_os_mutex_finish_init(&m->os);
    if (err) return err;
    m->loc_created = m->loc_last = l;
    err = k5_mutex_finish_init_stats(&m->stats);
    assert(err == 0);
    return 0;
}
#define k5_mutex_finish_init(M)	k5_mutex_finish_init_1((M), K5_DEBUG_LOC)
#define k5_mutex_destroy(M)			\
	(k5_os_mutex_assert_unlocked(&(M)->os),	\
	 (M)->loc_last = K5_DEBUG_LOC,		\
	 k5_os_mutex_destroy(&(M)->os))
static inline int k5_mutex_lock_1(k5_mutex_t *m, k5_debug_loc l)
{
    int err = 0;
    err = k5_os_mutex_lock(&m->os);
    if (err)
	return err;
    m->loc_last = l;
    return err;
}
#define k5_mutex_lock(M)	k5_mutex_lock_1(M, K5_DEBUG_LOC)
static inline int k5_mutex_unlock_1(k5_mutex_t *m, k5_debug_loc l)
{
    int err = 0;
    err = k5_os_mutex_unlock(&m->os);
    if (err)
	return err;
    m->loc_last = l;
    return err;
}
#define k5_mutex_unlock(M)	k5_mutex_unlock_1(M, K5_DEBUG_LOC)

#define k5_mutex_assert_locked(M)	k5_os_mutex_assert_locked(&(M)->os)
#define k5_mutex_assert_unlocked(M)	k5_os_mutex_assert_unlocked(&(M)->os)

#define k5_assert_locked	k5_mutex_assert_locked
#define k5_assert_unlocked	k5_mutex_assert_unlocked


/* Thread-specific data; implemented in a support file, because we'll
   need to keep track of some global data for cleanup purposes.

   Note that the callback function type is such that the C library
   routine free() is a valid callback.  */
typedef enum {
    K5_KEY_COM_ERR,
    K5_KEY_GSS_KRB5_SET_CCACHE_OLD_NAME,
    K5_KEY_GSS_KRB5_CCACHE_NAME,
    K5_KEY_MAX
} k5_key_t;
/* rename shorthand symbols for export */
#define k5_key_register	krb5int_key_register
#define k5_getspecific	krb5int_getspecific
#define k5_setspecific	krb5int_setspecific
#define k5_key_delete	krb5int_key_delete
extern int k5_key_register(k5_key_t, void (*)(void *));
extern void *k5_getspecific(k5_key_t);
extern int k5_setspecific(k5_key_t, void *);
extern int k5_key_delete(k5_key_t);

#endif /* multiple inclusion? */
