#include "RmlKit/UI/Backend/Dispatcher.h"

#include <format>
#include <utility>

#include "RmlKit/UI/Backend/Fatal.h"

namespace RmlKit::UI {

Dispatcher::Dispatcher() : eventType(SDL_RegisterEvents(1)) {
    if (instance)
        fatal("A dispatcher already exists");
    if (eventType == 0)
        fatal(std::format("SDL_RegisterEvents failed: {}", SDL_GetError()));
    instance = this;
}

Dispatcher::~Dispatcher() {
    stopJobs();
    instance = nullptr;
}

Dispatcher& Dispatcher::getInstance() {
    if (!instance)
        fatal("No dispatcher exists");
    return *instance;
}

void Dispatcher::postUi(Action action) {
    Dispatcher& dispatcher = getInstance();
    {
        const std::scoped_lock lock(dispatcher.mutex);
        const bool wakeQueued = !dispatcher.queued.empty();
        dispatcher.queued.push_back(std::move(action));
        if (wakeQueued)
            return;
    }
    //! Outside the lock: SDL runs event watchers inside SDL_PushEvent.
    SDL_Event wake{};
    wake.type = dispatcher.eventType;
    if (!SDL_PushEvent(&wake))
        fatal(std::format("SDL_PushEvent failed: {}", SDL_GetError()));
}

void Dispatcher::run(Job job) {
    Dispatcher& dispatcher = getInstance();
    if (dispatcher.stop.stop_requested())
        fatal("The dispatcher stopped its jobs");
    dispatcher.arena.enqueue([job = std::move(job), stop = dispatcher.stop.get_token()] { job(stop); }, dispatcher.jobs);
}

void Dispatcher::stopJobs() {
    stop.request_stop();
    arena.execute([this] { jobs.wait(); });
}

bool Dispatcher::dispatch(const SDL_Event& event) {
    if (event.type != eventType)
        return false;
    std::vector<Action> actions;
    {
        const std::scoped_lock lock(mutex);
        actions.swap(queued);
    }
    for (Action& action : actions)
        action();
    return true;
}

}
