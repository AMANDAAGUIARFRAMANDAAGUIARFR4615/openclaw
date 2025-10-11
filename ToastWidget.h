#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include <QGraphicsDropShadowEffect>

class ToastWidget : public QWidget {
public:
    explicit ToastWidget(const QString &message, QWidget *parent = nullptr) : QWidget(parent) {
        // 设置窗口标志，使其像气泡提示
        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
        
        // 设置背景颜色和透明度
        setStyleSheet("background-color: rgba(0, 0, 0, 180);"
                      "border-radius: 10px;");
        
        // 创建显示信息的标签
        QLabel *label = new QLabel(message, this);
        label->setStyleSheet("color: white; font-size: 14px; font-weight: bold; padding: 10px;");
        
        // 创建布局，并将标签添加到布局中
        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(label);
        layout->setContentsMargins(15, 10, 15, 10);
        setLayout(layout);
        
        // 添加阴影效果，让窗口看起来更精致
        QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(12);
        shadow->setOffset(4, 4);
        shadow->setColor(QColor(0, 0, 0, 160));
        setGraphicsEffect(shadow);

        // 显示窗口
        this->show();

        // 2秒后自动关闭该窗口
        QTimer::singleShot(2000, this, &ToastWidget::deleteLater);
    }
};
