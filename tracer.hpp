#pragma once

#include <_types/_uint64_t.h>
#include <atomic>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>

#define INTERNAL_TRACER_BUFFER_SIZE 1000000

#define TRACER TracerManager::getInstance()

#define TRACER_BEGIN(c, n) TRACER.add (c, n, Phases::Duration_Begin)
#define TRACER_END(c, n) TRACER.add (c, n, Phases::Duration_End)
#define TRACER_SCOPE(c, n) ScopedTrace ____scope (c, n)

#define TRACER_BEGIN_STR(c, n, k, v) TRACER.add (c, n, Phases::Duration_Begin, nullptr, k, v)
#define TRACER_END_STR(c, n, k, v) TRACER.add (c, n, Phases::Duration_End, nullptr, k, v)
#define TRACER_SCOPE_STR(c, n, k, v) ScopedTrace<std::string> ____scope (c, n, k, v)

#define TRACER_ASYNC_START(c, n, id) TRACER.add (c, n, Phases::Async_Begin, id)
#define TRACER_ASYNC_STEP(c, n, id, step) TRACER.add (c, n, Phases::Async_Step, id, "step", step)
#define TRACER_ASYNC_FINISH(c, n, id) TRACER.add (c, n, Phases::Async_End, id)

#define TRACER_ASYNC_START_STR(c, n, id, k, v) TRACER.add (c, n, Phases::Async_Begin, id, k, v)
#define TRACER_ASYNC_FINISH_STR(c, n, id, k, v) TRACER.add (c, n, Phases::Async_End, id, k, v)

#define TRACER_FLOW_START(c, n, id) TRACER.add (c, n, Phases::Flow_Start, id)
#define TRACER_FLOW_STEP(c, n, id, step) TRACER.add (c, n, Phases::Flow_Step, id, "step", step)
#define TRACER_FLOW_FINISH(c, n, id) TRACER.add (c, n, Phases::Flow_Finish, id)

#define TRACER_FLOW_START_STR(c, n, id, k, v) TRACER.add (c, n, Phases::Flow_Start, id, k, v)
#define TRACER_FLOW_FINISH_STR(c, n, id, k, v) TRACER.add (c, n, Phases::Flow_Finish, id, k, v)

#define TRACER_INSTANT(c, n) TRACER.add (c, n, Phases::Instant)

#define TRACER_COUNTER(c, n, v) TRACER.add (c, n, Phases::Counter, nullptr, n, std::to_string (v))

#define TRACER_META_PROCESS_NAME(n) TRACER.add ("process_name", n, Phases::Metadata)
#define TRACER_META_THREAD_NAME(n) TRACER.add ("thread_name", n, Phases::Metadata)

enum class Phases
{
    Duration_Begin,
    Duration_End,
    Instant,
    Counter,
    Async_Begin,
    Async_Step,
    Async_End,
    Flow_Start,
    Flow_Step,
    Flow_Finish,
    Metadata
};

struct Event
{
    std::string name;
    std::string category;
    void* id;
    int64_t ts;
    uint32_t pid;
    unsigned long tid;
    Phases phase;
    std::string argName;
    std::string argValue;
};

class FifoManager
{
public:
    FifoManager (int size) : bufferSize (size), validStart (0), validEnd (0)
    {
    }
    int getTotalSize() const noexcept
    {
        return bufferSize;
    }
    int getFreeSpace() const noexcept
    {
        const int used = getNumReady();
        return bufferSize - used;
    }
    int getNumReady() const noexcept
    {
        return validEnd - validStart;
    }

    int write (int num) noexcept
    {
        const int currentEnd = validEnd % bufferSize;
        const int openStart = (currentEnd + 1) % bufferSize;
        const int openEnd = (validStart > currentEnd) ? validStart.load() : bufferSize;

        const int numToWrite = std::min (num, openEnd - openStart);

        validEnd = (validEnd + numToWrite) % bufferSize;
        return currentEnd;
    }

