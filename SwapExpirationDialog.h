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
        setWindowTitle("互换到期时间"); // 标题更新
        setMinimumSize(950, 600);

        // 主布局
        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);

        // 创建左右面板的分割器
        auto splitter = new QSplitter(Qt::Horizontal, this);
        splitter->setHandleWidth(1);
        splitter->setChildrenCollapsible(false);

        // 初始化左侧面板
        auto leftWidget = new QWidget();
        createListPanel(leftWidget, leftFilterComboBox, leftSelectAllCheckBox, leftTableWidget, "源设备列表 (A)");
        
        // 初始化右侧面板
        auto rightWidget = new QWidget();
        createListPanel(rightWidget, rightFilterComboBox, rightSelectAllCheckBox, rightTableWidget, "目标设备列表 (B)");

        splitter->addWidget(leftWidget);
        splitter->addWidget(rightWidget);
        
        // 设置左右均分
        splitter->setStretchFactor(0, 1);
        splitter->setStretchFactor(1, 1);

        mainLayout->addWidget(splitter, 1);

        // 底部提示信息
        auto tipsLabel = new QLabel("提示: 请在左右两侧选择数量相同的设备，系统将按顺序互换它们的到期时间。");
        tipsLabel->setStyleSheet("color: #666; font-size: 12px;");
        mainLayout->addWidget(tipsLabel);

        // 底部按钮栏
        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox->button(QDialogButtonBox::Ok)->setText("确认互换");
        buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

        connect(buttonBox, &QDialogButtonBox::accepted, this, &SwapExpirationDialog::onConfirm);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        mainLayout->addWidget(buttonBox);

        // 初始化筛选器数据
        initFilterData(leftFilterComboBox);
        initFilterData(rightFilterComboBox);

        // 初始加载
        loadDeviceTable(leftTableWidget, leftFilterComboBox);
        loadDeviceTable(rightTableWidget, rightFilterComboBox);
    }

protected:

    // 通用面板创建函数
    void createListPanel(QWidget* container, QComboBox*& filterBox, QCheckBox*& selectAllBox, QTableWidget*& table, const QString& title) {
        auto layout = new QVBoxLayout(container);
        layout->setContentsMargins(10, 0, 10, 0);

        auto titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; margin-bottom: 5px; color: #333;");
        layout->addWidget(titleLabel);

        // 筛选栏
        auto filterLayout = new QHBoxLayout();
        auto filterLabel = new QLabel("分组:");
        filterBox = new QComboBox();
        
        filterLayout->addWidget(filterLabel);
        filterLayout->addWidget(filterBox, 1);
        layout->addLayout(filterLayout);

        // 全选栏
        auto selectionLayout = new QHBoxLayout();
        selectAllBox = new QCheckBox("全选");
        selectAllBox->setTristate(true);
        selectionLayout->addWidget(selectAllBox);
        selectionLayout->addStretch();
        layout->addLayout(selectionLayout);

        // 表格
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
        headerView->setSectionResizeMode(1, QHeaderView::Stretch); // 设备名称自适应
        headerView->setSectionsClickable(false);

        table->setStyleSheet(R"(
            QTableWidget { font-size: 13px; border: 1px solid #e0e0e0; alternate-background-color: #f9f9f9; }
            QTableWidget::item { border-bottom: 1px solid #f0f0f0; }
            QTableWidget::item:selected { background-color: #e6f7ff; color: #000000; }
            QHeaderView::section { background-color: #f5f5f5; border: none; border-bottom: 1px solid #d0d0d0; padding: 8px; font-weight: bold; color: #555555; }
        )");

        layout->addWidget(table, 1);

        // 信号连接
        connect(filterBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, &table, &filterBox](int index) {
             if (index >= 0) loadDeviceTable(table, filterBox);
        });

        connect(selectAllBox, &QCheckBox::clicked, [this, &table, &selectAllBox](bool) {
            Qt::CheckState state = selectAllBox->checkState();
            if (state == Qt::PartiallyChecked) {
                state = Qt::Checked;
                selectAllBox->setCheckState(state);
            }

            table->blockSignals(true);
            for (int i = 0; i < table->rowCount(); ++i) {
                auto item = table->item(i, 0);
                if (item) item->setCheckState(state);
            }
            table->blockSignals(false);
        });

        connect(table, &QTableWidget::itemChanged, [this, &table, &selectAllBox](QTableWidgetItem *item){
            if (item->column() == 0) {
                updateSelectAllState(table, selectAllBox);
            }
        });
    }

    void initFilterData(QComboBox* box) {
        box->blockSignals(true);
        box->clear();
        const auto& items = MainWindow::getInstance()->getTabs();
        for (const auto& item : items) {
            box->addItem(item.name, item.bit);
        }
        
        int currentIdx = MainWindow::getInstance()->tabWidget->currentIndex();
        if (currentIdx >= 0 && currentIdx < box->count()) {
            box->setCurrentIndex(currentIdx);
        } else {
             box->setCurrentIndex(-1);
        }
        box->blockSignals(false);
    }

    void loadDeviceTable(QTableWidget* table, QComboBox* filterBox)
    {
        int index = filterBox->currentIndex();
        if (index < 0) return;

        auto bit = filterBox->itemData(index).toUInt();
        auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

        table->blockSignals(true);
        table->clearContents();
        table->setRowCount(0);
        table->setRowCount(devices.size());

        for (int i = 0; i < devices.size(); ++i) {
            const auto &deviceInfo = devices[i];
            auto checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Unchecked); 
            checkItem->setData(Qt::UserRole, deviceInfo->deviceId);

            table->setItem(i, 0, checkItem);
            table->setItem(i, 1, new QTableWidgetItem(deviceInfo->deviceName));
            table->setItem(i, 2, new QTableWidgetItem(deviceInfo->model));

            auto expireAt = QDateTime::fromMSecsSinceEpoch(deviceInfo->expireAt.get()).toString("yyyy-MM-dd HH:mm:ss");
            auto expireItem = new QTableWidgetItem(expireAt);
            table->setItem(i, 3, expireItem);

            if (deviceInfo->expireAt.get() < Account::getInstance()->loginTime.get() + elapsedTimer->elapsed())
                expireItem->setForeground(QBrush(QColor("#d32f2f"))); // 过期显示红色
        }

        table->blockSignals(false);
        if (table == leftTableWidget) updateSelectAllState(leftTableWidget, leftSelectAllCheckBox);
        else updateSelectAllState(rightTableWidget, rightSelectAllCheckBox);
    }

    void updateSelectAllState(QTableWidget* table, QCheckBox* checkBox) {
        int checkedCount = 0;
        int rowCount = table->rowCount();

        for (int i = 0; i < rowCount; ++i) {
            if (table->item(i, 0)->checkState() == Qt::Checked) {
                checkedCount++;
            }
        }

        checkBox->blockSignals(true);
        if (rowCount > 0 && checkedCount == rowCount)
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

        QSet<QString> leftSet(leftIds.begin(), leftIds.end());
        QSet<QString> rightSet(rightIds.begin(), rightIds.end());
        if (!leftSet.intersect(rightSet).isEmpty()) {
             QToolTip::showText(QCursor::pos(), "同一台设备不能同时出现在交换双方中");
             return;
        }

        QJsonObject payload;
        payload["leftIds"] = QJsonArray::fromStringList(leftIds);
        payload["rightIds"] = QJsonArray::fromStringList(rightIds);

        setEnabled(false); 
        setCursor(Qt::WaitCursor);

        // 修改事件名为 swapExpire，更符合类名语义
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