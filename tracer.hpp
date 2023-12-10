#pragma once

#include <_types/_uint64_t.h>
#include <any>
#include <atomic>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>

#define INTERNAL_TRACER_BUFFER_SIZE 1000000

#define TRACER TracerManager::getInstance()

#define TRACER_BEGIN(c, n) TRACER.durationBegin(c, n)
#define TRACER_END(c, n) TRACER.durationEnd(c, n)
#define TRACER_SCOPE(c, n) ScopedTrace ____scope(c, n)

#define TRACER_BEGIN_INT(c, n, k, v) TRACER.durationBegin<int>(c, n, k, v)
#define TRACER_END_INT(c, n, k, v) TRACER.durationEnd<int>(c, n, k, v)
#define TRACER_SCOPE_INT(c, n, k, v) ScopedTrace<int> ____scope(c, n, k, v)

#define TRACER_BEGIN_STR(c, n, k, v) TRACER.durationBegin<std::string>(c, n, k, v)
#define TRACER_END_STR(c, n, k, v) TRACER.durationEnd<std::string>(c, n, k, v)
#define TRACER_SCOPE_STR(c, n, k, v) ScopedTrace<std::string> ____scope(c, n, k, v)

#define TRACER_ASYNC_START(c, n, id) TRACER.asyncBegin(c, n, id)
#define TRACER_ASYNC_STEP(c, n, id, step) TRACER.asyncStep(c, n, id, step)
#define TRACER_ASYNC_FINISH(c, n, id) TRACER.asyncEnd(c, n, id)

#define TRACER_ASYNC_START_INT(c, n, id, k, v) TRACER.asyncBegin<int>(c, n, id, k, v)
#define TRACER_ASYNC_FINISH_INT(c, n, id, k, v) TRACER.asyncEnd<int>(c, n, id, k, v)

#define TRACER_ASYNC_START_STR(c, n, id, k, v) TRACER.asyncBegin<std::string>(c, n, id, k, v)
#define TRACER_ASYNC_FINISH_STR(c, n, id, k, v) TRACER.asyncEnd<std::string>(c, n, id, k, v)

#define TRACER_FLOW_START(c, n, id) TRACER.flowStart(c, n, id)
#define TRACER_FLOW_STEP(c, n, id, step) TRACER.flowStep(c, n, id, step)
#define TRACER_FLOW_FINISH(c, n, id) TRACER.flowFinish(c, n, id)

#define TRACER_FLOW_START_INT(c, n, id, k, v) TRACER.flowStart<int>(c, n, id, k, v)
#define TRACER_FLOW_FINISH_INT(c, n, id, k, v) TRACER.flowFinish<int>(c, n, id, k, v)

#define TRACER_FLOW_START_STR(c, n, id, k, v) TRACER.flowStart<std::string>(c, n, id, k, v)
#define TRACER_FLOW_FINISH_STR(c, n, id, k, v) TRACER.flowFinish<std::string>(c, n, id, k, v)

#define TRACER_INSTANT(c, n) TRACER.instant(c, n)

#define TRACER_COUNTER(c, n, v) TRACER.counter(c, n, v)

#define TRACER_META_PROCESS_NAME(n) TRACER.metadata("process_name", n)
#define TRACER_META_THREAD_NAME(n) TRACER.metadata("thread_name", n)

template<typename T = void *> struct Event
{
	const std::string name;
	const std::string category;
	void *id;
	int64_t ts;
	uint32_t pid;
	unsigned long tid;
	char phase;
	const std::string argName;
	std::variant<int, std::string, void *> argValue;
};

class TracerManager
{
  public:
	static TracerManager &getInstance()
	{
		std::call_once(createInstance, []() { instance.reset(new TracerManager()); });
		return *instance;
	}

	void init(const std::string &jsonFile)
	{
		trace = std::ofstream(jsonFile);
		trace << "{\"traceEvents\":[" << std::endl;
		isTracing = true;
	}

	template<typename T = void *>
	inline void durationBegin(const std::string &category, const std::string &name, const std::string &key = "",
							  T value = nullptr)
	{
		processEvent<T>(category, name, 'B', nullptr, key, value);
	}

	template<typename T = void *>
	inline void durationEnd(const std::string &category, const std::string &name, const std::string &key = "",
							T value = nullptr)
	{
		processEvent<T>(category, name, 'E', nullptr, key, value);
	}

	inline void instant(const std::string &category, const std::string &name)
	{
		processEvent<void *>(category, name, 'i');
	}

	inline void counter(const std::string &category, const std::string &name, int count)
	{
		processEvent<int>(category, name, 'C', nullptr, name, count);
	}

	template<typename T = void *>
	inline void asyncBegin(const std::string &category, const std::string &name, void *id, const std::string &key = "",
						   T value = nullptr)
	{
		processEvent<T>(category, name, 'b', id, key, value);
	}

	inline void asyncStep(const std::string &category, const std::string &name, void *id, const std::string &step)
	{
		processEvent<std::string>(category, name, 'n', id, "step", step);
	}

