#pragma once

#include "MainWindow.h"
#include "Account.h"
#include "Safe.h"
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSplitter>
#include <QToolTip>

class SwapExpirationDialog : public QDialog {
    Q_OBJECT

public:
    explicit SwapExpirationDialog(QWidget *parent) : QDialog(parent)
    {
        setModal(true);
        setWindowTitle("互换到期时间");
        setMinimumSize(950, 600);

        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);

        auto splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setHandleWidth(1);
        splitter->setChildrenCollapsible(false);

        // 1. 先创建两个 Widget 容器
        auto leftWidget = new QWidget();
        auto rightWidget = new QWidget();

        // 2. 创建面板 (此时 TableWidget 会被 new 出来)
        createListPanel(leftWidget, leftFilterComboBox, leftSelectAllCheckBox, leftTableWidget, "源设备列表 (A)");
        createListPanel(rightWidget, rightFilterComboBox, rightSelectAllCheckBox, rightTableWidget, "目标设备列表 (B)");

        splitter->addWidget(leftWidget);
        splitter->addWidget(rightWidget);
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 1);

        mainLayout->addWidget(splitter, 1);

        auto tipsLabel = new QLabel("提示: 左右两侧选择的设备数量必须一致。同一台设备不能同时在两侧被选中。");
        tipsLabel->setStyleSheet("color: #666; font-size: 12px;");
        mainLayout->addWidget(tipsLabel);

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox->button(QDialogButtonBox::Ok)->setText("确认互换");
        buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

        connect(buttonBox, &QDialogButtonBox::accepted, this, &SwapExpirationDialog::onConfirm);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        mainLayout->addWidget(buttonBox);

        // 3. 建立互斥连接 (Cross-Linking)
        // 当左侧列表项变化时，尝试锁定/解锁右侧对应项
        connect(leftTableWidget, &QTableWidget::itemChanged, [this](QTableWidgetItem *item){
            if (item->column() == 0) {
                handleItemCheckChange(item, rightTableWidget);
                updateSelectAllState(leftTableWidget, leftSelectAllCheckBox);
            }
        });

        // 当右侧列表项变化时，尝试锁定/解锁左侧对应项
        connect(rightTableWidget, &QTableWidget::itemChanged, [this](QTableWidgetItem *item){
            if (item->column() == 0) {
                handleItemCheckChange(item, leftTableWidget);
                updateSelectAllState(rightTableWidget, rightSelectAllCheckBox);
            }
        });

        // 4. 初始化筛选器和数据
        initFilterData(leftFilterComboBox);
        initFilterData(rightFilterComboBox);

        // 初始加载 (传入另一侧的Table作为参考)
        loadDeviceTable(leftTableWidget, leftFilterComboBox, rightTableWidget);
        loadDeviceTable(rightTableWidget, rightFilterComboBox, leftTableWidget);
    }

