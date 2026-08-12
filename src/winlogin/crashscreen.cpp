#include "crashscreen.h"

#include <QFile>
#include <QKeyEvent>
#include <QPainter>
#include <QWindow>

#include <csignal>
#include <unistd.h>

class CrashScreenWindow : public QWidget
{
    Q_OBJECT
public:
    explicit CrashScreenWindow(ColorScheme scheme)
        : QWidget(nullptr)
        , m_scheme(scheme)
    {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setCursor(Qt::BlankCursor);
        setFocusPolicy(Qt::StrongFocus);
    }

signals:
    void superE();
    void shiftF10();

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        bool dark = m_scheme != ColorScheme::Light;
        p.fillRect(rect(), dark ? QColor(0, 0, 0) : QColor(255, 255, 255));
    }

    void keyPressEvent(QKeyEvent *e) override
    {
        if (e->isAutoRepeat()) {
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_F10 && (e->modifiers() & Qt::ShiftModifier)) {
            emit shiftF10();
            e->accept();
            return;
        }
        if (e->key() == Qt::Key_E && (e->modifiers() & Qt::MetaModifier)) {
            emit superE();
            e->accept();
            return;
        }
        QWidget::keyPressEvent(e);
    }

    void mousePressEvent(QMouseEvent *) override {}
    void mouseMoveEvent(QMouseEvent *) override {}

private:
    ColorScheme m_scheme;
};

CrashScreen::CrashScreen(Supervisor *supervisor, ColorScheme scheme, QObject *parent)
    : QObject(parent)
    , m_supervisor(supervisor)
    , m_scheme(scheme)
    , m_guardPath(guardPath())
    , m_useGuard(false)
    , m_takeover(false)
    , m_guard(nullptr)
    , m_fallback(nullptr)
{
    QByteArray platform = qgetenv("XDG_SESSION_TYPE");
    bool wayland = platform == "wayland";
    m_useGuard = false;
    if (wayland && QFile::exists(m_guardPath)) {
        QProcess probe;
        probe.start(m_guardPath, {QStringLiteral("probe")});
        if (probe.waitForFinished(1500)
            && probe.exitStatus() == QProcess::NormalExit
            && probe.exitCode() == 0)
            m_useGuard = true;
    }
}

CrashScreen::~CrashScreen()
{
    stopGuard();
    if (m_fallback)
        delete m_fallback;
}

QString CrashScreen::guardPath() const
{
    QString dir = QString::fromLocal8Bit(qgetenv("EXPLORER_HELPER_DIR"));
    if (dir.isEmpty())
        dir = QCoreApplication::applicationDirPath();
    return dir + QStringLiteral("/crashguard");
}

void CrashScreen::showTakeover()
{
    m_takeover = true;
    if (m_useGuard) {
        if (m_guard && m_guard->state() != QProcess::NotRunning) {
            ::kill(static_cast<pid_t>(m_guard->processId()), SIGUSR2);
            return;
        }
        startGuard();
        return;
    }
    showFallback();
}

void CrashScreen::showFallback()
{
    if (!m_fallback) {
        m_fallback = new CrashScreenWindow(m_scheme);
        m_fallback->setFocusPolicy(Qt::StrongFocus);
        auto *win = static_cast<CrashScreenWindow *>(m_fallback);
        connect(win, &CrashScreenWindow::superE, this, &CrashScreen::relaunchRequested);
        connect(win, &CrashScreenWindow::shiftF10, this, &CrashScreen::emergencyExitRequested);
    }
    m_fallback->showFullScreen();
    m_fallback->raise();
    m_fallback->activateWindow();
    m_fallback->setFocus(Qt::ActiveWindowFocusReason);
    m_fallback->grabKeyboard();
    if (!m_focusTimer) {
        m_focusTimer = new QTimer(this);
        connect(m_focusTimer, &QTimer::timeout, this, [this]() {
            if (!m_takeover) {
                m_focusTimer->stop();
                return;
            }
            if (m_fallback && m_fallback->isVisible()) {
                m_fallback->raise();
                m_fallback->activateWindow();
                m_fallback->setFocus(Qt::ActiveWindowFocusReason);
            }
        });
        m_focusTimer->start(400);
    }
}

void CrashScreen::deleteFallback()
{
    if (m_fallback) {
        delete m_fallback;
        m_fallback = nullptr;
    }
}

void CrashScreen::startGuard()
{
    m_guard = new QProcess(this);
    connect(m_guard, &QProcess::readyReadStandardOutput, this, &CrashScreen::onGuardOutput);
    connect(m_guard, &QProcess::finished, this, &CrashScreen::onGuardFinished);
    QStringList args;
    args << (m_scheme == ColorScheme::Dark ? QStringLiteral("black") : QStringLiteral("white"));
    m_guard->start(m_guardPath, args);
}

void CrashScreen::stopGuard()
{
    m_takeover = false;
    if (m_focusTimer)
        m_focusTimer->stop();
    if (!m_guard)
        return;
    if (m_guard->state() != QProcess::NotRunning) {
        m_guard->terminate();
        if (!m_guard->waitForFinished(800))
            m_guard->kill();
    }
    delete m_guard;
    m_guard = nullptr;
}

void CrashScreen::onGuardOutput()
{
    if (m_guard && m_guard->canReadLine()) {
        m_guard->readLine();
        emit relaunchRequested();
    }
}

void CrashScreen::onGuardFinished(int, QProcess::ExitStatus)
{
    m_guard = nullptr;
    if (m_takeover) {
        showFallback();
        return;
    }
    deleteFallback();
}

void CrashScreen::demote()
{
    if (m_guard && m_guard->state() != QProcess::NotRunning)
        ::kill(static_cast<pid_t>(m_guard->processId()), SIGUSR1);
    else if (m_fallback) {
        m_fallback->hide();
        if (m_focusTimer)
            m_focusTimer->stop();
    }
}

void CrashScreen::promote()
{
    if (m_guard && m_guard->state() != QProcess::NotRunning)
        ::kill(static_cast<pid_t>(m_guard->processId()), SIGUSR2);
    else if (m_fallback)
        m_fallback->raise();
}

#include "crashscreen.moc"