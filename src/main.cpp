#include "hooks.h"
#include "events.h"
#include "files.h"
#include "ui.h"
#include "cathub.h"

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
    cathub::CatHubAPI* cathub_api = nullptr;
    switch (a_msg->type)
    {
        case SKSE::MessagingInterface::kDataLoaded:
            logger::info("Reading Entries.");
            EntryParser::getSingleton()->readAnimEntries();
            EntryParser::getSingleton()->readKeyEntries();

            logger::info("Registering Events.");
            InputEventSink::RegisterSink();

            logger::info("Installing Hook.");
            stl::write_thunk_call<ProcessHitHook>();
            stl::write_thunk_call<UpdateHook>();

            logger::info("Looking for CatHub.");
            cathub_api = cathub::RequestCatHubAPI();
            if (cathub_api)
            {
                ImGui::SetCurrentContext(cathub_api->getContext());
                cathub_api->addMenu("Post-hit Killmoves", drawMenu);
                logger::info("CatHub integration succeed!");
            }
            else
                logger::warn("CatHub not found. In-game configuration disabled!");

            break;
        case SKSE::MessagingInterface::kPostLoad:
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            DelayedFuncModule::getSingleton()->flush();
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
    a_info->name        = Version::PROJECT.data();
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
    SKSE::AllocTrampoline(2 * 14);

    using namespace phkm;

    PhkmConfig::getSingleton()->readConfig();
    logger::info("Config loaded.");

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", processMessage))
    {
        return false;
    }

    // stl::write_vfunc<RE::Actor, HandleDamageVfunc>();
    // stl::write_vfunc<RE::Character, HandleDamageVfunc>();
    // stl::write_vfunc<RE::PlayerCharacter, HandleDamageVfunc>();

    return true;
}