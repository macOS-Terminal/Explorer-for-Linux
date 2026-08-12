#include "mainwindow.h"

#include <QCloseEvent>
#include <QDir>
#include <QEvent>
#include <QEventLoop>
#include <QFileIconProvider>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include "filelist.h"
#include "thispc.h"

static const QString kPcPath = QStringLiteral("__thispc__");

static bool crashAnticsEnabled()
{
    return qEnvironmentVariable("EXPLORER_CRASH_RESTART") == QLatin1String("1");
}

MainWindow::MainWindow()
    : m_thisPc(nullptr)
    , m_files(nullptr)
    , m_stack(nullptr)
    , m_sidebar(nullptr)
    , m_sidebarExtra(nullptr)
    , m_titleLabel(nullptr)
    , m_backBtn(nullptr)
    , m_forwardBtn(nullptr)
    , m_upBtn(nullptr)
    , m_breadcrumbHost(nullptr)
    , m_breadcrumbLayout(nullptr)
    , m_navStack(nullptr)
    , m_address(nullptr)
    , m_search(nullptr)
    , m_status(nullptr)
    , m_navigationCount(0)
{
    setWindowTitle(QStringLiteral("此电脑"));
    resize(1280, 800);
    setMinimumSize(720, 480);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(buildTitleBar());
    root->addWidget(buildCommandBar());

    auto *middle = new QWidget(central);
    auto *middleLayout = new QHBoxLayout(middle);
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);
    middleLayout->addWidget(buildSidebar());

    m_stack = new QStackedWidget(middle);
    m_thisPc = new ThisPcView(m_stack);
    m_files = new FileList(m_stack);
    m_stack->addWidget(m_thisPc);
    m_stack->addWidget(m_files);
    middleLayout->addWidget(m_stack, 1);
    root->addWidget(middle, 1);

    m_status = statusBar();
    m_status->setObjectName(QStringLiteral("StatusBar"));
    m_status->setSizeGripEnabled(false);
    m_status->showMessage(QStringLiteral("就绪"));

    setCentralWidget(central);

    connect(m_thisPc, &ThisPcView::driveActivated, this, &MainWindow::navigateTo);
    connect(m_files, &FileList::directoryActivated, this, &MainWindow::navigateTo);
    connect(m_sidebar, &QListWidget::itemActivated, this, &MainWindow::onSidebarActivated);
    connect(m_sidebar, &QListWidget::itemClicked, this, &MainWindow::onSidebarActivated);
    connect(m_sidebarExtra, &QListWidget::itemActivated, this, &MainWindow::onSidebarActivated);
    connect(m_sidebarExtra, &QListWidget::itemClicked, this, &MainWindow::onSidebarActivated);

    m_breadcrumbHost->installEventFilter(this);
    m_address->installEventFilter(this);

    m_notResponsive = new NotRespondingManager(this, this);

    navigateTo(kPcPath);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_breadcrumbHost && event->type() == QEvent::MouseButtonPress) {
        enterAddressMode();
        return true;
    }
    if (obj == m_address && event->type() == QEvent::KeyPress) {
        QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            leaveAddressMode();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

QWidget *MainWindow::buildTitleBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("TitleBar"));
    bar->setFixedHeight(36);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(10, 0, 0, 0);
    layout->setSpacing(8);

    auto *icon = new QLabel(bar);
    icon->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(16, 16));
    layout->addWidget(icon);

    m_titleLabel = new QLabel(bar);
    m_titleLabel->setObjectName(QStringLiteral("TitleLabel"));
    layout->addWidget(m_titleLabel);

    layout->addStretch();

    auto makeBtn = [&](const QString &glyph, const char *name, const QString &tip) {
        auto *b = new QPushButton(glyph, bar);
        b->setObjectName(QString::fromLatin1(name));
        b->setFixedSize(44, 36);
        b->setToolTip(tip);
        b->setCursor(Qt::ArrowCursor);
        return b;
    };
    QPushButton *minBtn = makeBtn(QStringLiteral("\u2014"), "TitleBtn", tr("最小化"));
    QPushButton *maxBtn = makeBtn(QStringLiteral("\u2610"), "TitleBtn", tr("最大化"));
    QPushButton *closeBtn = makeBtn(QStringLiteral("\u2715"), "TitleBtnClose", tr("关闭"));

    connect(minBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(maxBtn, &QPushButton::clicked, this, [this]() {
        if (isMaximized())
            showNormal();
        else
            showMaximized();
    });
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        if (crashAnticsEnabled())
            m_notResponsive->trigger();
    });

    layout->addWidget(minBtn);
    layout->addWidget(maxBtn);
    layout->addWidget(closeBtn);
    return bar;
}

