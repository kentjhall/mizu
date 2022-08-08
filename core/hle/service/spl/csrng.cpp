// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#include "core/hle/service/spl/csrng.h"

namespace Service::SPL {

CSRNG::CSRNG(Core::System& system_, std::shared_ptr<Module> module_)
    : Interface(system_, std::move(module_), "csrng") {
    static const FunctionInfo functions[] = {
        {0, &CSRNG::GenerateRandomBytes, "GenerateRandomBytes"},
    };
    RegisterHandlers(functions);
}

CSRNG::~CSRNG() = default;

} // namespace Service::SPL
