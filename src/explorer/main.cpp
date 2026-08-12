#include <QApplication>
#include <QIcon>
#include <QPixmap>

#include "mainwindow.h"
#include "style.h"

#include "theme.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("explorer.exe"));
    app.setApplicationDisplayName(QStringLiteral("Windows 资源管理器"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("folder")));

    ColorScheme scheme = detectColorScheme(&app);
    app.setStyleSheet(win11StyleSheet(scheme == ColorScheme::Dark));

    MainWindow window;
    window.show();

    return app.exec();
}