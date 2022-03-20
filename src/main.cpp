#include "hooks.h"
#include "files.h"
#include "phkm.h"

bool installLog()
{
    auto path = logger::log_directory();
    if (!path)
        return false;

    *path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
    log->set_level(spdlog::level::trace);
    log->flush_on(spdlog::level::trace);
#else
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
#endif

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%H:%M:%S:%e] %v"s);

    return true;
}

namespace phkm
{
void processMessage(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type)
    {
        case SKSE::MessagingInterface::kDataLoaded:
            logger::debug("kDataLoaded");
            if (ConfigParser::getSingleton()->isEnabled())
            {
                AnimEntryParser::getSingleton()->readEntries();
            }
            addConditionToParaFX();
            break;
        case SKSE::MessagingInterface::kPostLoad:
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            logger::debug("kPostLoadGame");
            break;
        case SKSE::MessagingInterface::kPostPostLoad:
            break;
        default:
            break;
    }
}
} // namespace phkm

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name        = Version::PROJECT;
    a_info->version     = Version::VERSION[0];

    if (a_skse->IsEditor())
    {
        logger::critical("Loaded in editor, marking as incompatible"sv);
        return false;
    }

    const auto ver = a_skse->RuntimeVersion();
    if (ver < SKSE::RUNTIME_1_5_39)
    {
        logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
        return false;
    }

    return true;
}

#ifndef BUILD_SE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
    SKSE::PluginVersionData v;

    v.PluginVersion(Version::VERSION);
    v.PluginName(Version::PROJECT);

    v.UsesAddressLibrary(true);
    v.CompatibleVersions({SKSE::RUNTIME_LATEST});

    return v;
}();
#endif

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    installLog();

    logger::info("Plugin loaded.");

    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(1 << 4);

    using namespace phkm;

    try
    {
        if (!ConfigParser::getSingleton()->readConfig())
        {
            logger::critical("Failed to load config file. Mod disabled.");
            return false;
        }
    }
    catch (nlohmann::json::type_error e)
    {
        logger::critical("Failed to parse config file. Mod disabled.");
        return false;
    }
    logger::info("Config loaded.");

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", processMessage))
    {
        return false;
    }

    // stl::write_vfunc<RE::Actor, HandleDamageVfunc>();
    // stl::write_vfunc<RE::Character, HandleDamageVfunc>();
    // stl::write_vfunc<RE::PlayerCharacter, HandleDamageVfunc>();

    stl::write_thunk_call<ProcessHitHook>();
    logger::info("Hook installed.");

    return true;
}