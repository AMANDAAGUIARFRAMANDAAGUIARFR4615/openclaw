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
#include <QComboBox>
#include <QCheckBox>
#include <QSplitter>
#include <QToolTip>
#include <QJsonArray>
#include <QJsonObject>
#include <algorithm>

class SwapExpirationDialog : public QDialog {
    Q_OBJECT

    // 自定义Item用于排序：不可用的排在后面，否则按原始索引排序
    class SortTableItem : public QTableWidgetItem {
    public:
        int originalIndex;
        SortTableItem(int index) : QTableWidgetItem(), originalIndex(index) {}
        
        bool operator<(const QTableWidgetItem &other) const override {
            // 状态比较：Enabled(false) < Disabled(true)，从而让Disabled排到底部
            bool myDisabled = !(flags() & Qt::ItemIsEnabled);
            bool otherDisabled = !(other.flags() & Qt::ItemIsEnabled);
            
            if (myDisabled != otherDisabled)
                return myDisabled < otherDisabled;
            
            // 状态相同时，保持原始加载顺序
            return originalIndex < static_cast<const SortTableItem&>(other).originalIndex;
        }
    };

public:
    explicit SwapExpirationDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("互换到期时间");
        setMinimumSize(960, 600);
        auto mainLayout = new QVBoxLayout(this);

        auto splitter = new QSplitter(Qt::Horizontal);
        
        sourceGroupWidget = new QWidget();
        targetGroupWidget = new QWidget();
        
        createPanel(sourceGroupWidget, sourceFilterComboBox, sourceSelectAllCheckBox, sourceDeviceTable, "源设备 (A)");
        createPanel(targetGroupWidget, targetFilterComboBox, targetSelectAllCheckBox, targetDeviceTable, "目标设备 (B)");
        
        splitter->addWidget(sourceGroupWidget);
        splitter->addWidget(targetGroupWidget);
        mainLayout->addWidget(splitter, 1);

        mainLayout->addWidget(new QLabel("提示: 左右设备互斥，不可选设备已置底。"));
        
        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        mainLayout->addWidget(buttonBox);

        // --- 核心联动逻辑绑定 ---
        auto bindInteractionLogic = [&](QTableWidget* currentTable, QTableWidget* oppositeTable, 
                                        QCheckBox* currentSelectAllBox, QCheckBox* oppositeSelectAllBox) {
            
            // 列表项点击 -> 处理互斥 -> 更新两边全选状态
            connect(currentTable, &QTableWidget::itemChanged, [=](QTableWidgetItem *item) {
                // 只处理第一列（复选框列）的变化
                if (item->column() == 0) {
                    syncItemMutexState(item, oppositeTable);
                    updateSelectAllState(oppositeTable, oppositeSelectAllBox); 
                    updateSelectAllState(currentTable, currentSelectAllBox); 
                }
            });

            // 筛选下拉框改变 -> 重新加载表格
            auto filterBox = currentTable->parentWidget()->findChild<QComboBox*>();
            if (filterBox) {
                connect(filterBox, &QComboBox::currentIndexChanged, [=]() {
                    loadTableData(currentTable, filterBox, oppositeTable, currentSelectAllBox);
                });
            }

            // 全选按钮点击
            connect(currentSelectAllBox, &QCheckBox::clicked, [=](bool) {
                Qt::CheckState state = currentSelectAllBox->checkState();
            
                // 用户点击时，如果是半选状态，通常预期是变为全选
                if (state == Qt::PartiallyChecked) {
                    state = Qt::Checked;
                    currentSelectAllBox->setCheckState(state);
                }

                currentTable->blockSignals(true);
                
                for(int i = 0; i < currentTable->rowCount(); ++i) {
                    QTableWidgetItem *item = currentTable->item(i, 0);
                    // 只有启用的项目才会被全选选中
                    if(item->flags() & Qt::ItemIsEnabled) { 
                        item->setCheckState(state);
                        // 循环中不触发排序(false)，避免性能问题
                        syncItemMutexState(item, oppositeTable, false); 
                    }
                }
                currentTable->blockSignals(false);
                // 循环结束后一次性排序对面表格
                oppositeTable->sortItems(0, Qt::AscendingOrder);
                updateSelectAllState(oppositeTable, oppositeSelectAllBox); 
            });
        };

        // 双向绑定逻辑
        bindInteractionLogic(sourceDeviceTable, targetDeviceTable, sourceSelectAllCheckBox, targetSelectAllCheckBox);
        bindInteractionLogic(targetDeviceTable, sourceDeviceTable, targetSelectAllCheckBox, sourceSelectAllCheckBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &SwapExpirationDialog::onConfirm);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        // 初始化数据
        initFilterComboBox(sourceFilterComboBox);
        initFilterComboBox(targetFilterComboBox);
        
