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
        bool isLeftButton = event->button() == Qt::LeftButton;

        if (item && !isCtrlPressed && isLeftButton) {
            // 设置“当前项”（虚线框），但在第二个参数传入 NoUpdate。
            // 这样 Qt 就只会把焦点移过去，而绝对不会改变该项的选中/变色状态。
            setCurrentItem(item, QItemSelectionModel::NoUpdate);

            emit itemPressed(item);

            // 直接返回，不调用父类的 mousePressEvent。
            // 这样就彻底拦截了 Qt 内部原本的“点击即选中”逻辑。
            return; 
        }

        // 其他情况（按了Ctrl、点了空白处、点了右键），走 Qt 默认逻辑
        // 这样依然保留了 Ctrl+Click 多选、点击空白处取消选择等功能。
        QListWidget::mousePressEvent(event);
    }
};