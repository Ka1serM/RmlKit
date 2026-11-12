#pragma once

#include <functional>
#include <mutex>
#include <stop_token>
#include <vector>

#include <SDL3/SDL_events.h>
#include <tbb/global_control.h>
#include <tbb/task_arena.h>
#include <tbb/task_group.h>

#include "RmlKit/Core/StdGaps.h"

namespace RmlKit::UI {

class Dispatcher {
public:
    using Action = std::move_only_function<void()>;
    using Job = std::move_only_function<void(std::stop_token) const>;

    Dispatcher();
    ~Dispatcher();
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    static void postUi(Action action);
    static void run(Job job);

    void stopJobs();
    bool dispatch(const SDL_Event& event);

private:
    static Dispatcher& getInstance();

    static inline Dispatcher* instance = nullptr;

    Uint32 eventType;
    std::mutex mutex;
    std::vector<Action> queued;
    std::stop_source stop;
    //! TBB gives a wasm32 worker 2 MB of stack, and Tint in NoorRHI needs the 4 MB that Emscripten gives a pthread.
    tbb::global_control stackSize{tbb::global_control::thread_stack_size, 4 * 1024 * 1024};
    tbb::task_arena arena;
    tbb::task_group jobs;
};

}
