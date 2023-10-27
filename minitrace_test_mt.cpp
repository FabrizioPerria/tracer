#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "tracer.hpp"

// Does some meaningless work.
int work(int cycles)
{
	int a = cycles;
	for (int i = 0; i < cycles; i++)
	{
		a ^= 373;
		a = (a << 13) | (a >> (32 - 13));
	}
	return a;
}

void *worker_thread(void *param)
{
	int id = ( int ) ( intptr_t ) param;
	char temp[256];
	sprintf(temp, "Worker Thread %i", id);
	TRACER_META_THREAD_NAME(temp);
	int x = 0;
	for (int i = 0; i < 32; i++)
	{
		TRACER_SCOPE(__FILE__, "Worker");
		/* TRACER_BEGIN_INT(__FILE__, "Worker", "ID", id); */
		x += work((rand() & 0x7fff) * 1000);
		/* TRACER_END(__FILE__, "Worker"); */
	}
	return ( void * ) ( intptr_t ) x;
}

void phase2()
{
	for (int i = 0; i < 10000; i++)
	{
		TRACER_BEGIN(__FILE__, __FUNCTION__);
		TRACER_END(__FILE__, __FUNCTION__);
	}
}

int main()
{
	int i;
	TRACER.init("mt_trace.json");
	TRACER_META_PROCESS_NAME("Multithreaded Test");
	TRACER_META_THREAD_NAME("Main Thread");
	TRACER_BEGIN(__FILE__, __FUNCTION__);
#define NUMT 8
	pthread_t threads[NUMT];
	for (i = 0; i < NUMT; i++)
	{
		pthread_create(&threads[i], 0, &worker_thread, ( void * ) ( intptr_t ) i);
	}
	for (i = 0; i < NUMT; i++)
	{
		pthread_join(threads[i], 0);
	}
	phase2();

	TRACER_END(__FILE__, __FUNCTION__);
	return 0;
}
