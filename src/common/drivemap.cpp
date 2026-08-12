#include "drivemap.h"

#include <QFile>
#include <QStringList>
#include <QTextStream>

#include <sys/statvfs.h>

static const QStringList kPseudoFs = {
    "proc", "sysfs", "devtmpfs", "tmpfs", "devpts", "cgroup", "cgroup2",
    "mqueue", "hugetlbfs", "securityfs", "debugfs", "tracefs", "configfs",
    "binfmt_misc", "autofs", "pstore", "fusectl", "rpc_pipefs", "nsfs",
    "efivarfs", "ramfs", "bpf", "iozone", "cramfs", "squashfs"
};

QList<DriveEntry> detectDrives()
{
    struct MountInfo {
        QString root;
        QString mountPoint;
        QString fsType;
    };
    QList<MountInfo> mounts;

    QFile file("/proc/self/mountinfo");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        for (QString line = in.readLine(); !line.isNull(); line = in.readLine()) {
            QStringList parts = line.split(' ');
            if (parts.size() < 6)
                continue;
            int dash = parts.indexOf('-');
            if (dash < 5 || dash + 2 >= parts.size())
                continue;
            MountInfo mi;
            mi.mountPoint = parts[4];
            mi.fsType = parts[dash + 1];
            mi.root = parts[3];
            if (kPseudoFs.contains(mi.fsType))
                continue;
            if (mi.fsType.startsWith(QLatin1String("fuse.")))
                continue;
            mounts.append(mi);
        }
    }

    if (mounts.isEmpty()) {
        DriveEntry root;
        root.letter = QLatin1Char('C');
        root.mountPoint = QStringLiteral("/");
        root.fsType = QStringLiteral("unknown");
        root.totalBytes = 0;
        root.availBytes = 0;
        return {root};
    }

    std::stable_sort(mounts.begin(), mounts.end(),
                     [](const MountInfo &a, const MountInfo &b) {
                         return a.mountPoint.length() < b.mountPoint.length();
                     });

    QList<DriveEntry> drives;
    QChar nextLetter = QLatin1Char('C');
    for (const MountInfo &m : mounts) {
        if (nextLetter > QLatin1Char('Z'))
            break;
        DriveEntry de;
        de.letter = nextLetter;
        nextLetter = QChar(nextLetter.unicode() + 1);
        de.mountPoint = m.mountPoint;
        de.fsType = m.fsType;
        struct statvfs sv;
        if (statvfs(m.mountPoint.toUtf8().constData(), &sv) == 0) {
            de.totalBytes = quint64(sv.f_blocks) * quint64(sv.f_frsize);
            de.availBytes = quint64(sv.f_bavail) * quint64(sv.f_frsize);
        } else {
            de.totalBytes = 0;
            de.availBytes = 0;
        }
        drives.append(de);
    }
    return drives;
}