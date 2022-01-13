#include <csignal>
#include <clocale>
#include <QApplication>
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

    // Init settings params
    QCoreApplication::setOrganizationName(QStringLiteral("yuzu team"));
    QCoreApplication::setApplicationName(QStringLiteral("yuzu"));

    // Set the DISPLAY variable in order to open web browsers
    // TODO (lat9nq): Find a better solution for AppImages to start external applications
    if (QString::fromLocal8Bit(qgetenv("DISPLAY")).isEmpty()) {
        qputenv("DISPLAY", ":0");
    }

    // Enables the core to make the qt created contexts current on std::threads
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);
    QApplication app(argc, argv);

    // Qt changes the locale and causes issues in float conversion using std::to_string() when
    // generating shaders
    ::setlocale(LC_ALL, "C");

#if defined(HAVE_SDL2)
    SDL_InitSubSystem(SDL_INIT_VIDEO);
    // SDL disables the screen saver by default, and setting the hint
    // SDL_HINT_VIDEO_ALLOW_SCREENSAVER doesn't seem to work, so we just enable the screen saver
    // for now.
    SDL_EnableScreenSaver();
#endif

    Service::StartServices();
    Service::RunForever(Kernel::SessionRequestHandlerPtr(new Service::SM::SM));
}
