#pragma once

#include <QObject>
#include <QListWidget>
#include <QSet>
#include <QList>
#include <QScrollBar>
#include <QEvent>
#include <QTimer>

class ViewportAwareBehavior : public QObject {
    Q_OBJECT
public:
    explicit ViewportAwareBehavior(QListWidget *listWidget) : QObject(listWidget), m_listWidget(listWidget)
    {
        if (m_listWidget) {
            m_calcTimer.setSingleShot(true);
            m_calcTimer.setInterval(50);
            connect(&m_calcTimer, &QTimer::timeout, this, &ViewportAwareBehavior::doCalculateVisibility);

            connect(m_listWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, &ViewportAwareBehavior::requestUpdate);
            connect(m_listWidget->horizontalScrollBar(), &QScrollBar::valueChanged, this, &ViewportAwareBehavior::requestUpdate);
            
            m_listWidget->installEventFilter(this); 
            
            QTimer::singleShot(0, this, &ViewportAwareBehavior::requestUpdate);
        }
    }

signals:
    void viewportItemsChanged(const QList<QListWidgetItem*>& itemsEntered, const QList<QListWidgetItem*>& itemsLeft);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == m_listWidget && event->type() == QEvent::Resize)
            requestUpdate();

        return QObject::eventFilter(watched, event);
    }

public slots:
    // 每次滚动或改变尺寸时调用，重新开始计时
    void requestUpdate() {
        m_calcTimer.start(); 
    }

private slots:
    // 真正执行计算的逻辑
    void doCalculateVisibility() {
        if (!m_listWidget) return;

        QSet<QListWidgetItem*> currentVisibleItems;
        QSet<QListWidgetItem*> allValidItems;
        
        QRect viewportRect = m_listWidget->viewport()->rect();

        for (int i = 0; i < m_listWidget->count(); ++i) {
            auto currentItem = m_listWidget->item(i);
            
            allValidItems.insert(currentItem);

            if (currentItem->isHidden()) continue;

            QRect itemVisualRect = m_listWidget->visualItemRect(currentItem);
            
            if (itemVisualRect.isValid() && viewportRect.intersects(itemVisualRect))
                currentVisibleItems.insert(currentItem);
        }

        // 过滤掉已经被销毁/从列表中移除的 item（求交集，只保留当前依然有效的指针）
        m_lastVisibleItems.intersect(allValidItems);

        QSet<QListWidgetItem*> itemsEntered = currentVisibleItems - m_lastVisibleItems;
        QSet<QListWidgetItem*> itemsLeft = m_lastVisibleItems - currentVisibleItems;

        if (!itemsEntered.isEmpty() || !itemsLeft.isEmpty())
            emit viewportItemsChanged(itemsEntered.values(), itemsLeft.values());

        m_lastVisibleItems = currentVisibleItems;
    }

private:
    QListWidget *m_listWidget;
    QSet<QListWidgetItem*> m_lastVisibleItems;
    QTimer m_calcTimer; // 防抖定时器
};