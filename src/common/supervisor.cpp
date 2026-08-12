#include "supervisor.h"

Supervisor::Supervisor(const QString &explorerPath, QObject *parent)
    : QObject(parent)
    , m_explorerPath(explorerPath)
    , m_proc(new QProcess(this))
    , m_intentionalStop(false)
{
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::finished, this, &Supervisor::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            emit explorerCrashed();
    });
}

void Supervisor::startExplorer(bool fromCrash)
{
    if (m_proc->state() != QProcess::NotRunning)
        return;
    m_intentionalStop = false;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("EXPLORER_SUPERVISED", "1");
    env.insert("EXPLORER_CRASH_RESTART", fromCrash ? "1" : "0");
    m_proc->setProcessEnvironment(env);
    m_proc->start(m_explorerPath, {});
    if (fromCrash)
        emit explorerRestarted();
}

void Supervisor::stopAll()
{
    m_intentionalStop = true;
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(1500))
            m_proc->kill();
    }
}

bool Supervisor::isRunning() const
{
    return m_proc->state() != QProcess::NotRunning;
}

qint64 Supervisor::processId() const
{
    return m_proc->processId();
}

void Supervisor::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_intentionalStop)
        return;
    bool crashed = (status == QProcess::CrashExit) || (exitCode != 0);
    if (crashed) {
        emit explorerCrashed();
        return;
    }
    m_intentionalStop = true;
}