#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <QWidget>

class FileList : public QWidget
{
    Q_OBJECT
public:
    explicit FileList(QWidget *parent = nullptr);

    void showDirectory(const QString &path);
    QString currentDirectory() const;

signals:
    void directoryActivated(const QString &path);

private slots:
    void onDoubleClicked(const QModelIndex &index);

private:
    class Model;
    Model *m_model;
    class View;
    View *m_view;
    QString m_current;
};