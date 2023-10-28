#pragma once

#include <_types/_uint64_t.h>
#include <atomic>
#include <ctime>
#include <fstream>
#include <sstream>
#include <thread>
#include <unistd.h>

#define INTERNAL_TRACER_BUFFER_SIZE 1000000

#define TRACER TracerManager::getInstance()

#define TRACER_BEGIN(c, n) TRACER.durationBegin(c, n)
#define TRACER_END(c, n) TRACER.durationEnd(c, n)
#define TRACER_SCOPE(c, n) ScopedTrace ____scope(c, n)
#define TRACER_BEGIN_INT(c, n, k, v) TRACER.durationBegin(c, n, ArgType::INT, k, ( void * ) v)
#define TRACER_END_INT(c, n, k, v) TRACER.durationEnd(c, n, ArgType::INT, k, ( void * ) v)
#define TRACER_SCOPE_INT(c, n, k, v) ScopedTrace ____scope(c, n, ArgType::INT, k, ( void * ) v)
#define TRACER_BEGIN_STR(c, n, k, v) TRACER.durationBegin(c, n, ArgType::STRING, k, ( void * ) v)
#define TRACER_END_STR(c, n, k, v) TRACER.durationEnd(c, n, ArgType::STRING, k, ( void * ) v)
#define TRACER_SCOPE_STR(c, n, k, v) ScopedTrace ____scope(c, n, ArgType::STRING, k, ( void * ) v)

#define TRACER_ASYNC_START(c, n, id) TRACER.asyncBegin(c, n, ( void * ) (id))
#define TRACER_ASYNC_STEP(c, n, id, step) TRACER.asyncStep(c, n, ( void * ) (id), ( void * ) (step))
#define TRACER_ASYNC_FINISH(c, n, id) TRACER.asyncEnd(c, n, ( void * ) (id))
#define TRACER_ASYNC_START_INT(c, n, id, k, v) TRACER.asyncBegin(c, n, ( void * ) (id), ArgType::INT, k, ( void * ) v)
#define TRACER_ASYNC_STEP_INT(c, n, id, step, k, v)                                                                    \
	TRACER.asyncStep(c, n, ( void * ) (id), ( void * ) (step), ArgType::INT, k, ( void * ) v)
#define TRACER_ASYNC_FINISH_INT(c, n, id, k, v) TRACER.asyncEnd(c, n, ( void * ) (id), ArgType::INT, k, ( void * ) v)
#define TRACER_ASYNC_START_STR(c, n, id, k, v)                                                                         \
	TRACER.asyncBegin(c, n, ( void * ) (id), ArgType::STRING, k, ( void * ) v)
#define TRACER_ASYNC_STEP_STR(c, n, id, step, k, v)                                                                    \
	TRACER.asyncStep(c, n, ( void * ) (id), ( void * ) (step), ArgType::STRING, k, ( void * ) v)
#define TRACER_ASYNC_FINISH_STR(c, n, id, k, v) TRACER.asyncEnd(c, n, ( void * ) (id), ArgType::STRING, k, ( void * ) v)

#define TRACER_FLOW_START(c, n, id) TRACER.flowStart(c, n, ( void * ) (id))
#define TRACER_FLOW_STEP(c, n, id, step) TRACER.flowStep(c, n, ( void * ) (id), ( void * ) (step))
#define TRACER_FLOW_FINISH(c, n, id) TRACER.flowFinish(c, n, ( void * ) (id))
#define TRACER_FLOW_START_INT(c, n, id, k, v) TRACER.flowStart(c, n, ( void * ) (id), ArgType::INT, k, ( void * ) v)
#define TRACER_FLOW_STEP_INT(c, n, id, step, k, v)                                                                     \
	TRACER.flowStep(c, n, ( void * ) (id), ( void * ) (step), ArgType::INT, k, ( void * ) v)
#define TRACER_FLOW_FINISH_INT(c, n, id, k, v) TRACER.flowFinish(c, n, ( void * ) (id), ArgType::INT, k, ( void * ) v)
#define TRACER_FLOW_START_STR(c, n, id, k, v) TRACER.flowStart(c, n, ( void * ) (id), ArgType::STRING, k, ( void * ) v)
#define TRACER_FLOW_STEP_STR(c, n, id, step, k, v)                                                                     \
	TRACER.flowStep(c, n, ( void * ) (id), ( void * ) (step), ArgType::STRING, k, ( void * ) v)
