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

class InputEventSink : public RE::BSTEventSink<RE::InputEvent*>
{
public:
    virtual RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
    {
        if (EntryParser::getSingleton()->keys.empty())
            return RE::BSEventNotifyControl::kContinue;

        if (!a_event || !a_eventSource)
            return RE::BSEventNotifyControl::kContinue;

        auto target_ref = RE::CrosshairPickData::GetSingleton()->targetActor;
        if (!target_ref || !target_ref.get())
            return RE::BSEventNotifyControl::kContinue;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player)
            return RE::BSEventNotifyControl::kContinue;

        auto target = target_ref.get()->As<RE::Actor>();
        if (!target)
            return RE::BSEventNotifyControl::kContinue;

        if (!areActorsReady(player, target))
            return RE::BSEventNotifyControl::kContinue;

        for (auto event = *a_event; event; event = event->next)
        {
            if (event->eventType == RE::INPUT_EVENT_TYPE::kButton)
            {
                const auto button = static_cast<RE::ButtonEvent*>(event);
                if (!button || !button->IsDown())
                    continue;

                auto key = button->GetIDCode();
                switch (button->device.get())
                {
                    case RE::INPUT_DEVICE::kMouse:
                        key += kMouseOffset;
                        break;
                    case RE::INPUT_DEVICE::kKeyboard:
                        key += kKeyboardOffset;
                        break;
                    case RE::INPUT_DEVICE::kGamepad:
                        key += kGamepadOffset;
                        break;
                    default:
                        continue;
                }

                for (const auto& key_entry : EntryParser::getSingleton()->keys)
                {
                    if (key_entry.key == key)
                    {
                        RE::PlaySound(key_entry.trigger_sound.c_str());

                        // Check key req
                        auto target_health  = target->GetActorValue(RE::ActorValue::kHealth);
                        auto player_stamina = player->GetActorValue(RE::ActorValue::kStamina);
                        if ((std::max(0, player->GetLevel() - target->GetLevel()) < key_entry.level_diff) ||
                            (key_entry.enemy_hp > 1 ? target_health > key_entry.enemy_hp : target_health / target->GetBaseActorValue(RE::ActorValue::kHealth) > key_entry.enemy_hp) ||
                            (key_entry.stamina_cost > 1 ? player_stamina < key_entry.stamina_cost : player_stamina / player->GetBaseActorValue(RE::ActorValue::kStamina) < key_entry.stamina_cost))
                            continue;

                        decltype(EntryParser::entries) entries_copy;
                        if (key_entry.anims.empty())
                            entries_copy = EntryParser::getSingleton()->entries;
                        else
                            for (const auto& name : key_entry.anims)
                                if (EntryParser::getSingleton()->entries.contains(name))
                                    entries_copy[name] = EntryParser::getSingleton()->entries[name];

                        filterEntries(entries_copy, player, target, !isDetectedBy(player, target), target->IsBleedingOut() || isInRagdoll(target));
                        if (entries_copy.empty())
                            continue;

                        // Choose random anim and play
                        std::uniform_int_distribution<size_t> distro(0, entries_copy.size() - 1);
                        auto                                  entry = std::next(entries_copy.begin(), distro(random_engine))->second;

                        if (isInRagdoll(target))
                        {
                            target->SetPosition(RE::NiPoint3(target->data.location.x, target->data.location.y, player->GetPositionZ()), true);
                            target->SetActorValue(RE::ActorValue::kParalysis, 0);
                            target->boolBits.reset(RE::Actor::BOOL_BITS::kParalyzed);
                            target->NotifyAnimationGraph("GetUpStart");

                            DelayedFuncModule::getSingleton()->addFunc(0.2f, [=]() {
                                if (!(isValid(player) && isValid(target))) return;
                                target->actorState1.knockState = RE::KNOCK_STATE_ENUM::kNormal;
                                target->NotifyAnimationGraph("GetUpExit");
                                entry.play(player, target);
                            });
                        }
                        else
                            entry.play(player, target);

                        return RE::BSEventNotifyControl::kContinue;
                    }
                }
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    static void RegisterSink()
    {
        static InputEventSink _sink;
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&_sink);
    }
};
} // namespace phkm