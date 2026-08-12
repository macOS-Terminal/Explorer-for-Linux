#include "crashscreen.h"

#include <QApplication>
#include <QAction>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>

#include "keygrab.h"
#include "supervisor.h"
#include "theme.h"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

static volatile sig_atomic_t g_usr1Flag = 0;

static void onUsr1(int)
{
    g_usr1Flag = 1;
}

static void installSigHandlers()
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = onUsr1;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, nullptr);
    struct sigaction sa2;
    std::memset(&sa2, 0, sizeof(sa2));
    sa2.sa_handler = [](int) {
        QCoreApplication::quit();
    };
    sigemptyset(&sa2.sa_mask);
    sigaction(SIGTERM, &sa2, nullptr);
    sigaction(SIGINT, &sa2, nullptr);
}

static QIcon makeTrayIcon(ColorScheme scheme)
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    QColor c = scheme == ColorScheme::Dark ? QColor(0, 120, 212) : QColor(0, 99, 177);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(c);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(6, 10, 52, 44, 8, 8);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(20);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(6, 10, 52, 44), Qt::AlignCenter, QStringLiteral("WIN"));
    return QIcon(pm);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("winlogin.exe"));
    app.setApplicationDisplayName(QStringLiteral("Windows 登录进程"));
    QApplication::setQuitOnLastWindowClosed(false);

    installSigHandlers();

    QString appDir = QCoreApplication::applicationDirPath();
    QString explorerBin = qEnvironmentVariable("EXPLORER_BIN");
    if (explorerBin.isEmpty())
        explorerBin = appDir + QStringLiteral("/explorer.exe");

    ColorScheme scheme = detectColorScheme(&app);
    Supervisor supervisor(explorerBin);
    CrashScreen screen(&supervisor, scheme);
    KeyBackend *backend = KeyBackend::create(&app);

    QObject::connect(&supervisor, &Supervisor::explorerCrashed,
                     &screen, &CrashScreen::showTakeover);
    QObject::connect(&screen, &CrashScreen::relaunchRequested, &app, [&] {
        supervisor.startExplorer(true);
        screen.demote();
    });
    QObject::connect(&screen, &CrashScreen::emergencyExitRequested, &app, [&] {
        supervisor.stopAll();
        QCoreApplication::quit();
    });
    QObject::connect(backend, &KeyBackend::relaunchRequested, &app, [&] {
        if (!supervisor.isRunning()) {
            supervisor.startExplorer(true);
            screen.demote();
        }
    });
    QObject::connect(backend, &KeyBackend::emergencyExitTriggered, &app, [&] {
        supervisor.stopAll();
        QCoreApplication::quit();
    });

    QTimer usr1Poll;
    QObject::connect(&usr1Poll, &QTimer::timeout, &app, [&] {
        if (g_usr1Flag) {
            g_usr1Flag = 0;
            supervisor.stopAll();
            QCoreApplication::quit();
        }
    });
    usr1Poll.start(250);

    QSystemTrayIcon tray(makeTrayIcon(scheme));
    tray.setToolTip(QStringLiteral("winlogin.exe — 受保护的登录进程"));
    QMenu *trayMenu = new QMenu();
    QAction *relaunchAct = trayMenu->addAction(QStringLiteral("重新拉起 explorer.exe"));
    QAction *crashAct = trayMenu->addAction(QStringLiteral("模拟崩溃 explorer.exe (SIGSEGV)"));
    QAction *quitAct = trayMenu->addAction(QStringLiteral("强制退出 winlogin.exe"));
    tray.setContextMenu(trayMenu);
    tray.show();

    QObject::connect(relaunchAct, &QAction::triggered, &app, [&] {
        if (!supervisor.isRunning()) {
            supervisor.startExplorer(true);
            screen.demote();
        }
    });
    QObject::connect(crashAct, &QAction::triggered, &app, [&] {
        qint64 pid = supervisor.processId();
        if (pid > 0)
            ::kill(static_cast<pid_t>(pid), SIGSEGV);
    });
    QObject::connect(quitAct, &QAction::triggered, &app, [&] {
        supervisor.stopAll();
        QCoreApplication::quit();
    });

    supervisor.startExplorer(false);

    qInfo().noquote() << "winlogin.exe backend:" << backend->backendName()
                      << "| scheme:" << (scheme == ColorScheme::Dark ? "dark" : "light")
                      << "| explorer:" << explorerBin;

    return app.exec();
}