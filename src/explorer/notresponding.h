#pragma once

#include <QList>
#include <QObject>
#include <QPoint>
#include <QWidget>

class NotRespondingDialog;

class NotRespondingManager : public QObject
{
    Q_OBJECT
public:
    explicit NotRespondingManager(QWidget *target, QObject *parent = nullptr);

    void trigger();
    void closeAll();

signals:
    void closingRequested();

private slots:
    void onDialogClicked(NotRespondingDialog *dialog);

private:
    struct Layer {
        QWidget *fog;
        NotRespondingDialog *dialog;
        int depth;
    };
    Layer spawnLayer(QRect anchorRect, int depth);
    void markUnresponsive(Layer &layer);

    QWidget *m_target;
    QList<Layer> m_layers;
    int m_maxDepth;
};