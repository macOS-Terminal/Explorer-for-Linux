#pragma once

#include <QChar>
#include <QList>
#include <QString>

struct DriveEntry {
    QChar letter;
    QString mountPoint;
    QString fsType;
    quint64 totalBytes;
    quint64 availBytes;
};

QList<DriveEntry> detectDrives();