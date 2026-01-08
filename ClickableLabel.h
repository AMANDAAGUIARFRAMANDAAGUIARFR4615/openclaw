#pragma once

#include <QLabel>
#include <QWidget>
#include <QMouseEvent>

class ClickableLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ClickableLabel(const QString &text, QWidget *parent = nullptr) : QLabel(text, parent)
    {
        setCursor(Qt::PointingHandCursor);
    }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton)
            emit clicked();
        
        QLabel::mousePressEvent(event);
    }
};
