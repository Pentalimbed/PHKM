#pragma once

namespace phkm
{
class AnimEventSink : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
{
public:
    virtual RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource)
    {
        if (!a_event || !a_eventSource)
        {
            logger::debug("Event Source Not Found!");
            return RE::BSEventNotifyControl::kContinue;
        }

        logger::debug("Just logging BSAnimationGraphEvent!");
        logger::debug("tag: {}, holder: {}, payload: {}",
                      a_event->tag, a_event->holder->GetName(), a_event->payload);
        return RE::BSEventNotifyControl::kContinue;
    }

    static void RegisterSink(RE::Actor* actor)
    {
        static AnimEventSink _sink;

        actor->AddAnimationGraphEventSink(&_sink);
    }

private:
    AnimEventSink() = default;

    ~AnimEventSink() = default;

    AnimEventSink(const AnimEventSink&) = delete;

    AnimEventSink(AnimEventSink&&) = delete;

    AnimEventSink& operator=(const AnimEventSink&) = delete;

    AnimEventSink& operator=(AnimEventSink&&) = delete;
};
} // namespace phkm