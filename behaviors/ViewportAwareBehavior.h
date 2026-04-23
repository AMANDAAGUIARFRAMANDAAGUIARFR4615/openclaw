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
        m_updateTimer.setSingleShot(true);
        connect(&m_updateTimer, &QTimer::timeout, this, &ViewportAwareBehavior::handleUpdateTimeout);

        connect(m_listWidget->verticalScrollBar(), &QScrollBar::valueChanged, this, &ViewportAwareBehavior::delayUpdate);
        connect(m_listWidget->horizontalScrollBar(), &QScrollBar::valueChanged, this, &ViewportAwareBehavior::delayUpdate);
        auto model = m_listWidget->model();
        connect(model, &QAbstractItemModel::dataChanged, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::headerDataChanged, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::layoutAboutToBeChanged, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::layoutChanged, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::rowsAboutToBeInserted, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::rowsInserted, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::columnsAboutToBeInserted, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::columnsInserted, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::columnsAboutToBeRemoved, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::columnsRemoved, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::modelAboutToBeReset, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::modelReset, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::rowsAboutToBeMoved, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::rowsMoved, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::columnsAboutToBeMoved, this, &ViewportAwareBehavior::delayUpdate);
        connect(model, &QAbstractItemModel::columnsMoved, this, &ViewportAwareBehavior::delayUpdate);
        
        m_listWidget->installEventFilter(this);
    }

signals:
    void viewportItemsChanged(const QList<QListWidgetItem*>& itemsEntered, const QList<QListWidgetItem*>& itemsLeft);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched == m_listWidget && event->type() == QEvent::Resize)
            delayUpdate();

        return QObject::eventFilter(watched, event);
    }

public slots:
    void delayUpdate() {
        if (!m_hasPendingDelay) {
            m_delayBaseVisibleItems = m_lastVisibleItems;
            m_hasPendingDelay = true;
        }

        m_updateTimer.start(kUpdateDelayMs);
    }

private slots:
    void handleUpdateTimeout() {
        m_hasPendingDelay = false;
        updateVisibility(m_delayBaseVisibleItems);
        m_delayBaseVisibleItems.clear();
    }

private:
    void updateVisibility(QSet<QListWidgetItem*> previousVisibleItems) {
        m_listWidget->doItemsLayout();

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
        previousVisibleItems.intersect(allValidItems);

        QSet<QListWidgetItem*> itemsEntered = currentVisibleItems - previousVisibleItems;
        QSet<QListWidgetItem*> itemsLeft = previousVisibleItems - currentVisibleItems;

        if (!itemsEntered.isEmpty() || !itemsLeft.isEmpty())
            emit viewportItemsChanged(itemsEntered.values(), itemsLeft.values());

        m_lastVisibleItems = currentVisibleItems;
    }

    QListWidget *m_listWidget;
    QSet<QListWidgetItem*> m_lastVisibleItems;
    QSet<QListWidgetItem*> m_delayBaseVisibleItems;
    QTimer m_updateTimer;
    bool m_hasPendingDelay = false;

    static constexpr int kUpdateDelayMs = 350;
};
