#include "utils.h"

namespace phkm
{
bool orCheck(nlohmann::json j_array, check_func_t func)
{
    for (const auto& item : j_array)
        if (item.is_array() ? andCheck(item, func) : func(item))
            return true;
    return j_array.empty();
}

bool andCheck(nlohmann::json j_array, check_func_t func)
{
    for (const auto& item : j_array)
        if (!(item.is_array() ? andCheck(item, func) : func(item)))
            return false;
    return j_array.empty();
}

bool isDetectedBy(RE::Actor* subject, RE::Actor* target)
{
    static bool                 is_initialized = false;
    static RE::TESConditionItem isDetectedCond;
    if (!is_initialized)
    {
        is_initialized = true;

        isDetectedCond.data.functionData.function  = RE::FUNCTION_DATA::FunctionID::kGetDetected;
        isDetectedCond.data.functionData.params[0] = nullptr;
        isDetectedCond.data.comparisonValue.f      = 1.0f;
        isDetectedCond.data.flags.opCode           = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        isDetectedCond.data.object                 = RE::CONDITIONITEMOBJECT::kTarget;
    }

    RE::ConditionCheckParams params(subject->As<RE::TESObjectREFR>(), target->As<RE::TESObjectREFR>());
    return isDetectedCond(params);
}

bool isLastHostileActor(RE::Actor* subject, RE::Actor* target)
{
    static bool                 is_initialized = false;
    static RE::TESConditionItem isLastHostileActorCond;
    if (!is_initialized)
    {
        is_initialized = true;

        isLastHostileActorCond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kIsLastHostileActor;
        isLastHostileActorCond.data.comparisonValue.f     = 1.0f;
        isLastHostileActorCond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        isLastHostileActorCond.data.object                = RE::CONDITIONITEMOBJECT::kTarget;
    }

    RE::ConditionCheckParams params(subject->As<RE::TESObjectREFR>(), target->As<RE::TESObjectREFR>());
    return isLastHostileActorCond(params);
}

bool isInPairedAnimation(RE::Actor* actor)
{
    static bool                 is_initialized = false;
    static RE::TESConditionItem isLastHostileActorCond;
    if (!is_initialized)
    {
        is_initialized = true;

        isLastHostileActorCond.data.functionData.function = RE::FUNCTION_DATA::FunctionID::kGetPairedAnimation;
        isLastHostileActorCond.data.comparisonValue.f     = 0.0f;
        isLastHostileActorCond.data.flags.opCode          = RE::CONDITION_ITEM_DATA::OpCode::kNotEqualTo;
        isLastHostileActorCond.data.object                = RE::CONDITIONITEMOBJECT::kTarget;
    }

    RE::ConditionCheckParams params(nullptr, actor->As<RE::TESObjectREFR>());
    return isLastHostileActorCond(params);
}
} // namespace phkm