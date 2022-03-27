#pragma once
#include "phkm.h"

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

        logger::debug("Anim Event {}, holder: {}, payload: {}",
                      a_event->tag, a_event->holder->GetName(), a_event->payload);
        if (a_event->tag == "IdleForceDefaultState")
        {
            logger::debug("return to normal!");
            auto actor = const_cast<RE::Actor*>(a_event->holder->As<RE::Actor>());
            actor->NotifyAnimationGraph("WeapOutRightReplaceForceEquip");
            DelayedFuncModule::getSingleton()->addFunc(0.1, [=]() { actor->NotifyAnimationGraph("WeapEquip_Out"); });
            actor->RemoveAnimationGraphEventSink(this);
        }

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