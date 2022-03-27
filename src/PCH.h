#pragma once

#define NOGDI

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>

#define DLLEXPORT __declspec(dllexport)

namespace logger = SKSE::log;
using namespace std::literals;

namespace stl
{
template <class F, class T>
void write_vfunc()
{
    REL::Relocation<std::uintptr_t> vtbl{F::VTABLE[0]};
    T::func = vtbl.write_vfunc(T::size, T::thunk);
}

template <class T>
void write_thunk_call()
{
    auto&                           trampoline = SKSE::GetTrampoline();
    REL::Relocation<std::uintptr_t> hook{REL::ID(T::id)};
    T::func = trampoline.write_call<5>(hook.address() + T::offset, T::thunk);
}
} // namespace stl

#include "Version.h"
