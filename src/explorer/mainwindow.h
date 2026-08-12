#pragma once

#include <QListWidgetItem>
#include <QMainWindow>
#include <QStack>

#include "notresponding.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QHBoxLayout;
class QStackedWidget;
class QStatusBar;
class ThisPcView;
class FileList;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void navigateTo(const QString &path);
    void goBack();
    void goForward();
    void goUp();
    void onAddressEntered();
    void onSearchEntered();
    void onSidebarActivated(QListWidgetItem *item);

private:
    QWidget *buildTitleBar();
    QWidget *buildCommandBar();
    QWidget *buildSidebar();
    void navigateToNoPush(const QString &path);
    void updateBreadcrumb(const QString &path);
    void updateNavButtons();
    void enterAddressMode();
    void leaveAddressMode();
    void fakeLoad(const QString &path, bool force);
    void setViewTitle(const QString &title);

    QStack<QString> m_back;
    QStack<QString> m_forward;
    QString m_currentLocation;

    ThisPcView *m_thisPc;
    FileList *m_files;
    QStackedWidget *m_stack;
    QListWidget *m_sidebar;
    QListWidget *m_sidebarExtra;

    QLabel *m_titleLabel;
    QPushButton *m_backBtn;
    QPushButton *m_forwardBtn;
    QPushButton *m_upBtn;
    QWidget *m_breadcrumbHost;
    QHBoxLayout *m_breadcrumbLayout;
    QStackedWidget *m_navStack;
    QLineEdit *m_address;
    QLineEdit *m_search;
    QStatusBar *m_status;

    NotRespondingManager *m_notResponsive;

    int m_navigationCount;
    bool m_chaosStarted = false;
};