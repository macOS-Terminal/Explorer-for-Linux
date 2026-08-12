#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class Supervisor : public QObject
{
    Q_OBJECT
public:
    explicit Supervisor(const QString &explorerPath, QObject *parent = nullptr);

    void startExplorer(bool fromCrash = false);
    void stopAll();
    bool isRunning() const;
    qint64 processId() const;

signals:
    void explorerCrashed();
    void explorerRestarted();

private slots:
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString m_explorerPath;
    QProcess *m_proc;
    bool m_intentionalStop;
};