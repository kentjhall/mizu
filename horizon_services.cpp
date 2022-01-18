#include <csignal>
#include <clocale>
#include <limits.h>
#include <SDL.h>
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/kernel/code_set.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "config/config.h"

static void on_sig(int) {
    // this allows logging to flush gracefully
    ::exit(1);
}

int main(int argc, char **argv) {
    if (::signal(SIGINT, on_sig) == SIG_ERR) {
        ::perror("signal failed");
        return 1;
    }
    if (::signal(SIGSEGV, on_sig) == SIG_ERR) {
        ::perror("signal failed");
        return 1;
    }

    // logger/config initialization
    Common::Log::Initialize();
    Config config;

    // loader thread for handling launch requests
    std::thread loader_thread(Loader::RunForever);
    loader_thread.detach();

    // setup for SDL
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL2: {}", SDL_GetError());
        return 1;
    }
    ::atexit(SDL_Quit);

    // start service threads, and run SM service in this thread
    Service::StartServices();
    Service::RunForever(Kernel::SessionRequestHandlerPtr(new Service::SM::SM));
}
