#include "tracer.hpp"

#include <iostream>
#include <unistd.h>

void c()
{
    TRACER_FLOW_START ("c++", "c()", (void*) 0x1234);
    usleep (10000);
    TRACER_FLOW_STEP ("c++", "c()", (void*) 0x1234, "step 1");
    usleep (10000);
    TRACER_FLOW_STEP ("c++", "c()", (void*) 0x1234, "step 2");
    usleep (10000);
    TRACER_FLOW_STEP ("c++", "c()", (void*) 0x1234, "step 3");
    usleep (10000);
    TRACER_FLOW_FINISH ("c++", "c()", (void*) 0x1234);
}

void b()
{
    TRACER_ASYNC_START ("c++", "b()", (void*) 0x5678);
    usleep (10000);
    TRACER_ASYNC_STEP ("c++", "b()", (void*) 0x5678, "step 1");
    usleep (10000);
    TRACER_ASYNC_STEP ("c++", "b()", (void*) 0x5678, "step 2");
    usleep (20000);
    c();
    usleep (10000);
    TRACER_ASYNC_FINISH ("c++", "b()", (void*) 0x5678);
}

void a()
{
    TRACER_SCOPE ("c++", "a()");
    usleep (20000);
    b();
    usleep (10000);
}

int main()
{
    int i;
    TRACER.init ("trace.json");

    TRACER_META_PROCESS_NAME ("minitrace_test");
    TRACER_META_THREAD_NAME ("main thread");

    int long_running_thing_1;
    int long_running_thing_2;

    TRACER_ASYNC_START ("background", "long_running", &long_running_thing_1);
    TRACER_ASYNC_START ("background", "long_running", &long_running_thing_2);

    TRACER_COUNTER ("main", "greebles", 3);
    TRACER_BEGIN ("main", "outer");
    usleep (80000);
    for (i = 0; i < 3; i++)
    {
        TRACER_BEGIN ("main", "inner");
        usleep (40000);
        TRACER_END ("main", "inner");
        usleep (10000);
        TRACER_COUNTER ("main", "greebles", 3 * i + 10);
    }
    TRACER_ASYNC_STEP ("background", "long_running", &long_running_thing_1, "middle step");
    usleep (80000);
    TRACER_END ("main", "outer");
    TRACER_COUNTER ("main", "greebles", 0);

    usleep (10000);
    a();

    usleep (50000);
    TRACER_INSTANT ("main", "the end");
    usleep (10000);
    TRACER_ASYNC_FINISH ("background", "long_running", &long_running_thing_1);
    TRACER_ASYNC_FINISH ("background", "long_running", &long_running_thing_2);

    return 0;
}