        loadTableData(sourceDeviceTable, sourceFilterComboBox, targetDeviceTable, sourceSelectAllCheckBox);
        loadTableData(targetDeviceTable, targetFilterComboBox, sourceDeviceTable, targetSelectAllCheckBox);
    }

private:
    QWidget *sourceGroupWidget, *targetGroupWidget;
    QComboBox *sourceFilterComboBox, *targetFilterComboBox;
    QCheckBox *sourceSelectAllCheckBox, *targetSelectAllCheckBox;
    QTableWidget *sourceDeviceTable, *targetDeviceTable;

    // 创建左右两侧的面板
    void createPanel(QWidget* containerWidget, QComboBox*& filterComboBox, QCheckBox*& selectAllCheckBox, QTableWidget*& tableWidget, const QString& title) {
        auto verticalLayout = new QVBoxLayout(containerWidget);
        verticalLayout->addWidget(new QLabel(title));
        
        auto filterLayout = new QHBoxLayout();
        filterLayout->addWidget(new QLabel("分组:"));
        filterComboBox = new QComboBox();
        filterComboBox->setParent(containerWidget);
        filterLayout->addWidget(filterComboBox);
        filterLayout->addStretch();
        verticalLayout->addLayout(filterLayout);

        selectAllCheckBox = new QCheckBox("全选");
        selectAllCheckBox->setTristate(true);
        verticalLayout->addWidget(selectAllCheckBox);

        tableWidget = new QTableWidget();
        tableWidget->setColumnCount(4);
        tableWidget->setHorizontalHeaderLabels({"", "设备名称", "机型", "到期时间"});
        tableWidget->setFrameShape(QFrame::NoFrame);
        tableWidget->setShowGrid(false);
        tableWidget->setAlternatingRowColors(true);
        tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableWidget->setFocusPolicy(Qt::NoFocus);

        auto headerView = tableWidget->horizontalHeader();
        headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
        headerView->setSectionResizeMode(1, QHeaderView::Stretch);
        headerView->setSectionsClickable(false);

        verticalLayout->addWidget(tableWidget);
    }

    void initFilterComboBox(QComboBox* comboBox) {
        for (const auto& tabInfo : MainWindow::getInstance()->getTabs()) {
            comboBox->addItem(tabInfo.name, tabInfo.bit);
        }
        comboBox->setCurrentIndex(MainWindow::getInstance()->tabWidget->currentIndex());
    }

    // 同步互斥状态：如果在A表选中了设备，则B表对应的设备变为不可选
    // sort: 是否触发排序
    void syncItemMutexState(QTableWidgetItem* sourceItem, QTableWidget* targetTableWidget, bool sort = true) {
        QString udid = sourceItem->data(Qt::UserRole).toString();
        bool isChecked = (sourceItem->checkState() == Qt::Checked);
        
        targetTableWidget->blockSignals(true); 
        for (int i = 0; i < targetTableWidget->rowCount(); ++i) {
            auto targetItem = targetTableWidget->item(i, 0);
            if (targetItem->data(Qt::UserRole).toString() == udid) {
                if (isChecked) {
                    // 源被选中 -> 目标禁用并变灰
                    targetItem->setCheckState(Qt::Unchecked);
                    targetItem->setFlags(targetItem->flags() & ~Qt::ItemIsEnabled);
                    targetTableWidget->item(i, 1)->setForeground(Qt::gray);
                    targetTableWidget->item(i, 3)->setForeground(Qt::gray);
                } else {
                    // 源取消选中 -> 目标恢复可用
                    targetItem->setFlags(targetItem->flags() | Qt::ItemIsEnabled);
                    targetTableWidget->item(i, 1)->setForeground(Qt::black);
                    
                    // 恢复到期时间的颜色逻辑
                    bool isExpired = targetTableWidget->item(i, 3)->data(Qt::UserRole).toBool();
                    targetTableWidget->item(i, 3)->setForeground(isExpired ? QColor("#d32f2f") : Qt::black);
                }
                break;
            }
        }
        if (sort) {
            targetTableWidget->sortItems(0, Qt::AscendingOrder);
        }
        targetTableWidget->blockSignals(false);
    }

    void loadTableData(QTableWidget* tableWidget, QComboBox* filterComboBox, QTableWidget* oppositeTableWidget, QCheckBox* selectAllCheckBox) {
        if (filterComboBox->currentIndex() < 0) return;
        
        auto bitMask = filterComboBox->currentData().toUInt();
        auto devices = DeviceInfo::getDevices(bitMask == 0 ? 0 : (1U << bitMask));
        
        // 获取对面表格已经选中的设备ID
        QSet<QString> lockedDeviceIds;
        for(int i = 0; i < oppositeTableWidget->rowCount(); ++i) {
            if(oppositeTableWidget->item(i, 0)->checkState() == Qt::Checked) {
                lockedDeviceIds.insert(oppositeTableWidget->item(i, 0)->data(Qt::UserRole).toString());
            }
        }

        tableWidget->blockSignals(true);
        tableWidget->setRowCount(0);
        tableWidget->setRowCount(devices.size());

        for (int i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];
            bool isLocked = lockedDeviceIds.contains(device->deviceId);

            // Checkbox Column (使用自定义的 SortTableItem，传入原始索引 i)
            auto itemCheckbox = new SortTableItem(i);
            itemCheckbox->setData(Qt::UserRole, device->deviceId);
            itemCheckbox->setCheckState(Qt::Unchecked);
            if (isLocked) {
                itemCheckbox->setFlags(itemCheckbox->flags() & ~Qt::ItemIsEnabled);
            }
            tableWidget->setItem(i, 0, itemCheckbox);

            // Name Column
            auto itemName = new QTableWidgetItem(device->deviceName);
            if (isLocked) itemName->setForeground(Qt::gray);
            tableWidget->setItem(i, 1, itemName);
            
            // Model Column
            tableWidget->setItem(i, 2, new QTableWidgetItem(device->model));

            // Expiration Column
            auto timeString = QDateTime::fromMSecsSinceEpoch(device->expireAt.get()).toString("yyyy-MM-dd HH:mm:ss");
            auto itemTime = new QTableWidgetItem(timeString);
            bool isExpired = device->expireAt.get() < QDateTime::currentMSecsSinceEpoch();
            itemTime->setData(Qt::UserRole, isExpired); 
            
            if (isLocked)
                itemTime->setForeground(Qt::gray);
            else if (isExpired)
                itemTime->setForeground(QColor("#d32f2f"));
            
            tableWidget->setItem(i, 3, itemTime);
        }
        
        // 加载完成后进行一次排序，确保禁用的项目沉底，其余保持原始顺序
        tableWidget->sortItems(0, Qt::AscendingOrder);
        
        tableWidget->blockSignals(false);
        updateSelectAllState(tableWidget, selectAllCheckBox);
    }

    void updateSelectAllState(QTableWidget* tableWidget, QCheckBox* checkBox) {
        int checkedCount = 0;
        int enabledCount = 0;
        
        for(int i = 0; i < tableWidget->rowCount(); ++i) {
            if(tableWidget->item(i, 0)->flags() & Qt::ItemIsEnabled) {
                enabledCount++;
                if(tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                    checkedCount++;
                }
            }
        }
        
        checkBox->blockSignals(true);
        if (enabledCount > 0 && checkedCount == enabledCount) {
            checkBox->setCheckState(Qt::Checked);
        } else if (checkedCount > 0) {
            checkBox->setCheckState(Qt::PartiallyChecked);
        } else {
            checkBox->setCheckState(Qt::Unchecked);
        }
        checkBox->setEnabled(enabledCount > 0);
        checkBox->blockSignals(false);
    }

    void onConfirm() {
        auto getSelectedIds = [](QTableWidget* tableWidget) {
            QJsonArray idArray;
            for(int i = 0; i < tableWidget->rowCount(); ++i) {
                if(tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                    idArray.append(tableWidget->item(i, 0)->data(Qt::UserRole).toString());
                }
            }
            return idArray;
        };

        QJsonArray sourceIds = getSelectedIds(sourceDeviceTable);
        QJsonArray targetIds = getSelectedIds(targetDeviceTable);

        if (sourceIds.isEmpty() || targetIds.isEmpty() || sourceIds.size() != targetIds.size()) {
            QToolTip::showText(QCursor::pos(), "左右两边选择的设备数量必须一致且不为空");
            return;
        }

        setEnabled(false);
        QJsonObject jsonPayload; 
        jsonPayload["leftIds"] = sourceIds; 
        jsonPayload["rightIds"] = targetIds;

        webSocketClient->emitEvent("swapExpire", jsonPayload, [=](const QJsonValue &response) {
            setEnabled(true);
            if (response.isString()) { 
                QToolTip::showText(QCursor::pos(), response.toString()); 
                return; 
            }

            for (const QJsonValue &jsonValue : response.toArray()) {
                auto deviceInfo = DeviceInfo::getDevice(jsonValue["udid"].toString());
                if(deviceInfo)
                    deviceInfo->expireAt = jsonValue[HIDE("expireAt")].toInteger();
            }
            accept();
        });
    }
};
