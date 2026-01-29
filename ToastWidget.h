#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QApplication>
#include <QScreen>

class ToastWidget : public QWidget {
    Q_OBJECT

public:
    explicit ToastWidget(const QString &message, QWidget *parent = nullptr) : QWidget() {
        if (parent && parent->window()->isMinimized()) {
            deleteLater();
            return;
        }

        setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);

        QLabel *label = new QLabel(message, this);
        
        label->setStyleSheet(R"(
            QLabel {
                color: white;
                font-size: 16px;
                font-weight: bold;
                background-color: rgba(0, 0, 0, 180);
                border-radius: 10px;
                padding: 10px 20px;
            }
        )");

        label->setAlignment(Qt::AlignCenter);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->addWidget(label);

        auto shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(15);
        shadow->setOffset(0, 4);
        shadow->setColor(QColor(0, 0, 0, 100));
        label->setGraphicsEffect(shadow);

        adjustSize();
        centerWidget(parent);

        auto fadeIn = new QPropertyAnimation(this, "windowOpacity");
        fadeIn->setDuration(300);
        fadeIn->setStartValue(0);
        fadeIn->setEndValue(1);
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

        show();

        QTimer::singleShot(2000, this, [this] {
            auto fadeOut = new QPropertyAnimation(this, "windowOpacity");
            fadeOut->setDuration(300);
            fadeOut->setStartValue(1);
            fadeOut->setEndValue(0);
            fadeOut->start(QAbstractAnimation::DeleteWhenStopped);

            connect(fadeOut, &QPropertyAnimation::finished, this, &ToastWidget::deleteLater);
        });
    }

private:
    void centerWidget(QWidget *parent) {
        int x, y;
        if (parent) {
            // 居中于父窗口
            QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
            x = parentCenter.x() - width() / 2;
            y = parentCenter.y() - height() / 2;
        } else {
            // 居中于屏幕
            QRect screenGeom = qApp->primaryScreen()->availableGeometry();
            x = screenGeom.center().x() - width() / 2;
            y = screenGeom.center().y() - height() / 2;
        }
        move(x, y);
    }
};
