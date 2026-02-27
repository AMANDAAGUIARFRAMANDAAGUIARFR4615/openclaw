#pragma once

#include "WebSocketClient.h"
#include "BaseDialog.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QClipboard>
#include <QToolTip>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QApplication>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDateEdit>
#include <QStringList>
#include <QFrame>
#include <QTimer>
#include <QTableWidget>
#include <QHeaderView>
#include <QShortcut>
#include <QKeySequence>
#include <QStyleHints>
#include <algorithm>

class RedeemRecordDialog : public BaseDialog {
public:
    explicit RedeemRecordDialog(QWidget *parent = nullptr) : BaseDialog("兑换码记录", parent) {
        setupUI();
        setupStyle();
        setupConnections();
        
        // 初始化完毕自动查一次
        doQuery();
    }

private:
    QButtonGroup *statusButtonGroup;
    QRadioButton *unredeemedRadio;
    QRadioButton *redeemedRadio;
    
    QCheckBox *dateFilterCheck;
    QDateEdit *dateEdit;
    QPushButton *queryButton;
    
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
    QWidget *cardContainer;
    QVBoxLayout *cardLayout;
#else
    QTableWidget *tableWidget;
#endif

    void setupUI() {
        auto mainLayout = contentLayout();

        auto topWidget = new QWidget(this);
        topWidget->setObjectName("TopWidget");
        
        statusButtonGroup = new QButtonGroup(topWidget);
        unredeemedRadio = new QRadioButton("未兑换", topWidget);
        redeemedRadio = new QRadioButton("已兑换", topWidget);
        statusButtonGroup->addButton(unredeemedRadio, 0);
        statusButtonGroup->addButton(redeemedRadio, 1);
        unredeemedRadio->setChecked(true);
        
        dateFilterCheck = new QCheckBox("指定日期:", topWidget);
        dateEdit = new QDateEdit(QDate::currentDate(), topWidget);
        dateEdit->setCalendarPopup(true);
        dateEdit->setDisplayFormat("yyyy-MM-dd");
        
        dateFilterCheck->setEnabled(false);
        dateEdit->setEnabled(false);

        queryButton = new QPushButton("查询", topWidget);
        queryButton->setObjectName("QueryButton");

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        // --- 移动端顶部布局 ---
        dateEdit->setMinimumHeight(40);
        queryButton->setMinimumHeight(40);
        queryButton->setMinimumWidth(80);
        dateFilterCheck->setMinimumHeight(40);
        
        auto topLayout = new QVBoxLayout(topWidget);
        topLayout->setContentsMargins(15, 15, 15, 15);
        topLayout->setSpacing(12);

        auto row1Layout = new QHBoxLayout();
        row1Layout->setContentsMargins(0, 0, 0, 0);
        row1Layout->addWidget(unredeemedRadio);
        row1Layout->addSpacing(15);
        row1Layout->addWidget(redeemedRadio);
        row1Layout->addStretch();
        row1Layout->addWidget(queryButton);

        auto row2Layout = new QHBoxLayout();
        row2Layout->setContentsMargins(0, 0, 0, 0);
        row2Layout->addWidget(dateFilterCheck);
        row2Layout->addWidget(dateEdit);
        row2Layout->setStretchFactor(dateEdit, 1);

        topLayout->addLayout(row1Layout);
        topLayout->addLayout(row2Layout);
#else
        // --- PC端顶部布局 ---
        dateEdit->setFixedWidth(130);
        queryButton->setFixedWidth(80);
        queryButton->setMinimumHeight(32);
        
        auto topLayout = new QHBoxLayout(topWidget);
        topLayout->setContentsMargins(20, 15, 20, 15);
        
        topLayout->addWidget(unredeemedRadio);
        topLayout->addSpacing(10);
        topLayout->addWidget(redeemedRadio);
        topLayout->addSpacing(30);
        topLayout->addWidget(dateFilterCheck);
        topLayout->addWidget(dateEdit);
        topLayout->addStretch();
        topLayout->addWidget(queryButton);
#endif
        mainLayout->addWidget(topWidget);

        // ================= 数据展示区域 (根据平台差异化) =================
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        // --- 移动端：普通卡片列表容器 ---
        cardContainer = new QWidget(this);
        cardContainer->setObjectName("CardContainer");
        
        cardLayout = new QVBoxLayout(cardContainer);
        cardLayout->setContentsMargins(15, 15, 15, 15);
        cardLayout->setSpacing(12);
        cardLayout->addStretch(); // 底部弹簧，防止卡片分散

        mainLayout->addWidget(cardContainer);
#else
        // --- PC端：现代化表格 ---
        tableWidget = new QTableWidget(0, 3, this);
        tableWidget->setHorizontalHeaderLabels({"兑换码", "兑换人", "兑换时间"});
        tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); 
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers); 
        tableWidget->verticalHeader()->setVisible(false); 
        tableWidget->setShowGrid(false); 
        tableWidget->setFrameShape(QFrame::NoFrame); 
        tableWidget->verticalHeader()->setDefaultSectionSize(45); 
        tableWidget->setSelectionMode(QAbstractItemView::ExtendedSelection); 
        tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        
        // 加入边距防止表格贴边
        auto tableMargins = new QHBoxLayout();
        tableMargins->setContentsMargins(15, 15, 15, 15);
        tableMargins->addWidget(tableWidget);
        mainLayout->addLayout(tableMargins);

        // PC端快捷键：批量复制
        auto copyShortcut = new QShortcut(QKeySequence::Copy, tableWidget);
        copyShortcut->setContext(Qt::WidgetShortcut); 
        connect(copyShortcut, &QShortcut::activated, [this]() {
            QList<QTableWidgetItem *> selectedItems = tableWidget->selectedItems();
            if (selectedItems.isEmpty()) return;

            QStringList codes;
            QList<int> rows;
            for (auto item : selectedItems) {
                if (!rows.contains(item->row())) {
                    rows.append(item->row());
                    codes << tableWidget->item(item->row(), 0)->text();
                }
            }

            qApp->clipboard()->setText(codes.join("\n"));
            QToolTip::showText(QCursor::pos(), QString("已复制 %1 个兑换码").arg(codes.size()));
        });
