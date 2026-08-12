#pragma once

#include <QObject>
#include <QString>

class KeyBackend : public QObject
{
    Q_OBJECT
public:
    explicit KeyBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~KeyBackend();
    static KeyBackend *create(QObject *parent = nullptr);
    virtual QString backendName() const = 0;

signals:
    void emergencyExitTriggered();
    void relaunchRequested();
};