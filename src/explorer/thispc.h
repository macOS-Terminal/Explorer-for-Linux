#pragma once

#include <QWidget>

class ThisPcView : public QWidget
{
    Q_OBJECT
public:
    explicit ThisPcView(QWidget *parent = nullptr);

signals:
    void driveActivated(const QString &mountPoint);

private:
    class DriveCell;
    class Grid;
    Grid *m_grid;
};