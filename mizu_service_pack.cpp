#include <csignal>
#include <clocale>
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "config/config.h"

static void on_sig(int) {
    // catching here allows logging to be flushed
    ::exit(1);
}

int main(int argc, char **argv) {
    if (::signal(SIGINT, on_sig) == SIG_ERR) {
        perror("signal failed");
        return 1;
    }
    if (::signal(SIGSEGV, on_sig) == SIG_ERR) {
        perror("signal failed");
        return 1;
    }

    Config config;

    Common::Log::Initialize();
    Settings::LogSettings();

    Service::StartServices();
    Service::RunForever(Kernel::SessionRequestHandlerPtr(new Service::SM::SM));
}
