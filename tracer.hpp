#pragma once

#include <_types/_uint64_t.h>
#include <cassert>
#include <ctime>
#include <fstream>
#include <sstream>
#include <mach-o/dyld.h>
#include <sys/_types/_int64_t.h>
#include <unistd.h>
#include <thread>

#define INTERNAL_TRACER_BUFFER_SIZE 1000000

#define TRACER TracerManager::getInstance()

#define TRACER_BEGIN(c, n) TRACER.durationBegin(c, n)
#define TRACER_END(c, n) TRACER.durationEnd(c, n)
#define TRACER_SCOPE(c, n) MTRScopedTrace ____mtr_scope(c, n)
#define TRACER_BEGIN_INT(c, n, k, v) TRACER.durationBegin(c, n, ArgType::INT, k, ( void * ) v)
#define TRACER_END_INT(c, n, k, v) TRACER.durationEnd(c, n, ArgType::INT, k, ( void * ) v)
#define TRACER_SCOPE_INT(c, n, k, v) MTRScopedTrace ____mtr_scope(c, n, ArgType::INT, k, ( void * ) v)

#define TRACER_START(c, n, id) TRACER.asyncBegin(c, n, ( void * ) (id))
#define TRACER_STEP(c, n, id, step) TRACER.asyncStep(c, n, ( void * ) (id), ( void * ) (step))
#define TRACER_FINISH(c, n, id) TRACER.asyncEnd(c, n, ( void * ) (id))

#define TRACER_FLOW_START(c, n, id) TRACER.flowBegin(c, n, ( void * ) (id))
#define TRACER_FLOW_STEP(c, n, id, step) TRACER.flowStep(c, n, ( void * ) (id), ( void * ) (step))
#define TRACER_FLOW_FINISH(c, n, id) TRACER.flowEnd(c, n, ( void * ) (id))

#define TRACER_INSTANT(c, n) TRACER.instant(c, n)

#define TRACER_COUNTER(c, n, v) TRACER.counter(c, n, v)

#define TRACER_META_PROCESS_NAME(n) TRACER.metadata("process_name", ( void * ) n)
#define TRACER_META_THREAD_NAME(n) TRACER.metadata("thread_name", ( void * ) n)

typedef enum
{
	NONE,
	INT,
	STRING,
} ArgType;

typedef struct event_t
{
	std::string name;
	std::string category;
	void *id;
	uint64_t ts;
	uint32_t pid;
	uint32_t tid;
	char phase;
	ArgType argType;
	std::string argName;
	std::string aStr;
	int aInt;
	double aDouble;
} Event;

class TracerManager
{
  public:
	static TracerManager &getInstance()
	{
		std::call_once(onlyOne, []() { instance.reset(new TracerManager()); });
		return *instance;
	}

	void init(const std::string jsonFile)
	{
		trace = std::ofstream(jsonFile);
		trace << "{\"traceEvents\":[" << std::endl;
		isTracing = TRUE;
		emptyTrace = TRUE;
	}

	void durationBegin(const std::string category, const std::string name, ArgType type = ArgType::NONE,
					   const std::string key = "", void *value = nullptr)
	{
		processEvent(category, name, 'B', 0, type, key, value);
	}

	void durationEnd(const std::string category, const std::string name, ArgType type = ArgType::NONE,
					 const std::string key = "", void *value = nullptr)
	{
		processEvent(category, name, 'E', 0, type, key, value);
	}

	void instant(const std::string category, const std::string name)
	{
		processEvent(category, name, 'i');
	}

	void counter(const std::string category, const std::string name, int64_t count)
	{
		processEvent(category, name, 'C', 0, ArgType::INT, name, ( void * ) count);
	}

	void asyncBegin(const std::string category, const std::string name, void *id)
	{
		processEvent(category, name, 'S', id);
	}

	void asyncStep(const std::string category, const std::string name, void *id, void *step)
	{
		processEvent(category, name, 'T', id, ArgType::STRING, "step", step);
	}

	void asyncEnd(const std::string category, const std::string name, void *id)
	{
		processEvent(category, name, 'F', id);
	}

	void flowStart(const std::string category, const std::string name, void *id)
	{
		processEvent(category, name, 's', id);
	}

	void flowStep(const std::string category, const std::string name, void *id, void *step)
	{
		processEvent(category, name, 't', id, ArgType::STRING, "step", step);
	}

	void flowEnd(const std::string category, const std::string name, void *id)
	{
		processEvent(category, name, 'f', id);
	}

	void metadata(const std::string metadataName, void *argValue)
	{
		processEvent("", metadataName, 'M', 0, ArgType::STRING, "name", argValue);
	}

