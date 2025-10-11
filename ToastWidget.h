#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>

class ToastWidget : public QWidget {
public:
    explicit ToastWidget(const QString &message, QWidget *parent = nullptr) : QWidget(parent) {
        // 设置窗口标志，使其像气泡提示
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        
        // 设置窗口透明背景，圆角，渐变色背景
        setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
                      "stop:0 rgba(0, 0, 0, 180), stop:1 rgba(0, 0, 0, 255));"
                      "border-radius: 12px;");
        
        // 创建标签，并设置字体、颜色和内边距
        QLabel *label = new QLabel(message, this);
        label->setStyleSheet("color: white; font-size: 16px; font-weight: bold; padding: 15px;");

        // 创建布局，将标签添加到布局中
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(label);
        layout->setContentsMargins(20, 15, 20, 15);
        setLayout(layout);

        // 为窗口添加柔和的阴影效果
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(20);
        shadow->setOffset(6, 6);
        shadow->setColor(QColor(0, 0, 0, 180));
        setGraphicsEffect(shadow);

        // 设置动画效果，使 Toast 显得更流畅
        QPropertyAnimation *fadeInAnimation = new QPropertyAnimation(this, "windowOpacity");
        fadeInAnimation->setDuration(500);
        fadeInAnimation->setStartValue(0);
        fadeInAnimation->setEndValue(1);
        fadeInAnimation->start();

        // 显示窗口
        this->show();

        // 2秒后自动关闭该窗口
        QTimer::singleShot(2000, this, &ToastWidget::fadeOutAndDelete);
    }

private slots:
    void fadeOutAndDelete() {
        // 设置消失动画
        QPropertyAnimation *fadeOutAnimation = new QPropertyAnimation(this, "windowOpacity");
        fadeOutAnimation->setDuration(500);
        fadeOutAnimation->setStartValue(1);
        fadeOutAnimation->setEndValue(0);
        fadeOutAnimation->start();

        // 动画结束后删除窗口
        connect(fadeOutAnimation, &QPropertyAnimation::finished, this, &ToastWidget::deleteLater);
    }
};