protected:

    void createListPanel(QWidget* container, QComboBox*& filterBox, QCheckBox*& selectAllBox, QTableWidget*& table, const QString& title) {
        auto layout = new QVBoxLayout(container);
        layout->setContentsMargins(10, 0, 10, 0);

        auto titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; margin-bottom: 5px; color: #333;");
        layout->addWidget(titleLabel);

        auto filterLayout = new QHBoxLayout();
        filterBox = new QComboBox();
        filterLayout->addWidget(new QLabel("分组:"));
        filterLayout->addWidget(filterBox, 1);
        layout->addLayout(filterLayout);

        auto selectionLayout = new QHBoxLayout();
        selectAllBox = new QCheckBox("全选");
        selectAllBox->setTristate(true);
        selectionLayout->addWidget(selectAllBox);
        selectionLayout->addStretch();
        layout->addLayout(selectionLayout);

        table = new QTableWidget(this);
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({"", "设备名称", "机型", "到期时间"});
        table->setFrameShape(QFrame::NoFrame);
        table->setShowGrid(false);
        table->setAlternatingRowColors(true);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setFocusPolicy(Qt::NoFocus);

        auto headerView = table->horizontalHeader();
        headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
        headerView->setSectionResizeMode(1, QHeaderView::Stretch);
        headerView->setSectionsClickable(false);

        table->setStyleSheet(R"(
            QTableWidget { font-size: 13px; border: 1px solid #e0e0e0; alternate-background-color: #f9f9f9; }
            QTableWidget::item { border-bottom: 1px solid #f0f0f0; }
            QTableWidget::item:selected { background-color: #e6f7ff; color: #000000; }
            QTableWidget::item:disabled { color: #cccccc; background-color: #f0f0f0; }
            QHeaderView::section { background-color: #f5f5f5; border: none; border-bottom: 1px solid #d0d0d0; padding: 8px; font-weight: bold; color: #555555; }
        )");

        layout->addWidget(table, 1);

        // 筛选器变化重新加载
        connect(filterBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, &table, &filterBox](int index) {
             if (index >= 0) {
                 // 重新加载时，需要根据"另一个表"的状态来禁用当前表的某些行
                 QTableWidget* other = (table == leftTableWidget) ? rightTableWidget : leftTableWidget;
                 loadDeviceTable(table, filterBox, other);
             }
        });

        // 全选按钮逻辑
        connect(selectAllBox, &QCheckBox::clicked, [this, &table, &selectAllBox](bool) {
            Qt::CheckState state = selectAllBox->checkState();
            if (state == Qt::PartiallyChecked) state = Qt::Checked;
            selectAllBox->setCheckState(state);

            table->blockSignals(true); // 暂时屏蔽信号，防止触发 handleItemCheckChange 的递归
            for (int i = 0; i < table->rowCount(); ++i) {
                auto item = table->item(i, 0);
                // 关键修改：只有 Enabled 的项才允许被全选/取消
                if (item && (item->flags() & Qt::ItemIsEnabled)) {
                    item->setCheckState(state);
                    // 手动调用逻辑处理对侧锁定 (因为屏蔽了信号)
                    QTableWidget* other = (table == leftTableWidget) ? rightTableWidget : leftTableWidget;
                    handleItemCheckChange(item, other); 
                }
            }
            table->blockSignals(false);
        });
    }

    // 处理互斥逻辑：当 sourceItem 状态改变时，去 targetTable 找对应设备并禁用/启用
    void handleItemCheckChange(QTableWidgetItem* sourceItem, QTableWidget* targetTable) {
        if (!sourceItem || !targetTable) return;

        QString udid = sourceItem->data(Qt::UserRole).toString();
        bool isChecked = (sourceItem->checkState() == Qt::Checked);

        // 在目标表中查找该设备
        // 注意：只能查找到当前目标表中可见（已加载）的设备
        for (int i = 0; i < targetTable->rowCount(); ++i) {
            auto targetItem = targetTable->item(i, 0);
            if (targetItem && targetItem->data(Qt::UserRole).toString() == udid) {
                targetTable->blockSignals(true); // 防止递归触发 itemChanged
                
                if (isChecked) {
                    // 如果源被选中，目标变灰且不可选
                    targetItem->setFlags(targetItem->flags() & ~Qt::ItemIsEnabled);
                    targetItem->setCheckState(Qt::Unchecked); // 强制取消勾选
                    // 视觉置灰
                    targetTable->item(i, 1)->setForeground(Qt::gray);
                    targetTable->item(i, 2)->setForeground(Qt::gray);
                } else {
                    // 如果源取消选中，目标恢复可选
                    targetItem->setFlags(targetItem->flags() | Qt::ItemIsEnabled);
                    // 视觉恢复
                    targetTable->item(i, 1)->setForeground(Qt::black);
                    targetTable->item(i, 2)->setForeground(Qt::black);
                }
                
                targetTable->blockSignals(false);
                break; // 找到即停止
            }
        }
    }

    void initFilterData(QComboBox* box) {
        box->blockSignals(true);
        box->clear();
        const auto& items = MainWindow::getInstance()->getTabs();
        for (const auto& item : items) {
            box->addItem(item.name, item.bit);
        }
        int currentIdx = MainWindow::getInstance()->tabWidget->currentIndex();
        box->setCurrentIndex((currentIdx >= 0 && currentIdx < box->count()) ? currentIdx : -1);
        box->blockSignals(false);
    }

    // 加载列表
    void loadDeviceTable(QTableWidget* table, QComboBox* filterBox, QTableWidget* otherTable)
    {
        int index = filterBox->currentIndex();
        if (index < 0) return;

        auto bit = filterBox->itemData(index).toUInt();
        auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

        // 1. 获取"另一侧"目前已选中设备的ID集合
        QSet<QString> otherSelectedIds;
        if (otherTable) {
            for (int i = 0; i < otherTable->rowCount(); ++i) {
                if (otherTable->item(i, 0)->checkState() == Qt::Checked) {
                    otherSelectedIds.insert(otherTable->item(i, 0)->data(Qt::UserRole).toString());
                }
            }
        }

        table->blockSignals(true);
        table->clearContents();
        table->setRowCount(0);
        table->setRowCount(devices.size());

        for (int i = 0; i < devices.size(); ++i) {
            const auto &deviceInfo = devices[i];
            auto checkItem = new QTableWidgetItem();
            checkItem->setData(Qt::UserRole, deviceInfo->deviceId);

            // 2. 检查该设备是否在另一侧被选中
            if (otherSelectedIds.contains(deviceInfo->deviceId)) {
                checkItem->setCheckState(Qt::Unchecked);
                checkItem->setFlags(checkItem->flags() & ~Qt::ItemIsEnabled); // 禁用
            } else {
                checkItem->setCheckState(Qt::Unchecked);
                checkItem->setFlags(checkItem->flags() | Qt::ItemIsEnabled); // 启用
            }

            table->setItem(i, 0, checkItem);

            auto nameItem = new QTableWidgetItem(deviceInfo->deviceName);
            auto modelItem = new QTableWidgetItem(deviceInfo->model);
            
            // 如果被禁用，文字颜色也置灰
            if (!checkItem->flags().testFlag(Qt::ItemIsEnabled)) {
                nameItem->setForeground(Qt::gray);
                modelItem->setForeground(Qt::gray);
            }

            table->setItem(i, 1, nameItem);
            table->setItem(i, 2, modelItem);

            auto expireAt = QDateTime::fromMSecsSinceEpoch(deviceInfo->expireAt.get()).toString("yyyy-MM-dd HH:mm:ss");
            auto expireItem = new QTableWidgetItem(expireAt);
            table->setItem(i, 3, expireItem);

            if (deviceInfo->expireAt.get() < Account::getInstance()->loginTime.get() + elapsedTimer->elapsed())
                expireItem->setForeground(QBrush(QColor("#d32f2f")));
        }

        table->blockSignals(false);
        
        // 更新全选框状态
        if (table == leftTableWidget) updateSelectAllState(leftTableWidget, leftSelectAllCheckBox);
        else updateSelectAllState(rightTableWidget, rightSelectAllCheckBox);
    }

    void updateSelectAllState(QTableWidget* table, QCheckBox* checkBox) {
        int checkedCount = 0;
        int selectableCount = 0; // 只统计可选的行
        int rowCount = table->rowCount();

        for (int i = 0; i < rowCount; ++i) {
            auto item = table->item(i, 0);
            if (item->flags() & Qt::ItemIsEnabled) {
                selectableCount++;
                if (item->checkState() == Qt::Checked) {
                    checkedCount++;
                }
            }
        }

        checkBox->blockSignals(true);
        if (selectableCount > 0 && checkedCount == selectableCount)
            checkBox->setCheckState(Qt::Checked);
        else if (checkedCount == 0)
            checkBox->setCheckState(Qt::Unchecked);
        else
            checkBox->setCheckState(Qt::PartiallyChecked);
        checkBox->blockSignals(false);
    }

    QList<QString> getSelectedDeviceIds(QTableWidget* table) const {
        QList<QString> result;
        for (int i = 0; i < table->rowCount(); ++i) {
            auto item = table->item(i, 0);
            if (item && item->checkState() == Qt::Checked) {
                result.append(item->data(Qt::UserRole).toString());
            }
        }
        return result;
    }

    void onConfirm() {
        QList<QString> leftIds = getSelectedDeviceIds(leftTableWidget);
        QList<QString> rightIds = getSelectedDeviceIds(rightTableWidget);

        if (leftIds.isEmpty() || rightIds.isEmpty()) {
            QToolTip::showText(QCursor::pos(), "请在左右两边至少各选择一台设备");
            return;
        }

        if (leftIds.size() != rightIds.size()) {
            QToolTip::showText(QCursor::pos(), QString("左右选择数量必须一致 (A: %1, B: %2)").arg(leftIds.size()).arg(rightIds.size()));
            return;
        }

        QJsonObject payload;
        payload["leftIds"] = QJsonArray::fromStringList(leftIds);
        payload["rightIds"] = QJsonArray::fromStringList(rightIds);

        setEnabled(false); 
        setCursor(Qt::WaitCursor);

        webSocketClient->emitEvent("swapExpire", payload, [=](const QJsonValue &res) {
            setEnabled(true);
            unsetCursor();

            if (res.isString()) {
                QToolTip::showText(QCursor::pos(), res.toString());
                return;
            }

            const auto& devices = res["devices"].toArray();
            for (const QJsonValue &item : devices) {
                auto deviceInfo = DeviceInfo::getDevice(item["udid"].toString());
                if (deviceInfo) {
                    deviceInfo->expireAt = item[HIDE("expireAt")].toInteger();
                }
            }
            
            QToolTip::showText(QCursor::pos(), "互换成功");
            accept();
        });
    }

private:
    QTableWidget *leftTableWidget;
    QComboBox *leftFilterComboBox; 
    QCheckBox *leftSelectAllCheckBox;

    QTableWidget *rightTableWidget;
    QComboBox *rightFilterComboBox; 
    QCheckBox *rightSelectAllCheckBox;
};