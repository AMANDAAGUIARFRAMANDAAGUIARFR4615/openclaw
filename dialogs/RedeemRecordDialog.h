#pragma once

#include "WebSocketClient.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
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

class RedeemRecordDialog : public QDialog {
public:
    explicit RedeemRecordDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("兑换记录查询");
        resize(600, 500);
        
        auto mainLayout = new QVBoxLayout(this);
        auto topLayout = new QHBoxLayout();
        
        auto statusComboBox = new QComboBox(this);
        statusComboBox->addItem("未兑换", 0);
        statusComboBox->addItem("已兑换", 1);
        statusComboBox->setFixedWidth(100);

        auto dateFilterCheck = new QCheckBox("指定日期:", this);
        auto dateEdit = new QDateEdit(QDate::currentDate(), this);
        dateEdit->setCalendarPopup(true); // 开启下拉日历弹窗
        dateEdit->setDisplayFormat("yyyy-MM-dd");
        
        dateFilterCheck->setEnabled(false);
        dateEdit->setEnabled(false);

        auto queryButton = new QPushButton("查询", this);

        topLayout->addWidget(new QLabel("状态:", this));
        topLayout->addWidget(statusComboBox);
        topLayout->addSpacing(15);
        topLayout->addWidget(dateFilterCheck);
        topLayout->addWidget(dateEdit, 1);
        topLayout->addWidget(queryButton);
        mainLayout->addLayout(topLayout);

        auto tableWidget = new QTableWidget(0, 3, this); // 直接指定 0行3列
        tableWidget->setHorizontalHeaderLabels({"兑换码", "兑换人", "兑换时间"});
        tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        mainLayout->addWidget(tableWidget);

        connect(statusComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
            bool isRedeemed = index == 1;
            dateFilterCheck->setEnabled(isRedeemed);
            if (!isRedeemed) dateFilterCheck->setChecked(false); // 切回"未兑换"强制取消勾选
        });

        connect(dateFilterCheck, &QCheckBox::toggled, dateEdit, &QWidget::setEnabled);

        connect(tableWidget, &QTableWidget::cellClicked, [tableWidget](int row, int column) {
            auto item = tableWidget->item(row, column);
            if (!item->text().isEmpty()) {
                qApp->clipboard()->setText(item->text());
                QToolTip::showText(QCursor::pos(), "已复制到剪切板");
            }
        });

        QPointer<RedeemRecordDialog> safeThis(this); // 防止异步回调时窗口已销毁导致崩溃

        auto doQuery = [=]() {
            tableWidget->setRowCount(0); // 清空表格
            
            QJsonObject params;
            int status = statusComboBox->currentData().toInt();
            params["status"] = status; 

            // 仅在"已兑换"且"勾选"日期筛选时，附带时间参数
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
                    tableWidget->setItem(i, 0, new QTableWidgetItem(item["code"].toString()));
                    tableWidget->setItem(i, 1, new QTableWidgetItem(item["phone"].toString()));
                    
                    QString timeStr;
                    QString rawTime = item["redeemAt"].toString();
                    if (!rawTime.isEmpty()) {
                        QDateTime dt = QDateTime::fromString(rawTime, Qt::ISODate);
                        if (dt.isValid()) timeStr = dt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
                    }
                    tableWidget->setItem(i, 2, new QTableWidgetItem(timeStr));
                }
            });
        };

        connect(queryButton, &QPushButton::clicked, doQuery);
        
        // 初始化完毕自动查一次
        doQuery();
    }
};