#pragma once

#include <QListWidget>
#include <QScrollBar>
#include <QLayout>
#include <QSet>
#include <QEvent>
#include <QRect>
#include <QAbstractItemModel>

class VideoVisibilityManager : public QObject {
    Q_OBJECT
public:
    explicit VideoVisibilityManager(QListWidget* listWidget, QObject* parent = nullptr) : m_list(listWidget), QObject(parent)
    {
        // connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this, &VideoVisibilityManager::checkVisibility);
        // connect(m_list->model(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &VideoVisibilityManager::onRowsAboutToBeRemoved);

        // m_list->installEventFilter(this);
        // m_list->viewport()->installEventFilter(this);
    }

    // 当列表数据发生增删时，手动调用此函数刷新状态
    void refresh() {
        checkVisibility();
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (event->type() == QEvent::Resize || event->type() == QEvent::ChildPolished)
            checkVisibility();
        
        return QObject::eventFilter(obj, event);
    }

private slots:
    void onRowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) {
        Q_UNUSED(parent);
        
        for (int i = start; i <= end; ++i) {
            QListWidgetItem* item = m_list->item(i);
            if (m_lastVisibleItems.contains(item)) {
                setPlayState(item, false);
                m_lastVisibleItems.remove(item);
            }
        }
    }

private:
    QListWidget* m_list;
    QSet<QListWidgetItem*> m_lastVisibleItems; // 上一次检测时可见的项

    void checkVisibility() {
        QSet<QListWidgetItem*> currentVisibleItems;
        QRect viewRect = m_list->viewport()->rect();
        
        int count = m_list->count();
        for (int i = 0; i < count; ++i) {
            QListWidgetItem* item = m_list->item(i);
            QRect itemRect = m_list->visualItemRect(item);

            // 1. 如果 Item 底部还在视口顶部上方，跳过
            if (itemRect.bottom() < viewRect.top())
                continue;

            // 2. 如果 Item 顶部已经跑出视口底部，后续的 item 肯定也在下面，直接结束循环
            if (itemRect.top() > viewRect.bottom())
                break;

            // 3. 判断相交 (可视)
            if (itemRect.intersects(viewRect))
                currentVisibleItems.insert(item);
        }

        // 暂停之前可见但现在不可见的
        for (QListWidgetItem* item : m_lastVisibleItems) {
            if (!currentVisibleItems.contains(item))
                setPlayState(item, false);
        }

        // 播放之前不可见但现在可见的
        for (QListWidgetItem* item : currentVisibleItems) {
            if (!m_lastVisibleItems.contains(item))
                setPlayState(item, true);
        }

        m_lastVisibleItems = currentVisibleItems;
    }

    void setPlayState(QListWidgetItem* item, bool play) {
        QWidget* frame = m_list->itemWidget(item);
        if (!frame || !frame->layout() || frame->layout()->count() < 1) return;

        QLayoutItem* layoutItem = frame->layout()->itemAt(0);
        if (QWidget* player = layoutItem->widget()) {
            const char* method = play ? "play" : "pause";
            QMetaObject::invokeMethod(player, method);
        }
    }
};