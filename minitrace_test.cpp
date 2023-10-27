#include "tracer.hpp"

#include <iostream>
#include <unistd.h>

void c()
{
	TRACER_SCOPE("c++", "c()");
	usleep(10000);
}

void b()
{
	TRACER_SCOPE("c++", "b()");
	usleep(20000);
	c();
	usleep(10000);
}

void a()
{
	TRACER_SCOPE("c++", "a()");
	usleep(20000);
	b();
	usleep(10000);
}

int main()
{
	int i;
	TRACER.init("trace.json");

	TRACER_META_PROCESS_NAME("minitrace_test");
	TRACER_META_THREAD_NAME("main thread");

	int long_running_thing_1;
	int long_running_thing_2;

	TRACER_START("background", "long_running", &long_running_thing_1);
	TRACER_START("background", "long_running", &long_running_thing_2);

	TRACER_COUNTER("main", "greebles", 3);
	TRACER_BEGIN("main", "outer");
	usleep(80000);
	for (i = 0; i < 3; i++)
	{
		TRACER_BEGIN("main", "inner");
		usleep(40000);
		TRACER_END("main", "inner");
		usleep(10000);
		TRACER_COUNTER("main", "greebles", 3 * i + 10);
	}
	TRACER_STEP("background", "long_running", &long_running_thing_1, "middle step");
	usleep(80000);
	TRACER_END("main", "outer");
	TRACER_COUNTER("main", "greebles", 0);

	usleep(10000);
	a();

	usleep(50000);
	TRACER_INSTANT("main", "the end");
	usleep(10000);
	TRACER_FINISH("background", "long_running", &long_running_thing_1);
	TRACER_FINISH("background", "long_running", &long_running_thing_2);

	return 0;
}