#define TRACER_FLOW_FINISH_STR(c, n, id, k, v)                                                                         \
	TRACER.flowFinish(c, n, ( void * ) (id), ArgType::STRING, k, ( void * ) v)

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

	void init(const std::string &jsonFile)
	{
		trace = std::ofstream(jsonFile);
		trace << "{\"traceEvents\":[" << std::endl;
		isTracing = true;
		emptyTrace = true;
	}

	inline void durationBegin(const std::string &category, const std::string &name, ArgType type = ArgType::NONE,
							  const std::string &key = "", void *value = nullptr)
	{
		processEvent(category, name, 'B', 0, type, key, value);
	}

	inline void durationEnd(const std::string &category, const std::string &name, ArgType type = ArgType::NONE,
							const std::string &key = "", void *value = nullptr)
	{
		processEvent(category, name, 'E', 0, type, key, value);
	}

	inline void instant(const std::string &category, const std::string &name)
	{
		processEvent(category, name, 'i');
	}

	inline void counter(const std::string &category, const std::string &name, int64_t count)
	{
		processEvent(category, name, 'C', 0, ArgType::INT, name, ( void * ) count);
	}

	inline void asyncBegin(const std::string &category, const std::string &name, void *id)
	{
		processEvent(category, name, 'S', id);
	}

	inline void asyncStep(const std::string &category, const std::string &name, void *id, void *step)
	{
		processEvent(category, name, 'T', id, ArgType::STRING, "step", step);
	}

	inline void asyncEnd(const std::string &category, const std::string &name, void *id)
	{
		processEvent(category, name, 'F', id);
	}

	inline void flowStart(const std::string &category, const std::string &name, void *id)
	{
		processEvent(category, name, 's', id);
	}

	inline void flowStep(const std::string &category, const std::string &name, void *id, void *step)
	{
		processEvent(category, name, 't', id, ArgType::STRING, "step", step);
	}

	inline void flowFinish(const std::string &category, const std::string &name, void *id)
	{
		processEvent(category, name, 'f', id);
	}

	inline void metadata(const std::string &metadataName, void *argValue)
	{
		processEvent("", metadataName, 'M', 0, ArgType::STRING, "name", argValue);
	}

	void flush(void)
	{
		{
			std::unique_lock<std::mutex> lock(flushMutex);
			flushCV.wait(lock, [this] { return eventsInProgress == 0; });

			flushBuffer.clear();
			flushBuffer.reserve(eventBuffer.size());
			std::swap(flushBuffer, eventBuffer);
			eventBuffer.clear();
		}

		for (const auto &raw : flushBuffer)
		{
			std::string traceBuffer;
			traceBuffer += (emptyTrace ? "\n" : ",\n");
			emptyTrace = false;
			traceBuffer += "{\"cat\":\"" + raw.category + "\",";
			traceBuffer += "\"pid\":" + std::to_string(raw.pid) + ",";
			traceBuffer += "\"tid\":" + std::to_string(raw.tid) + ",";
			traceBuffer += "\"ts\":" + std::to_string(raw.ts - timeOffset) + ",";
			traceBuffer += "\"ph\":\"" + std::string(1, raw.phase) + "\",";
			traceBuffer += "\"name\":\"" + raw.name + "\",";
			traceBuffer += "\"args\":";

			switch (raw.argType)
			{
			case INT:
				traceBuffer += "{\"" + raw.argName + "\":" + std::to_string(raw.aInt) + "}";
				break;
			case STRING:
				traceBuffer += "{\"" + raw.argName + "\":\"" + raw.aStr + "\"}";
				break;
			case NONE:
			default:
				traceBuffer += "{}";
				break;
			}

			if (raw.phase == 'X')
				traceBuffer += ",\"dur\":" + std::to_string(raw.aDouble);

			if (raw.id)
			{
				std::stringstream ss;
				ss << ",\"id\":\"" << std::hex << std::setw(8) << std::setfill('0')
				   << static_cast<uint32_t>(reinterpret_cast<uintptr_t>(raw.id)) << "\"" << std::dec;
				traceBuffer += ss.str();
			}

			traceBuffer += "}";
			trace << traceBuffer;
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
	TracerManager() : eventsInProgress(0), isTracing(false), emptyTrace(true), timeOffset(getTime())
	{
		eventBuffer.reserve(INTERNAL_TRACER_BUFFER_SIZE);
		flushBuffer.reserve(INTERNAL_TRACER_BUFFER_SIZE);
	}

	static std::unique_ptr<TracerManager> instance;
	static std::once_flag onlyOne;

	std::vector<Event> eventBuffer;
	std::vector<Event> flushBuffer;

	std::atomic<uint16_t> eventsInProgress;

	bool isTracing;
	bool emptyTrace;

	std::ofstream trace;

	std::mutex flushMutex;
	std::mutex eventMutex;
	std::condition_variable flushCV;

	uint64_t timeOffset;

	double getTime()
	{
		// struct timespec ts;
		// clock_gettime(CLOCK_MONOTONIC, &ts);
		// return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
		return std::chrono::duration_cast<std::chrono::microseconds>(
				   std::chrono::high_resolution_clock::now().time_since_epoch())
			.count();
	}

	void processEvent(const std::string &category, const std::string &name, char ph, void *id = nullptr,
					  ArgType arg_type = NONE, const std::string &arg_name = "", void *arg_value = nullptr)
	{
		std::atomic_fetch_add(&eventsInProgress, 1);
		Event ev;
		ev.category = category;
		ev.name = name;
		ev.id = id;
		ev.phase = ph;
		ev.ts = getTime();
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

		std::lock_guard<std::mutex> lock(eventMutex);
		eventBuffer.push_back(ev);

		if (eventBuffer.size() >= INTERNAL_TRACER_BUFFER_SIZE)
			flush();

		std::atomic_fetch_sub(&eventsInProgress, 1);
	}

	std::uint16_t gettid()
	{
		return std::hash<std::thread::id>()(std::this_thread::get_id());
	}
};

std::unique_ptr<TracerManager> TracerManager::instance;
std::once_flag TracerManager::onlyOne;

class ScopedTrace
{
  public:
	ScopedTrace(const std::string &category, const std::string &name, ArgType type=ArgType::NONE, std::string key = "", void *value = nullptr)
		: category(category), name(name), type(type), key(key), value(value)
	{
		TRACER.durationBegin(category, name, type, key, value);
	}

	~ScopedTrace()
	{
		TRACER.durationEnd(category, name, type, key, value);
	}

  private:
	std::string category;
	std::string name;
	ArgType type;
	std::string key;
	void *value;
};