    int read (int num) noexcept
    {
        const int currentStart = validStart % bufferSize;
        const int currentEnd = validEnd % bufferSize;

        const int numReady = (currentEnd - currentStart + bufferSize) % bufferSize;
        const int numToRead = std::min (num, numReady);

        validStart = (validStart + numToRead) % bufferSize;
        return currentStart;
    }

private:
    int bufferSize;
    std::atomic<int> validStart, validEnd;
};

class TracerManager
{
public:
    static TracerManager& getInstance()
    {
        std::call_once (createInstance, []() { instance.reset (new TracerManager()); });
        return *instance;
    }

    void init (const std::string& jsonFile)
    {
        trace = std::ofstream (jsonFile);
        trace << "{\"traceEvents\":[" << std::endl;
        isTracing = true;
    }

    void add (const std::string& category,
              const std::string& name,
              Phases ph,
              void* id = nullptr,
              const std::string& arg_name = "",
              const std::string& arg_value = "")
    {
        if (isTracing && fifoManager.getFreeSpace() > 0)
        {
            auto index = fifoManager.write (1);
            eventBuffer.at (index) = { name, category, id, getTime(), (uint32_t) getpid(), gettid(), ph, arg_name, arg_value };
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
    }

    static std::unique_ptr<TracerManager> instance;
    static std::once_flag createInstance;
    std::once_flag firstEntry;

    std::array<Event, INTERNAL_TRACER_BUFFER_SIZE> eventBuffer;
    FifoManager fifoManager { INTERNAL_TRACER_BUFFER_SIZE };

    bool isTracing { false };
    std::ofstream trace;
    int64_t timeOffset { getTime() };

    int64_t getTime()
    {
        return std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }

    unsigned long gettid()
    {
        return std::hash<std::thread::id>() (std::this_thread::get_id());
    }

    std::string getIdStr (void* id)
    {
        if (id)
        {
            std::stringstream ss;
            ss << "0x" << std::hex << reinterpret_cast<std::uintptr_t> (id);
            return ss.str();
        }
        else
            return "0x0";
    }

    void flush (void)
    {
        while (fifoManager.getNumReady() > 0)
        {
            auto index = fifoManager.read (1);
            auto ev = eventBuffer[index];

            std::string traceBuffer = ",\n{";
            std::call_once (firstEntry, [&traceBuffer]() { traceBuffer.erase (0, 2); });
            traceBuffer += "\"cat\":\"" + ev.category + "\",";
            traceBuffer += "\"pid\":" + std::to_string (ev.pid) + ",";
            traceBuffer += "\"tid\":" + std::to_string (ev.tid) + ",";
            traceBuffer += "\"ts\":" + std::to_string (ev.ts - timeOffset) + ",";
            traceBuffer += "\"ph\":\"" + std::string (1, phase.at (ev.phase)) + "\",";
            traceBuffer += "\"name\":\"" + ev.name + "\",";
            traceBuffer += "\"id\":\"" + getIdStr (ev.id) + "\"";
            traceBuffer += ",\"args\": {\"" + ev.argName + "\":\"" + ev.argValue + "\"}";
            traceBuffer += "}";
            trace << traceBuffer;
        }
    }

    const std::map<Phases, char> phase = { //
        { Phases::Duration_Begin, 'B' }, { Phases::Duration_End, 'E' }, { Phases::Instant, 'i' },   { Phases::Counter, 'C' },
        { Phases::Async_Begin, 'b' },    { Phases::Async_Step, 'n' },   { Phases::Async_End, 'e' }, { Phases::Flow_Start, 's' },
        { Phases::Flow_Step, 't' },      { Phases::Flow_Finish, 'f' },  { Phases::Metadata, 'M' }
    };
};

std::unique_ptr<TracerManager> TracerManager::instance;
std::once_flag TracerManager::createInstance;

class ScopedTrace
{
public:
    ScopedTrace (const std::string& category, const std::string& name, std::string key = "", std::string value = "")
        : _category (category), _name (name), _key (key), _value (value)
    {
        TRACER.add (_category, _name, Phases::Duration_Begin, nullptr, _key, _value);
    }

    ~ScopedTrace()
    {
        TRACER.add (_category, _name, Phases::Duration_End, nullptr, _key, _value);
    }

private:
    std::string _category;
    std::string _name;
    std::string _key;
    std::string _value;
};
