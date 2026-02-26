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
#include <QCalendarWidget>
#include <QDateTime>
#include <QDate>
#include <QVariant>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QApplication>

class RedeemRecordDialog : public QDialog {
public:
    explicit RedeemRecordDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("兑换记录查询");
        resize(600, 500);
        
        auto *mainLayout = new QVBoxLayout(this);
        auto *topLayout = new QHBoxLayout();
        
        auto *dateButton = new QPushButton("点击选择日期 (查询未兑换)", this);
        auto *clearButton = new QPushButton("清除", this);
        auto *queryButton = new QPushButton("查询", this);
        clearButton->setFixedWidth(60);

        topLayout->addWidget(new QLabel("日期筛选:", this));
        topLayout->addWidget(dateButton, 1);
        topLayout->addWidget(clearButton);
        topLayout->addWidget(queryButton);
        mainLayout->addLayout(topLayout);

        auto *tableWidget = new QTableWidget(this);
        tableWidget->setColumnCount(3);
        tableWidget->setHorizontalHeaderLabels({"兑换码", "兑换人", "兑换时间"});
        tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        mainLayout->addWidget(tableWidget);

        // 使用 QPointer 防止网络回调时窗口已关闭导致的崩溃
        QPointer<RedeemRecordDialog> dialogPointer(this);

        // 复制功能
        connect(tableWidget, &QTableWidget::cellClicked, [tableWidget](int row, int column) {
            if (column > 1 || !tableWidget->item(row, column)) return;
            if (tableWidget->item(row, column)->text().isEmpty()) return;

            qApp->clipboard()->setText(tableWidget->item(row, column)->text());
            QToolTip::showText(QCursor::pos(), "已复制到剪切板");
        });

        // 日期选择：直接将日期存在按钮的 property 中
        connect(dateButton, &QPushButton::clicked, [dateButton, this]() {
            QDialog calendarDialog(this);
            calendarDialog.setWindowTitle("选择日期");
            auto *calendarLayout = new QVBoxLayout(&calendarDialog);
            auto *calendarWidget = new QCalendarWidget(&calendarDialog);
            
            // 读取当前存储的日期，若无则默认今天
            QDate currentDate = dateButton->property("date").toDate();
            calendarWidget->setSelectedDate(currentDate.isValid() ? currentDate : QDate::currentDate());
            
            calendarLayout->addWidget(calendarWidget);
            connect(calendarWidget, &QCalendarWidget::activated, &calendarDialog, &QDialog::accept);
            
            if (calendarDialog.exec() == QDialog::Accepted) {
                dateButton->setProperty("date", calendarWidget->selectedDate());
                dateButton->setText(calendarWidget->selectedDate().toString("yyyy-MM-dd"));
            }
        });

        // 清除逻辑
        connect(clearButton, &QPushButton::clicked, [dateButton]() {
            dateButton->setProperty("date", QVariant()); // 清空 property
            dateButton->setText("兑换日期");
        });

        // 查询逻辑
        auto doQuery = [=]() {
            tableWidget->setRowCount(0); // 清空表格

            QJsonObject params;
            QDate date = dateButton->property("date").toDate();
            if (date.isValid()) params["date"] = date.toString("yyyy-MM-dd");

            webSocketClient->emitEvent("get_redeem_codes", params, [=](const QJsonValue &res) {
                // 如果窗口已被销毁，直接返回，避免崩溃
                if (!dialogPointer) return; 
                
                QJsonArray jsonArray = res.toArray();
                tableWidget->setRowCount(jsonArray.size());

                if (jsonArray.isEmpty()) {
                    QToolTip::showText(QCursor::pos(), "未找到记录");
                    return;
                }
                
                for (int i = 0; i < jsonArray.size(); ++i) {
                    const auto& item = jsonArray[i].toObject();
                    tableWidget->setItem(i, 0, new QTableWidgetItem(item["code"].toString()));
                    tableWidget->setItem(i, 1, new QTableWidgetItem(item["phone"].toString()));
                    QDateTime dateTime = QDateTime::fromString(item["redeemAt"].toString(), Qt::ISODate);
                    tableWidget->setItem(i, 2, new QTableWidgetItem(dateTime.toLocalTime().toString("yyyy-MM-dd HH:mm:ss")));
                }
            });
        };

        connect(queryButton, &QPushButton::clicked, doQuery);
        
        // 界面初始化完毕后自动执行一次查询
        doQuery();
    }
};