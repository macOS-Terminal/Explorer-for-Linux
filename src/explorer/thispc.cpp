#include "thispc.h"

#include <QEnterEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#include "drivemap.h"

class ThisPcView::DriveCell : public QWidget
{
    Q_OBJECT
public:
    explicit DriveCell(const DriveEntry &entry, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_entry(entry)
        , m_hover(false)
    {
        setFixedSize(170, 92);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
    }

    const DriveEntry &entry() const { return m_entry; }

signals:
    void activated();

protected:
    void enterEvent(QEnterEvent *event) override
    {
        m_hover = true;
        update();
        QWidget::enterEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        m_hover = false;
        update();
        QWidget::leaveEvent(event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QRectF cell = rect().adjusted(1, 1, -1, -1);
        if (m_hover) {
            p.setBrush(QColor(0, 120, 212, 24));
            p.setPen(QPen(QColor(0, 120, 212, 120), 1));
            p.drawRoundedRect(cell, 8, 8);
        } else {
            p.setPen(QPen(QColor(0, 0, 0, 18), 1));
            p.drawRoundedRect(cell, 8, 8);
        }

        QRectF iconRect(20, 10, 56, 44);
        QLinearGradient g(iconRect.topLeft(), iconRect.bottomRight());
        QColor steel(120, 140, 160);
        QColor steelLight(190, 205, 225);
        if (m_entry.totalBytes > 0) {
            steel = QColor(70, 120, 200);
            steelLight = QColor(160, 200, 245);
        }
        g.setColorAt(0, steelLight);
        g.setColorAt(1, steel);
        p.setBrush(g);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(iconRect, 6, 6);
        p.setBrush(QColor(50, 50, 50, 70));
        p.drawRoundedRect(QRectF(iconRect.left() + 4, iconRect.bottom() - 10, iconRect.width() - 8, 4), 2, 2);
        QRectF led(iconRect.right() - 18, iconRect.center().y() - 3, 6, 6);
        p.setBrush(m_entry.totalBytes > 0 ? QColor(80, 200, 120) : QColor(200, 200, 60));
        p.drawEllipse(led);
        p.setPen(QColor(230, 240, 250));
        QFont f = font();
        f.setPixelSize(20);
        f.setBold(true);
        p.setFont(f);
        p.drawText(iconRect.adjusted(4, 6, -4, -4), Qt::AlignCenter, QString(m_entry.letter) + QLatin1Char(':'));

        QFont label = font();
        label.setPixelSize(11);
        p.setFont(label);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRect(0, 56, 170, 16), Qt::AlignCenter,
                   QStringLiteral("本地磁盘 (%1:)").arg(m_entry.letter));

        QFont meta = font();
        meta.setPixelSize(9);
        p.setFont(meta);
        p.setPen(palette().color(QPalette::Disabled, QPalette::WindowText));
        QString sizeText = m_entry.totalBytes > 0
            ? QStringLiteral("可用空间 %.1f GB / 共 %.1f GB")
                  .arg(m_entry.availBytes / 1073741824.0, 0, 'f', 1)
                  .arg(m_entry.totalBytes / 1073741824.0, 0, 'f', 1)
            : QStringLiteral("容量未知");
        p.drawText(QRect(0, 72, 170, 14), Qt::AlignCenter, sizeText);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        QWidget::mousePressEvent(event);
        if (event->button() == Qt::LeftButton)
            emit activated();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        QWidget::mouseDoubleClickEvent(event);
        emit activated();
    }

private:
    DriveEntry m_entry;
    bool m_hover;
};

class ThisPcView::Grid : public QWidget
{
    Q_OBJECT
public:
    explicit Grid(const QList<DriveEntry> &entries, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_entries(entries)
        , m_section(nullptr)
        , m_flow(nullptr)
        , m_columns(4)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 12, 20, 12);
        layout->setSpacing(6);

        m_section = new QLabel(this);
        m_section->setObjectName(QStringLiteral("ThisPcSection"));
        layout->addWidget(m_section);

        auto *gridLabel = new QLabel(tr("本地磁盘"), this);
        gridLabel->setObjectName(QStringLiteral("ThisPcSection"));
        layout->addWidget(gridLabel);

        m_flow = new QWidget(this);
        layout->addWidget(m_flow);

        rebuild();
    }

    void rebuild()
    {
        if (m_flowLayout) {
            while (QLayoutItem *item = m_flowLayout->takeAt(0))
                delete item->widget();
            delete m_flowLayout;
        }
        m_flowLayout = new QGridLayout(m_flow);
        m_flowLayout->setContentsMargins(0, 2, 0, 0);
        m_flowLayout->setSpacing(6);
        m_flowLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        int row = 0;
        int col = 0;
        for (const DriveEntry &e : m_entries) {
            auto *cell = new DriveCell(e, m_flow);
            m_flowLayout->addWidget(cell, row, col);
            connect(cell, &DriveCell::activated, this,
                    [this, e]() { emit driveActivated(e.mountPoint); });
            if (++col >= m_columns) {
                col = 0;
                ++row;
            }
        }
        m_section->setText(tr("设备与驱动器 (%1)").arg(m_entries.size()));
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);
        int cols = qMax(1, (width() - 12) / 176);
        if (cols != m_columns) {
            m_columns = cols;
            rebuild();
        }
    }

signals:
    void driveActivated(const QString &mountPoint);

private:
    QList<DriveEntry> m_entries;
    QLabel *m_section;
    QWidget *m_flow;
    QGridLayout *m_flowLayout = nullptr;
    int m_columns;
};

ThisPcView::ThisPcView(QWidget *parent)
    : QWidget(parent)
    , m_grid(nullptr)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QList<DriveEntry> drives = detectDrives();
    m_grid = new Grid(drives, scroll);
    scroll->setWidget(m_grid);
    outer->addWidget(scroll);

    connect(m_grid, &Grid::driveActivated, this, &ThisPcView::driveActivated);
}

#include "thispc.moc"