QWidget *MainWindow::buildCommandBar()
{
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("CommandBar"));
    bar->setFixedHeight(40);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 3, 8, 3);
    layout->setSpacing(4);

    auto navBtn = [&](const QString &glyph, const char *name, const QString &tip) {
        auto *b = new QPushButton(glyph, bar);
        b->setObjectName(QString::fromLatin1(name));
        b->setFixedSize(30, 30);
        b->setToolTip(tip);
        b->setCursor(Qt::ArrowCursor);
        return b;
    };
    m_backBtn = navBtn(QStringLiteral("\u25C0"), "NavBack", tr("后退 (Alt+左)"));
    m_forwardBtn = navBtn(QStringLiteral("\u25B6"), "NavForward", tr("前进 (Alt+右)"));
    m_upBtn = navBtn(QStringLiteral("\u25B2"), "NavUp", tr("向上"));
    layout->addWidget(m_backBtn);
    layout->addWidget(m_forwardBtn);
    layout->addWidget(m_upBtn);

    m_navStack = new QStackedWidget(bar);
    m_navStack->setFixedHeight(28);

    m_breadcrumbHost = new QWidget(m_navStack);
    m_breadcrumbLayout = new QHBoxLayout(m_breadcrumbHost);
    m_breadcrumbLayout->setContentsMargins(8, 0, 8, 0);
    m_breadcrumbLayout->setSpacing(2);
    m_breadcrumbLayout->addStretch();
    m_breadcrumbHost->setCursor(Qt::PointingHandCursor);
    m_navStack->addWidget(m_breadcrumbHost);

    m_address = new QLineEdit(m_navStack);
    m_address->setObjectName(QStringLiteral("Address"));
    m_address->setClearButtonEnabled(true);
    m_address->setCursorPosition(0);
    m_navStack->addWidget(m_address);

    layout->addWidget(m_navStack, 1);

    m_search = new QLineEdit(bar);
    m_search->setObjectName(QStringLiteral("Search"));
    m_search->setPlaceholderText(QStringLiteral("搜索 Explorer For Linux"));
    m_search->setFixedSize(180, 28);
    layout->addWidget(m_search);

    connect(m_backBtn, &QPushButton::clicked, this, &MainWindow::goBack);
    connect(m_forwardBtn, &QPushButton::clicked, this, &MainWindow::goForward);
    connect(m_upBtn, &QPushButton::clicked, this, &MainWindow::goUp);
    connect(m_address, &QLineEdit::returnPressed, this, &MainWindow::onAddressEntered);
    connect(m_search, &QLineEdit::returnPressed, this, &MainWindow::onSearchEntered);

    return bar;
}

