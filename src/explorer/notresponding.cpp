#include "notresponding.h"

#include <QEventLoop>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <csignal>

class NotRespondingDialog : public QWidget
{
    Q_OBJECT
public:
    explicit NotRespondingDialog(int depth, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_depth(depth)
    {
        setObjectName(QStringLiteral("OverlayBox"));
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setFixedSize(460, 150);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(14, 10, 14, 12);
        layout->setSpacing(6);

        auto *title = new QLabel(this);
        title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 13px;"));
        title->setText(depth == 0 ? QStringLiteral("explorer.exe 未响应")
                                  : QStringLiteral("explorer.exe（未响应）"));
        layout->addWidget(title);

        auto *text = new QLabel(this);
        text->setText(QStringLiteral("程序未响应，可能会导致系统不稳定。\n")
                      + QStringLiteral("窗口仍在响应？请等待（第 %1 层）。").arg(depth + 1));
        layout->addWidget(text);

        auto *buttons = new QWidget(this);
        auto *h = new QHBoxLayout(buttons);
        h->setContentsMargins(0, 0, 0, 0);
        h->addStretch();
        m_alwaysWait = new QPushButton(QStringLiteral("始终等待"), buttons);
        m_closeProgram = new QPushButton(QStringLiteral("关闭程序"), buttons);
        m_waitShortcut = new QPushButton(QStringLiteral("等待程序响应(S)"), buttons);
        if (depth < 2)
            m_closeProgram->hide();
        h->addWidget(m_alwaysWait);
        h->addWidget(m_closeProgram);
        h->addWidget(m_waitShortcut);
        layout->addWidget(buttons);

        connect(m_waitShortcut, &QPushButton::clicked, this, &NotRespondingDialog::spawnNext);
        connect(m_alwaysWait, &QPushButton::clicked, this, &NotRespondingDialog::spawnNext);
        connect(m_closeProgram, &QPushButton::clicked, this, &NotRespondingDialog::closeProgram);

        setCursor(Qt::PointingHandCursor);
    }

    int depth() const { return m_depth; }

signals:
    void spawnNext();
    void closeProgram();

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QWidget::mousePressEvent(event);
        emit spawnNext();
    }

private:
    int m_depth;
    QPushButton *m_alwaysWait;
    QPushButton *m_closeProgram;
    QPushButton *m_waitShortcut;
};

class FogOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit FogOverlay(bool dark, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_dark(dark)
    {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), m_dark ? QColor(24, 24, 24, 220) : QColor(245, 245, 245, 215));
        QColor pen = m_dark ? QColor(190, 190, 190) : QColor(120, 120, 120);
        p.setPen(pen);
        QFont f = font();
        f.setPixelSize(12);
        p.setFont(f);
        p.drawText(rect().adjusted(10, 8, -10, -10), Qt::AlignTop | Qt::AlignHCenter,
                   QStringLiteral("程序未响应"));
    }

private:
    bool m_dark;
};

NotRespondingManager::NotRespondingManager(QWidget *target, QObject *parent)
    : QObject(parent)
    , m_target(target)
    , m_maxDepth(24)
{
}

static void jankyStutter(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

NotRespondingManager::Layer NotRespondingManager::spawnLayer(QRect anchorRect, int depth)
{
    Layer layer;
    layer.depth = depth;

    bool dark = m_target->palette().window().color().lightness() < 128;
    layer.fog = new FogOverlay(dark);
    layer.fog->setGeometry(anchorRect);
    layer.fog->show();

    QPoint pos = anchorRect.topLeft() + QPoint(48, 36);
    layer.dialog = new NotRespondingDialog(depth);
    layer.dialog->move(pos);
    layer.dialog->show();
    layer.dialog->raise();

    connect(layer.dialog, &NotRespondingDialog::spawnNext, this,
            [this, layer]() { onDialogClicked(layer.dialog); });
    connect(layer.dialog, &NotRespondingDialog::closeProgram, this, [this]() {
        if (qEnvironmentVariableIsSet("EXPLORER_SUPERVISED")) {
            std::fflush(nullptr);
            std::raise(SIGSEGV);
        } else {
            closeAll();
        }
    });
    return layer;
}

void NotRespondingManager::trigger()
{
    if (qEnvironmentVariable("EXPLORER_CRASH_RESTART") != QLatin1String("1"))
        return;
    m_target->raise();
    QRect anchor = QRect(m_target->mapToGlobal(QPoint(0, 0)),
                         m_target->frameGeometry().size());
    m_layers.append(spawnLayer(anchor, 0));
}

void NotRespondingManager::closeAll()
{
    for (Layer &l : m_layers) {
        l.dialog->deleteLater();
        l.fog->deleteLater();
    }
    m_layers.clear();
}

void NotRespondingManager::markUnresponsive(Layer &layer)
{
    layer.dialog->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    layer.dialog->setWindowOpacity(0.55);
}

void NotRespondingManager::onDialogClicked(NotRespondingDialog *dialog)
{
    int index = -1;
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].dialog == dialog) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return;
    if (index >= m_maxDepth) {
        jankyStutter(400);
        return;
    }
    markUnresponsive(m_layers[index]);
    jankyStutter(180);
    QRect anchor = QRect(dialog->mapToGlobal(QPoint(0, 0)), dialog->size());
    m_layers.append(spawnLayer(anchor, index + 1));
    m_target->raise();
}

#include "notresponding.moc"