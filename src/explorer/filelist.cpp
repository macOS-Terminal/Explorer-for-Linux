#include "filelist.h"

#include <QFileIconProvider>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QScrollBar>
#include <QTreeView>
#include <QVBoxLayout>

class FileList::Model : public QFileSystemModel
{
public:
    explicit Model(QObject *parent = nullptr) : QFileSystemModel(parent)
    {
        setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        setReadOnly(true);
        setNameFilters(QStringList());
        setNameFilterDisables(false);
    }
};

class FileList::View : public QTreeView
{
public:
    explicit View(QWidget *parent = nullptr) : QTreeView(parent)
    {
        setRootIsDecorated(true);
        setUniformRowHeights(true);
        setIndentation(18);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setAlternatingRowColors(false);
        setAnimated(false);
        setSortingEnabled(true);

        QHeaderView *h = header();
        h->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        h->setSectionResizeMode(QHeaderView::Interactive);
        h->setStretchLastSection(true);
        h->setMinimumSectionSize(64);
    }
};

FileList::FileList(QWidget *parent)
    : QWidget(parent)
    , m_model(new Model(this))
    , m_view(new View(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_model->setRootPath(QStringLiteral("/"));
    m_view->setModel(m_model);
    m_view->header()->resizeSection(0, 320);
    m_view->header()->resizeSection(1, 90);
    m_view->header()->resizeSection(2, 90);
    m_view->header()->resizeSection(3, 150);
    layout->addWidget(m_view);

    connect(m_view, &QTreeView::doubleClicked, this, &FileList::onDoubleClicked);
}

void FileList::showDirectory(const QString &path)
{
    m_current = path;
    m_view->setRootIndex(m_model->index(path));
    m_view->expand(m_model->index(path));
    m_view->verticalScrollBar()->setValue(0);
}

QString FileList::currentDirectory() const
{
    return m_current;
}

void FileList::onDoubleClicked(const QModelIndex &index)
{
    if (!m_model->isDir(index))
        return;
    QString path = m_model->filePath(index);
    emit directoryActivated(path);
}

#include "filelist.moc"