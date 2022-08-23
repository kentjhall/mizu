#include <csignal>
#include <clocale>
#include <limits.h>
#include <QApplication>
#include <QHBoxLayout>
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/kernel/code_set.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "configuration/config.h"

static void on_sig(int) {
    // this allows logging to flush gracefully
    ::exit(1);
}

int main(int argc, char **argv) {
    if (::signal(SIGINT, on_sig) == SIG_ERR) {
        ::perror("signal failed");
        return 1;
    }
    if (::signal(SIGTERM, on_sig) == SIG_ERR) {
        ::perror("signal failed");
        return 1;
    }
    if (::signal(SIGSEGV, on_sig) == SIG_ERR) {
        ::perror("signal failed");
        return 1;
    }

    QCoreApplication::setOrganizationName(QStringLiteral("Kent Hall"));
    QCoreApplication::setApplicationName(QStringLiteral("mizu"));

    // logger/config initialization
    Common::Log::Initialize();
    Config::config = std::make_shared<Config>();

    // loader thread for handling launch requests
    std::thread loader_thread(Loader::RunForever);
    loader_thread.detach();

    // setup for Qt
    if (QString::fromLocal8Bit(qgetenv("DISPLAY")).isEmpty()) {
        qputenv("DISPLAY", ":0");
    }

    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication app(argc, argv);
    QWidget dummy; // prevents render window from telling the QApplication to exit
    auto* layout = new QHBoxLayout(&dummy);
    layout->setContentsMargins(0, 0, 0, 0);
    dummy.setLayout(layout);
    dummy.show();

    setlocale(LC_ALL, "C");

    // start service threads
    Service::StartServices();

    // service manager thread
    std::thread sm_thread([](){
        if (horizon_servctl(HZN_SCTL_REGISTER_NAMED_SERVICE, (unsigned long)"sm:") == -1) {
            LOG_CRITICAL(Service, "HZN_SCTL_REGISTER_NAMED_SERVICE failed");
            ::exit(1);
        }
        Service::RunForever(Kernel::SessionRequestHandlerPtr(new Service::SM::SM));
    });
    sm_thread.detach();

    return app.exec();
}
