#include "theme.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QPalette>
#include <QProcess>
#include <QSettings>
#include <QTextStream>

static ColorScheme fromBasename(const QString &name)
{
    QString lower = name.toLower();
    if (lower.contains("dark") || lower.contains("black"))
        return ColorScheme::Dark;
    if (lower.contains("light") || lower.contains("white"))
        return ColorScheme::Light;
    return ColorScheme::Unknown;
}

static ColorScheme fromGtkSettings()
{
    QFile file(QDir::homePath() + "/.config/gtk-3.0/settings.ini");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QSettings settings(file.fileName(), QSettings::IniFormat);
        settings.beginGroup("Settings");
        if (settings.value("gtk-application-prefer-dark-theme", false).toBool())
            return ColorScheme::Dark;
        ColorScheme cs = fromBasename(settings.value("gtk-theme-name").toString());
        if (cs != ColorScheme::Unknown)
            return cs;
    }
    QProcess proc;
    proc.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
    if (proc.waitForFinished(800)) {
        QString out = QString::fromUtf8(proc.readAllStandardOutput().trimmed());
        if (out.contains("prefer-dark"))
            return ColorScheme::Dark;
        if (out.contains("default")) {
            proc.start("gsettings", {"get", "org.gnome.desktop.interface", "gtk-theme"});
            if (proc.waitForFinished(800))
                return fromBasename(QString::fromUtf8(proc.readAllStandardOutput().trimmed()));
        }
    }
    return ColorScheme::Unknown;
}

static ColorScheme fromKdeGlobals()
{
    QSettings settings(QDir::homePath() + "/.config/kdeglobals", QSettings::IniFormat);
    settings.beginGroup("General");
    return fromBasename(settings.value("ColorScheme").toString());
}

ColorScheme detectColorScheme(QApplication *app)
{
    if (app) {
        QPalette pal = app->palette();
        QColor base = pal.color(QPalette::Window);
        if (base.isValid()) {
            int lum = qMax(base.red(), qMax(base.green(), base.blue()));
            return lum < 128 ? ColorScheme::Dark : ColorScheme::Light;
        }
    }
    const QByteArray gtkTheme = qgetenv("GTK_THEME");
    if (!gtkTheme.isEmpty()) {
        ColorScheme cs = fromBasename(QString::fromUtf8(gtkTheme));
        if (cs != ColorScheme::Unknown)
            return cs;
    }
    ColorScheme cs = fromGtkSettings();
    if (cs != ColorScheme::Unknown)
        return cs;
    cs = fromKdeGlobals();
    if (cs != ColorScheme::Unknown)
        return cs;
    return ColorScheme::Unknown;
}