#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QWidget>

#include "theme.h"

class Supervisor;

class CrashScreen : public QObject
{
    Q_OBJECT
public:
    explicit CrashScreen(Supervisor *supervisor, ColorScheme scheme, QObject *parent = nullptr);
    ~CrashScreen() override;

    void showTakeover();
    void demote();
    void promote();

signals:
    void relaunchRequested();
    void emergencyExitRequested();

private slots:
    void onGuardOutput();
    void onGuardFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString guardPath() const;
    void startGuard();
    void stopGuard();
    void showFallback();
    void deleteFallback();

    Supervisor *m_supervisor;
    ColorScheme m_scheme;
    QString m_guardPath;
    bool m_useGuard;
    bool m_takeover;
    QProcess *m_guard;
    QWidget *m_fallback;
    QTimer *m_focusTimer = nullptr;
};