#pragma once

#include <QListWidget>
#include <QMouseEvent>

class ExplicitSelectionListWidget : public QListWidget
{
    Q_OBJECT
public:
    explicit ExplicitSelectionListWidget(QWidget *parent = nullptr) : QListWidget(parent) {}

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QListWidgetItem *item = itemAt(event->pos());

        bool isCtrlPressed = event->modifiers() & Qt::ControlModifier;

        if (item && !isCtrlPressed) {
            // 设置当前焦点项，但不更新选中状态
            setCurrentItem(item, QItemSelectionModel::NoUpdate);
            emit itemPressed(item);
            
            // 拦截事件，阻止默认的“点击即选中”
            return; 
        }

        QListWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        bool isCtrlPressed = event->modifiers() & Qt::ControlModifier;

        // 如果是 左键按住 且 没有按Ctrl
        if (!isCtrlPressed) {
            // 直接忽略该事件，防止触发父类的“拖动选中”或“框选”逻辑
            // 这样鼠标移动就不会改变选中状态了
            return;
        }

        // 其他情况（如按住Ctrl拖动、或者是为了启动拖拽操作），走默认逻辑
        QListWidget::mouseMoveEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Control || event->matches(QKeySequence::SelectAll))
            QListWidget::keyPressEvent(event);
    }
    
    void keyReleaseEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Control || event->key() == Qt::Key_A)
            QListWidget::keyReleaseEvent(event);
    }
};
