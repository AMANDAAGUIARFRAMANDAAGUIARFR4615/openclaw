#pragma once

#include "WebSocketClient.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QClipboard>
#include <QToolTip>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QApplication>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QShortcut>
#include <QKeySequence>
#include <QStringList>
#include <algorithm>

class RedeemRecordDialog : public QDialog {
public:
    explicit RedeemRecordDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("兑换记录查询");

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        setWindowState(Qt::WindowMaximized);
#else
        resize(600, 500);
#endif
        
        auto mainLayout = new QVBoxLayout(this);
        
        auto statusComboBox = new QComboBox(this);
        statusComboBox->addItem("未兑换", 0);
        statusComboBox->addItem("已兑换", 1);
        
        auto dateFilterCheck = new QCheckBox("指定日期:", this);
        auto dateEdit = new QDateEdit(QDate::currentDate(), this);
        dateEdit->setCalendarPopup(true); // 开启下拉日历弹窗
        dateEdit->setDisplayFormat("yyyy-MM-dd");
        
        dateFilterCheck->setEnabled(false);
        dateEdit->setEnabled(false);

        auto queryButton = new QPushButton("查询", this);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        // 移动端：为了防止横向空间不足，使用网格布局分两行
        statusComboBox->setMinimumHeight(40);
        dateEdit->setMinimumHeight(40);
        queryButton->setMinimumHeight(40);
        dateFilterCheck->setMinimumHeight(40);

        auto topLayout = new QGridLayout();
        topLayout->addWidget(new QLabel("状态:", this), 0, 0);
        topLayout->addWidget(statusComboBox, 0, 1);
        topLayout->addWidget(queryButton, 0, 2);
        topLayout->addWidget(dateFilterCheck, 1, 0);
        topLayout->addWidget(dateEdit, 1, 1, 1, 2);
        mainLayout->addLayout(topLayout);
#else
        // 桌面端：水平一字排开
        statusComboBox->setFixedWidth(100);
        auto topLayout = new QHBoxLayout();
        topLayout->addWidget(new QLabel("状态:", this));
        topLayout->addWidget(statusComboBox);
        topLayout->addSpacing(15);
        topLayout->addWidget(dateFilterCheck);
        topLayout->addWidget(dateEdit, 1);
        topLayout->addWidget(queryButton);
        mainLayout->addLayout(topLayout);
#endif

        auto tableWidget = new QTableWidget(0, 3, this); // 直接指定 0行3列
        tableWidget->setHorizontalHeaderLabels({"兑换码", "兑换人", "兑换时间"});
        tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        
        // 支持多选，并且是以单元格为基础的选择模式
        tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection); 
        tableWidget->setSelectionBehavior(QAbstractItemView::SelectItems);
        mainLayout->addWidget(tableWidget);

        // 状态切换逻辑
        connect(statusComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
            bool isRedeemed = index == 1;
            dateFilterCheck->setEnabled(isRedeemed);
            if (!isRedeemed) dateFilterCheck->setChecked(false); // 切回"未兑换"强制取消勾选
        });

        connect(dateFilterCheck, &QCheckBox::toggled, dateEdit, &QWidget::setEnabled);

        auto copyShortcut = new QShortcut(QKeySequence::Copy, tableWidget);
        copyShortcut->setContext(Qt::WidgetShortcut); 
        connect(copyShortcut, &QShortcut::activated, [tableWidget]() {
            QList<QTableWidgetItem *> selectedItems = tableWidget->selectedItems();
            if (selectedItems.isEmpty()) return;

            // 按照行号排序，保证复制的顺序是从上到下的
            std::sort(selectedItems.begin(), selectedItems.end(), [](QTableWidgetItem *a, QTableWidgetItem *b) {
                return a->row() < b->row();
            });

            QStringList codes;
            // 因为限制了只能选中第一列，所以直接遍历取出文字即可
            for (auto item : selectedItems) {
                codes << item->text();
            }

            qApp->clipboard()->setText(codes.join("\n"));
            QToolTip::showText(QCursor::pos(), QString("已复制 %1 个兑换码").arg(codes.size()));
        });

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        auto closeButton = new QPushButton("关闭", this);
        closeButton->setMinimumHeight(50);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        mainLayout->addWidget(closeButton);
#endif

        QPointer<RedeemRecordDialog> safeThis(this); // 防止异步回调时窗口已销毁导致崩溃

        auto doQuery = [=]() {
            tableWidget->setRowCount(0); // 清空表格
            
            QJsonObject params;
            int status = statusComboBox->currentData().toInt();
            params["status"] = status; 

            if (status == 1 && dateFilterCheck->isChecked())
                params["date"] = dateEdit->date().toString("yyyy-MM-dd");

            webSocketClient->emitEvent("get_redeem_codes", params, [=](const QJsonValue &res) {
                if (!safeThis) return; 
                
                QJsonArray jsonArray = res.toArray();
                if (jsonArray.isEmpty()) {
                    QToolTip::showText(QCursor::pos(), "未找到记录");
                    return;
                }
                
                tableWidget->setRowCount(jsonArray.size());
                for (int i = 0; i < jsonArray.size(); ++i) {
                    QJsonObject item = jsonArray[i].toObject();
                    
                    // 第1列：兑换码 (可以正常选中)
                    auto codeItem = new QTableWidgetItem(item["code"].toString());
                    tableWidget->setItem(i, 0, codeItem);
                    
                    // 第2列：兑换人 (去除可选中标志)
                    auto phoneItem = new QTableWidgetItem(item["phone"].toString());
                    phoneItem->setFlags(phoneItem->flags() & ~Qt::ItemIsSelectable);
                    tableWidget->setItem(i, 1, phoneItem);
                    
                    // 第3列：兑换时间 (去除可选中标志)
                    QString timeStr;
                    QString rawTime = item["redeemAt"].toString();
                    if (!rawTime.isEmpty()) {
                        QDateTime dt = QDateTime::fromString(rawTime, Qt::ISODate);
                        if (dt.isValid()) timeStr = dt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
                    }
                    auto timeItem = new QTableWidgetItem(timeStr);
                    timeItem->setFlags(timeItem->flags() & ~Qt::ItemIsSelectable);
                    tableWidget->setItem(i, 2, timeItem);
                }
            });
        };

        connect(queryButton, &QPushButton::clicked, doQuery);
        
        // 初始化完毕自动查一次
        doQuery();
    }
};