QWidget *MainWindow::buildSidebar()
{
    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("Sidebar"));
    panel->setFixedWidth(170);

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(0);

    auto sectionTitle = [&](const QString &text) {
        auto *label = new QLabel(text, panel);
        label->setObjectName(QStringLiteral("SidebarSection"));
        return label;
    };

    auto makeList = [&](QWidget *parent) {
        auto *list = new QListWidget(parent);
        list->setObjectName(QStringLiteral("SidebarList"));
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        return list;
    };
    auto addItem = [&](QListWidget *list, const QString &text, const QString &path, QStyle::StandardPixmap pix) {
        auto *item = new QListWidgetItem(list);
        item->setIcon(style()->standardIcon(pix));
        item->setText(text);
        item->setData(Qt::UserRole, path);
    };

    layout->addWidget(sectionTitle(tr("文件夹")));
    m_sidebar = makeList(panel);
    addItem(m_sidebar, tr("主页"), QDir::homePath(), QStyle::SP_DirHomeIcon);
    addItem(m_sidebar, tr("下载"), QDir::homePath() + QStringLiteral("/Downloads"), QStyle::SP_DirIcon);
    addItem(m_sidebar, tr("文档"), QDir::homePath() + QStringLiteral("/Documents"), QStyle::SP_DirIcon);
    addItem(m_sidebar, tr("图片"), QDir::homePath() + QStringLiteral("/Pictures"), QStyle::SP_DirIcon);
    layout->addWidget(m_sidebar, 1);

    layout->addSpacing(2);
    layout->addWidget(sectionTitle(tr("位置")));
    m_sidebarExtra = makeList(panel);
    addItem(m_sidebarExtra, tr("此电脑"), kPcPath, QStyle::SP_ComputerIcon);
    addItem(m_sidebarExtra, tr("回收站"), QStringLiteral("__trash__"), QStyle::SP_TrashIcon);
    layout->addWidget(m_sidebarExtra);

    return panel;
}

void MainWindow::navigateTo(const QString &path)
{
    if (path == m_currentLocation)
        return;
    if (!m_currentLocation.isEmpty())
        m_back.push(m_currentLocation);
    m_forward.clear();

    m_currentLocation = path;
    if (path == kPcPath) {
        m_stack->setCurrentWidget(m_thisPc);
        setViewTitle(QStringLiteral("此电脑"));
    } else if (path == QStringLiteral("__trash__")) {
        if (QDir home = QDir::home(); home.exists(".local/share/Trash/files")) {
            navigateTo(home.absoluteFilePath(".local/share/Trash/files"));
            return;
        }
        m_stack->setCurrentWidget(m_files);
        m_files->showDirectory(QStringLiteral("/dev/null"));
        setViewTitle(QStringLiteral("回收站"));
    } else {
        QDir dir(path);
        if (!dir.exists()) {
            m_status->showMessage(tr("找不到路径: ") + path, 4000);
            m_back.pop();
            return;
        }
        m_stack->setCurrentWidget(m_files);
        m_files->showDirectory(path);
        setViewTitle(dir.dirName().isEmpty() ? path : dir.dirName());
    }
    updateBreadcrumb(path);
    updateNavButtons();

    if (path != kPcPath && path != QStringLiteral("__trash__"))
        fakeLoad(path, false);
}

void MainWindow::goBack()
{
    if (m_back.isEmpty())
        return;
    m_forward.push(m_currentLocation);
    QString target = m_back.pop();
    m_currentLocation = target;
    navigateToNoPush(target);
}

void MainWindow::goForward()
{
    if (m_forward.isEmpty())
        return;
    QString target = m_forward.pop();
    m_back.push(m_currentLocation);
    navigateToNoPush(target);
}

void MainWindow::goUp()
{
    if (m_currentLocation == kPcPath || m_currentLocation == QStringLiteral("__trash__"))
        return;
    QDir dir(m_currentLocation);
    if (dir.cdUp()) {
        QString parent = dir.absolutePath();
        if (parent == m_currentLocation)
            parent = kPcPath;
        navigateTo(parent);
    } else {
        navigateTo(kPcPath);
    }
}

void MainWindow::navigateToNoPush(const QString &path)
{
    m_currentLocation = path;
    if (path == kPcPath) {
        m_stack->setCurrentWidget(m_thisPc);
        setViewTitle(QStringLiteral("此电脑"));
    } else {
        m_stack->setCurrentWidget(m_files);
        m_files->showDirectory(path);
        QDir dir(path);
        setViewTitle(dir.dirName().isEmpty() ? path : dir.dirName());
    }
    updateBreadcrumb(path);
    updateNavButtons();
    fakeLoad(path, false);
}

