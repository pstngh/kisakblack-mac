// sdl_mainthread.cpp — see sdl_mainthread.h.
#include "sdl_mainthread.h"

#if defined(__APPLE__)

#include <pthread.h>

namespace {
pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  g_cond  = PTHREAD_COND_INITIALIZER;
// One request at a time: posters serialise on g_mutex while their work is pending.
void (*g_fn)(void *);
void *g_arg;
bool g_pending;
} // namespace

void Sys_RunOnMainThread(void (*fn)(void *), void *arg) {
    if (pthread_main_np()) {
        fn(arg);
        return;
    }
    pthread_mutex_lock(&g_mutex);
    while (g_pending)
        pthread_cond_wait(&g_cond, &g_mutex);
    g_fn = fn;
    g_arg = arg;
    g_pending = true;
    while (g_pending)
        pthread_cond_wait(&g_cond, &g_mutex);
    pthread_cond_broadcast(&g_cond);  // wake the next poster, if any
    pthread_mutex_unlock(&g_mutex);
}

void Sys_ServiceMainThreadWork() {
    // Not from inside posted work (a wait in it would lock g_mutex again).
    static bool servicing;
    if (!pthread_main_np() || servicing)
        return;
    pthread_mutex_lock(&g_mutex);
    if (g_pending) {
        servicing = true;
        g_fn(g_arg);
        servicing = false;
        g_fn = nullptr;
        g_arg = nullptr;
        g_pending = false;
        pthread_cond_broadcast(&g_cond);
    }
    pthread_mutex_unlock(&g_mutex);
}

#else

void Sys_RunOnMainThread(void (*fn)(void *), void *arg) { fn(arg); }
void Sys_ServiceMainThreadWork() {}

#endif