	void flush(void)
	{
		{
			std::lock_guard<std::mutex> lock(flushMutex);

			if (isFlushing)
			{
				return;
			}
			isFlushing = TRUE;
			flushBuffer.swap(eventBuffer);
			int eventsInProgressCopy = 1;
			while (eventsInProgressCopy != 0)
			{
				std::lock_guard<std::mutex> lock(eventMutex);
				eventsInProgressCopy = eventsInProgress;
			}
		}

		for (auto raw : flushBuffer)
		{
			trace << (emptyTrace ? "" : ",") << std::endl;
			emptyTrace = FALSE;
			trace << "{\"cat\":\"" << raw.category << "\",";
			trace << "\"pid\":" << raw.pid << ",";
			trace << "\"tid\":" << raw.tid << ",";
			trace << "\"ts\":" << raw.ts - timeOffset << ",";
			trace << "\"ph\":\"" << raw.phase << "\",";
			trace << "\"name\":\"" << raw.name << "\",";
			trace << "\"args\":";

			switch (raw.argType)
			{
			case INT:
				trace << "{\"" << raw.argName << "\":" << raw.aInt << "}";
				break;
			case STRING:
				trace << "{\"" << raw.argName << "\":\"" << raw.aStr << "\"}";
				break;
			case NONE:
			default:
				trace << "{}";
				break;
			}

			if (raw.phase == 'X')
				trace << ",\"dur\":" << static_cast<double>(raw.aDouble);

			trace << ",\"id\":\"" << std::hex << std::setw(8) << std::setfill('0')
				  << static_cast<uint32_t>(reinterpret_cast<uintptr_t>(raw.id)) << "\"" << std::dec;

			trace << "}";
		}
		flushBuffer.clear();
		{
			std::lock_guard<std::mutex> lock(flushMutex);
			isFlushing = FALSE;
		}
	}

	~TracerManager()
	{
		if (trace.is_open())
		{
			flush();
			trace << std::endl << "]}" << std::endl;
			trace.close();
		}
	}

  private:
	TracerManager()
	{
		eventBuffer.reserve(INTERNAL_TRACER_BUFFER_SIZE);
		flushBuffer.reserve(INTERNAL_TRACER_BUFFER_SIZE);
		timeOffset = ( uint64_t ) (getTime() * 1000000);
		isTracing = FALSE;
		isFlushing = FALSE;
	}

	static std::unique_ptr<TracerManager> instance;
	static std::once_flag onlyOne;

	std::vector<Event> eventBuffer;
	std::vector<Event> flushBuffer;

	uint64_t eventsInProgress;

	bool isTracing;
	bool isFlushing;
	bool emptyTrace;

	std::ofstream trace;

	std::mutex flushMutex;
	std::mutex eventMutex;

	uint64_t timeOffset;

	double getTime()
	{
		static std::chrono::time_point<std::chrono::system_clock> start;
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

		if (start == std::chrono::time_point<std::chrono::system_clock>())
		{
			start = now;
		}

		std::chrono::duration<double> elapsed_seconds = now - start;
		return elapsed_seconds.count();
	}

	void processEvent(const std::string category, const std::string name, char ph, void *id = nullptr,
					  ArgType arg_type = NONE, const std::string arg_name = "", void *arg_value = nullptr)
	{
		std::lock_guard<std::mutex> lock(eventMutex);

		Event ev;
		ev.category = category;
		ev.name = name;
		ev.id = id;
		ev.phase = ph;
		ev.ts = ( uint64_t ) (getTime() * 1000000);
		ev.tid = gettid();
		ev.pid = getpid();
		ev.argType = arg_type;
		ev.argName = arg_name;

		switch (arg_type)
		{
		case INT:
			ev.aInt = (( uint64_t ) arg_value);
			break;
		case STRING:
			ev.aStr = std::string(( const char * ) arg_value);
			break;
		case NONE:
		default:
			break;
		}

		eventBuffer.push_back(ev);
	}

	std::uint16_t gettid()
	{
		return std::hash<std::thread::id>()(std::this_thread::get_id());
	}
};

std::unique_ptr<TracerManager> TracerManager::instance;
std::once_flag TracerManager::onlyOne;

class MTRScopedTrace
{
  public:
	MTRScopedTrace(const std::string category, const std::string name, std::string key = "", void *value = nullptr)
		: category(category), name(name), key(key), value(value)
	{
		if (key == "" && value == nullptr)
			TRACER.durationBegin(category, name);
	}

	~MTRScopedTrace()
	{
		if (key == "" && value == nullptr)
			TRACER.durationEnd(category, name);
	}

  private:
	std::string category;
	std::string name;
	std::string key;
	void *value;
};