void MainWindow::updateBreadcrumb(const QString &path)
{
    while (QLayoutItem *item = m_breadcrumbLayout->takeAt(0))
        delete item->widget();

    auto addCrumb = [&](const QString &label, const QString &target, bool last) {
        auto *b = new QPushButton(label, m_breadcrumbHost);
        b->setObjectName(last ? QStringLiteral("CrumbLast") : QStringLiteral("Crumb"));
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        connect(b, &QPushButton::clicked, this, [this, target]() { navigateTo(target); });
        m_breadcrumbLayout->addWidget(b);
    };
    auto addSep = [&]() {
        auto *sep = new QLabel(QStringLiteral("\u203A"), m_breadcrumbHost);
        sep->setObjectName(QStringLiteral("CrumbSep"));
        m_breadcrumbLayout->addWidget(sep);
    };

    addCrumb(QStringLiteral("此电脑"), kPcPath, path == kPcPath);
    if (path != kPcPath) {
        addSep();
        QStringList parts = QDir(path).absolutePath().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString accum;
        for (int i = 0; i < parts.size(); ++i) {
            accum = accum.isEmpty() ? QStringLiteral("/") + parts[i] : accum + QLatin1Char('/') + parts[i];
            if (i == parts.size() - 1) {
                addCrumb(parts[i], accum, true);
            } else {
                addCrumb(parts[i], accum, false);
            }
            if (i != parts.size() - 1)
                addSep();
        }
    }
}

void MainWindow::updateNavButtons()
{
    m_backBtn->setEnabled(!m_back.isEmpty());
    m_forwardBtn->setEnabled(!m_forward.isEmpty());
    m_upBtn->setEnabled(m_currentLocation != kPcPath);
}

void MainWindow::enterAddressMode()
{
    m_address->setText(m_currentLocation);
    m_navStack->setCurrentWidget(m_address);
    m_address->setFocus(Qt::MouseFocusReason);
    m_address->selectAll();
}

void MainWindow::leaveAddressMode()
{
    m_navStack->setCurrentWidget(m_breadcrumbHost);
}

void MainWindow::fakeLoad(const QString &path, bool force)
{
    ++m_navigationCount;
    bool heavy = path == QStringLiteral("/") || QDir(path).entryInfoList(QDir::AllEntries | QDir::Hidden).size() > 3000;
    bool randomTrap = crashAnticsEnabled()
        && ((QRandomGenerator::global()->bounded(100) < 12) || (m_navigationCount % 15 == 14));
    if (!force && !heavy && !randomTrap)
        return;

    m_status->showMessage(QStringLiteral("正在计算项目大小…"));
    {
        QEventLoop loop;
        QTimer::singleShot(heavy ? 2200 : 900, &loop, &QEventLoop::quit);
        loop.exec();
    }
    m_status->showMessage(QStringLiteral("就绪"));
    m_notResponsive->trigger();
}

void MainWindow::onAddressEntered()
{
    QString text = m_address->text().trimmed();
    if (text.isEmpty()) {
        leaveAddressMode();
        return;
    }
    navigateTo(text);
    leaveAddressMode();
}

void MainWindow::onSearchEntered()
{
    m_status->showMessage(QStringLiteral("正在搜索 \"") + m_search->text().trimmed() + QStringLiteral("\" …"));
    {
        QEventLoop loop;
        QTimer::singleShot(1600, &loop, &QEventLoop::quit);
        loop.exec();
    }
    if ((m_search->text().trimmed().isEmpty() ? 1 : 0)
        || (crashAnticsEnabled() && QRandomGenerator::global()->bounded(100) < 60))
        m_notResponsive->trigger();
}

void MainWindow::onSidebarActivated(QListWidgetItem *item)
{
    if (!item)
        return;
    QString target = item->data(Qt::UserRole).toString();
    navigateTo(target);
}

void MainWindow::setViewTitle(const QString &title)
{
    setWindowTitle(title);
    if (m_titleLabel)
        m_titleLabel->setText(title);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    if (crashAnticsEnabled())
        m_notResponsive->trigger();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (crashAnticsEnabled() && !m_chaosStarted) {
        m_chaosStarted = true;
        QTimer::singleShot(900, this, [this]() { m_notResponsive->trigger(); });
    }
}