#pragma once

#define START_THREAD(x) pthread_t x##_thread; pthread_create(&x##_thread, NULL, x, NULL)
#define WAIT_THREAD(x) pthread_join(x##_thread, NULL)

#define MINUTE 60
