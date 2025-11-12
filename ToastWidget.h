#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QTimer>

class ToastWidget : public QWidget {
public:
    explicit ToastWidget(const QString &message, QWidget *parent = nullptr) : QWidget(parent) {
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);

        QLabel *label = new QLabel(message, this);
        label->setStyleSheet("color: white; font-size: 16px; font-weight: bold; padding: 15px;");

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(label);

        // 阴影效果
        auto shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(20);
        shadow->setOffset(6, 6);
        shadow->setColor(QColor(0, 0, 0, 180));
        setGraphicsEffect(shadow);

        // 动画效果
        auto fadeIn = new QPropertyAnimation(this, "windowOpacity");
        fadeIn->setDuration(500);
        fadeIn->setStartValue(0);
        fadeIn->setEndValue(1);
        fadeIn->start();

        show();

        // 自动关闭
        QTimer::singleShot(2000, [this, fadeIn] {
            auto fadeOut = new QPropertyAnimation(this, "windowOpacity");
            fadeOut->setDuration(500);
            fadeOut->setStartValue(1);
            fadeOut->setEndValue(0);
            fadeOut->start();

            connect(fadeOut, &QPropertyAnimation::finished, this, &QWidget::deleteLater);
        });
    }
};