	template<typename T = void *>
	inline void asyncEnd(const std::string &category, const std::string &name, void *id, const std::string &key = "",
						 T value = nullptr)
	{
		processEvent<T>(category, name, 'e', id, key, value);
	}

	template<typename T = void *>
	inline void flowStart(const std::string &category, const std::string &name, void *id, const std::string &key = "",
						  T value = nullptr)
	{
		processEvent<T>(category, name, 's', id, key, value);
	}

	inline void flowStep(const std::string &category, const std::string &name, void *id, const std::string &step)
	{
		processEvent<std::string>(category, name, 't', id, "step", step);
	}

	template<typename T = void *>
	inline void flowFinish(const std::string &category, const std::string &name, void *id, const std::string &key = "",
						   T value = nullptr)
	{
		processEvent<T>(category, name, 'f', id, key, value);
	}

	inline void metadata(const std::string &metadataName, const std::string &argValue)
	{
		processEvent<std::string>("", metadataName, 'M', nullptr, "name", argValue);
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
			std::string traceBuffer = ",\n{";
			std::call_once(firstEntry, [&traceBuffer]() { traceBuffer.erase(0, 2); });
			traceBuffer += "\"cat\":\"" + raw.category + "\",";
			traceBuffer += "\"pid\":" + std::to_string(raw.pid) + ",";
			traceBuffer += "\"tid\":" + std::to_string(raw.tid) + ",";
			traceBuffer += "\"ts\":" + std::to_string(raw.ts - timeOffset) + ",";
			traceBuffer += "\"ph\":\"" + std::string(1, raw.phase) + "\",";
			traceBuffer += "\"name\":\"" + raw.name + "\",";
			traceBuffer += "\"id\":\"" + getIdStr(raw.id) + "\"";

			std::visit(
				[&traceBuffer, &raw](auto &&argValue) {
					using T = std::decay_t<decltype(argValue)>;
					if constexpr (std::is_same_v<T, int>)
					{
						traceBuffer += ",\"args\": {\"" + raw.argName + "\":" + std::to_string(argValue) + "}";
					}
					else if constexpr (std::is_same_v<T, std::string>)
					{
						traceBuffer += ",\"args\": {\"" + raw.argName + "\":\"" + argValue + "\"}";
					}
				},
				raw.argValue);

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
	TracerManager() : eventsInProgress(0), isTracing(false), timeOffset(getTime())
	{
		eventBuffer.reserve(INTERNAL_TRACER_BUFFER_SIZE);
		flushBuffer.reserve(INTERNAL_TRACER_BUFFER_SIZE);
	}

	static std::unique_ptr<TracerManager> instance;
	static std::once_flag createInstance;
	std::once_flag firstEntry;

	std::vector<Event<std::variant<int, std::string>>> eventBuffer;
	std::vector<Event<std::variant<int, std::string>>> flushBuffer;

	std::atomic<uint16_t> eventsInProgress;

	bool isTracing;

	std::ofstream trace;

	std::mutex flushMutex;
	std::mutex eventMutex;
	std::condition_variable flushCV;

	int64_t timeOffset;

	int64_t getTime()
	{
		return std::chrono::duration_cast<std::chrono::microseconds>(
				   std::chrono::high_resolution_clock::now().time_since_epoch())
			.count();
	}

	template<typename T = void *>
	void processEvent(const std::string &category, const std::string &name, char ph, void *id = nullptr,
					  const std::string &arg_name = "", T arg_value = nullptr)
	{
		std::atomic_fetch_add(&eventsInProgress, 1);
		Event<std::variant<int, std::string>> ev{name,	   category, id,	   getTime(), ( uint32_t ) getpid(),
												 gettid(), ph,		 arg_name, nullptr};

		if constexpr (std::is_same_v<T, int> || std::is_same_v<T, std::string>)
		{
			ev.argValue = arg_value;
		}

		std::lock_guard<std::mutex> lock(eventMutex);
		eventBuffer.push_back(ev);

		if (eventBuffer.size() >= INTERNAL_TRACER_BUFFER_SIZE)
			flush();

		std::atomic_fetch_sub(&eventsInProgress, 1);
	}

	unsigned long gettid()
	{
		return std::hash<std::thread::id>()(std::this_thread::get_id());
	}

	std::string getIdStr(void *id)
	{
		if (id)
		{
			std::stringstream ss;
			ss << "0x" << std::hex << reinterpret_cast<std::uintptr_t>(id);
			return ss.str();
		}
		else
			return "0x0";
	}
};

std::unique_ptr<TracerManager> TracerManager::instance;
std::once_flag TracerManager::createInstance;

template<typename T = void *> class ScopedTrace
{
  public:
	ScopedTrace(const std::string &category, const std::string &name, std::string key = "", T value = nullptr)
		: _category(category), _name(name), _key(key), _value(value)
	{
		TRACER.durationBegin(category, name, key, value);
	}

	~ScopedTrace()
	{
		TRACER.durationEnd(_category, _name, _key, _value);
	}

  private:
	std::string _category;
	std::string _name;
	std::string _key;
	T _value;
};