#endif
    }

    void setupStyle() {
        bool isDarkMode = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        QString qss;

        if (isDarkMode) {
            qss = R"(
                RedeemRecordDialog { background-color: #121212; }
                #TopWidget { background-color: #1E1E1E; border-bottom: 1px solid #2D2D30; }
                #CardContainer { background-color: #121212; }

                QDateEdit {
                    border: 1px solid #3E3E42; border-radius: 4px; padding: 4px 10px;
                    background-color: #252526; font-size: 13px; color: #D4D4D4;
                }
                QDateEdit:disabled { background-color: #1E1E1E; color: #666666; }
                QCheckBox { color: #CCCCCC; }
                QRadioButton { color: #E0E0E0; font-size: 14px; spacing: 8px; }
                QRadioButton::indicator { width: 18px; height: 18px; }

                #QueryButton {
                    background-color: #0E639C; color: #FFFFFF; border: none;
                    border-radius: 4px; font-size: 14px; font-weight: bold; padding: 0 15px; 
                }
                #QueryButton:pressed { background-color: #1177BB; }

                /* --- 移动端卡片样式 --- */
                #CardFrame { background-color: #1E1E1E; border-radius: 8px; border: 1px solid #2D2D30; }
                .CodeLabel { font-size: 16px; font-weight: bold; color: #E0E0E0; font-family: monospace; }
                .InfoLabel { font-size: 13px; color: #888888; }
                .CopyButton {
                    background-color: #252526; color: #4DAAF1; border: 1px solid #3E3E42;
                    border-radius: 12px; padding: 4px 12px; font-size: 12px; font-weight: bold;
                }
                .CopyButton:pressed { background-color: #333333; }

                /* --- PC端表格现代化样式 --- */
                QTableWidget {
                    background-color: #1E1E1E; border-radius: 6px;
                    border: 1px solid #2D2D30; outline: none;
                }
                QTableWidget::item {
                    border-bottom: 1px solid #2D2D30; padding: 0px 10px; color: #CCCCCC; font-size: 13px;
                }
                QTableWidget::item:selected { background-color: #094771; color: #FFFFFF; }
                QHeaderView::section {
                    background-color: #252526; color: #AAAAAA; font-weight: bold; font-size: 13px;
                    border: none; border-bottom: 2px solid #2D2D30; padding: 8px 10px; text-align: left;
                }
            )";
        } else {
            qss = R"(
                RedeemRecordDialog { background-color: #F4F5F7; }
                #TopWidget { background-color: #FFFFFF; border-bottom: 1px solid #E4E7ED; }
                #CardContainer { background-color: #F4F5F7; }

                QDateEdit {
                    border: 1px solid #DCDFE6; border-radius: 4px; padding: 4px 10px;
                    background-color: #FFFFFF; font-size: 13px; color: #303133;
                }
                QDateEdit:disabled { background-color: #F2F6FC; color: #C0C4CC; }
                QCheckBox { color: #606266; }
                QRadioButton { color: #303133; font-size: 14px; spacing: 8px; }
                QRadioButton::indicator { width: 18px; height: 18px; }

                #QueryButton {
                    background-color: #409EFF; color: white; border: none;
                    border-radius: 4px; font-size: 14px; font-weight: bold; padding: 0 15px; 
                }
                #QueryButton:pressed { background-color: #3a8ee6; }

                /* --- 移动端卡片样式 --- */
                #CardFrame { background-color: #FFFFFF; border-radius: 8px; border: 1px solid #EBEEF5; }
                .CodeLabel { font-size: 16px; font-weight: bold; color: #303133; font-family: monospace; }
                .InfoLabel { font-size: 13px; color: #909399; }
                .CopyButton {
                    background-color: #F2F6FC; color: #409EFF; border: 1px solid #DCDFE6;
                    border-radius: 12px; padding: 4px 12px; font-size: 12px; font-weight: bold;
                }
                .CopyButton:pressed { background-color: #E4E7ED; }

                /* --- PC端表格现代化样式 --- */
                QTableWidget {
                    background-color: #FFFFFF; border-radius: 6px;
                    border: 1px solid #E4E7ED; outline: none;
                }
                QTableWidget::item {
                    border-bottom: 1px solid #EBEEF5; padding: 0px 10px; color: #606266; font-size: 13px;
                }
                QTableWidget::item:selected { background-color: #F0F7FF; color: #409EFF; }
                QHeaderView::section {
                    background-color: #FAFAFA; color: #909399; font-weight: bold; font-size: 13px;
                    border: none; border-bottom: 2px solid #EBEEF5; padding: 8px 10px; text-align: left;
                }
            )";
        }
        
        this->setStyleSheet(qss);
    }

    void setupConnections() {
        connect(redeemedRadio, &QRadioButton::toggled, [=](bool isRedeemed) {
            dateFilterCheck->setEnabled(isRedeemed);
            if (!isRedeemed) dateFilterCheck->setChecked(false);
            
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
            tableWidget->setColumnHidden(1, !isRedeemed);
            tableWidget->setColumnHidden(2, !isRedeemed);
#endif
        });

        connect(dateFilterCheck, &QCheckBox::toggled, dateEdit, &QWidget::setEnabled);
        connect(queryButton, &QPushButton::clicked, this, &RedeemRecordDialog::doQuery);
    }

    void doQuery() {
        QJsonObject params;
        int status = statusButtonGroup->checkedId();
        params["status"] = status; 

        if (status == 1 && dateFilterCheck->isChecked()) {
            params["date"] = dateEdit->date().toString("yyyy-MM-dd");
        }

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        QLayoutItem *child;
        while ((child = cardLayout->takeAt(0)) != nullptr) {
            if (child->widget()) child->widget()->deleteLater();
            delete child;
        }
        cardLayout->addStretch();
#else
        tableWidget->setRowCount(0);
#endif

        QPointer<RedeemRecordDialog> safeThis(this);
        webSocketClient->emitEvent("get_redeem_codes", params, [=](const QJsonValue &res) {
            if (!safeThis) return; 
            
            QJsonArray jsonArray = res.toArray();
            
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
            if (jsonArray.isEmpty()) {
                auto emptyLabel = new QLabel("暂无兑换码记录", cardContainer);
                emptyLabel->setAlignment(Qt::AlignCenter);
                bool isDark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
                QString emptyColor = isDark ? "#666666" : "#C0C4CC";
                emptyLabel->setStyleSheet(QString("color: %1; font-size: 14px; margin-top: 50px;").arg(emptyColor));
                
                cardLayout->insertWidget(cardLayout->count() - 1, emptyLabel);
                return;
            }
            
            for (int i = 0; i < jsonArray.size(); ++i) {
                QJsonObject item = jsonArray[i].toObject();
                QString codeStr = item["code"].toString();
                
                QFrame *card = new QFrame(cardContainer);
                card->setObjectName("CardFrame");
                auto cardInnerLayout = new QVBoxLayout(card);
                cardInnerLayout->setContentsMargins(15, 15, 15, 15);
                cardInnerLayout->setSpacing(8);

                auto topLayout = new QHBoxLayout();
                auto codeLabel = new QLabel(codeStr, card);
                codeLabel->setProperty("class", "CodeLabel");
                codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
                
                auto copyBtn = new QPushButton("复制", card);
                copyBtn->setProperty("class", "CopyButton");
                copyBtn->setCursor(Qt::PointingHandCursor);
                connect(copyBtn, &QPushButton::clicked, [=]() {
                    qApp->clipboard()->setText(codeStr);
                    copyBtn->setText("已复制 ✔");
                    
                    bool isDark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
                    if (isDark) {
                        copyBtn->setStyleSheet("background-color: #1A3315; color: #67C23A; border-color: #67C23A;");
                    } else {
                        copyBtn->setStyleSheet("background-color: #E1F3D8; color: #67C23A; border-color: #67C23A;");
                    }

                    QTimer::singleShot(1500, copyBtn, [=](){
                        copyBtn->setText("复制");
                        copyBtn->setStyleSheet("");
                    });
                });

                topLayout->addWidget(codeLabel);
                topLayout->addStretch();
                topLayout->addWidget(copyBtn);
                cardInnerLayout->addLayout(topLayout);

                if (status == 1) { 
                    QString phoneStr = item["phone"].toString();
                    if(!phoneStr.isEmpty()) {
                        auto phoneLabel = new QLabel(QString("兑换人: %1").arg(phoneStr), card);
                        phoneLabel->setProperty("class", "InfoLabel");
                        cardInnerLayout->addWidget(phoneLabel);
                    }
                    QString rawTime = item["redeemAt"].toString();
                    if (!rawTime.isEmpty()) {
                        QDateTime dt = QDateTime::fromString(rawTime, Qt::ISODate);
                        if (dt.isValid()) {
                            auto timeLabel = new QLabel(QString("时  间: %1").arg(dt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss")), card);
                            timeLabel->setProperty("class", "InfoLabel");
                            cardInnerLayout->addWidget(timeLabel);
                        }
                    }
                }
                cardLayout->insertWidget(cardLayout->count() - 1, card);
            }
#else
            if (jsonArray.isEmpty()) return;
            tableWidget->setRowCount(jsonArray.size());
            
            for (int i = 0; i < jsonArray.size(); ++i) {
                QJsonObject item = jsonArray[i].toObject();
                
                auto codeItem = new QTableWidgetItem(item["code"].toString());
                QFont f = codeItem->font(); f.setFamily("Consolas"); codeItem->setFont(f);
                tableWidget->setItem(i, 0, codeItem);
                
                auto phoneItem = new QTableWidgetItem(item["phone"].toString());
                tableWidget->setItem(i, 1, phoneItem);
                
                QString timeStr;
                QString rawTime = item["redeemAt"].toString();
                if (!rawTime.isEmpty()) {
                    QDateTime dt = QDateTime::fromString(rawTime, Qt::ISODate);
                    if (dt.isValid()) timeStr = dt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss");
                }
                auto timeItem = new QTableWidgetItem(timeStr);
                tableWidget->setItem(i, 2, timeItem);
            }
#endif
        });
    }
};