#pragma once

#include "ToastWidget.h"
#include "MainWindow.h"
#include "Account.h"
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QSet>

class RenewalDialog : public QDialog {
    Q_OBJECT

public:
    enum DurationType { Monthly, Yearly };
    enum PaymentMethod { WeChat, Voucher };

    const int BASE_PRICE_PER_MONTH = 10;

    explicit RenewalDialog(QWidget *parent) : QDialog(parent)
    {
        setModal(true);
        setWindowTitle("续费");
        setMinimumSize(480, 720);

        auto filterLayout = new QHBoxLayout();
        auto filterLabel = new QLabel("分组筛选:");
        filterComboBox = new QComboBox();

        const auto& items = MainWindow::getInstance()->getTabs();
        for (const auto& item : items) {
            filterComboBox->addItem(item.name, item.bit);
        }

        filterLayout->addWidget(filterLabel);
        filterLayout->addWidget(filterComboBox);
        filterLayout->addStretch();

        tableWidget = new QTableWidget(this);
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

        tableWidget->setStyleSheet(R"(
            QTableWidget { font-size: 13px; border: 1px solid #e0e0e0; alternate-background-color: #f9f9f9; }
            QTableWidget::item { border-bottom: 1px solid #f0f0f0; }
            QTableWidget::item:selected { background-color: #e6f7ff; color: #000000; }
            QHeaderView::section { background-color: #f5f5f5; border: none; border-bottom: 1px solid #d0d0d0; padding: 8px; font-weight: bold; color: #555555; }
        )");

        auto optionsLayout = new QVBoxLayout();
        optionsLayout->setSpacing(15);

        auto durationGroupBox = new QGroupBox("续费周期");
        QHBoxLayout *durationLayout = new QHBoxLayout(durationGroupBox);

        monthRadioButton = new QRadioButton(QString("月付 (¥%1/台)").arg(BASE_PRICE_PER_MONTH));
        int yearPrice = BASE_PRICE_PER_MONTH * 12 / 2; // 年付5折
        yearRadioButton = new QRadioButton(QString("年付 (¥%1/台 - 5折特惠)").arg(yearPrice));
        yearRadioButton->setStyleSheet("color: #d32f2f; font-weight: bold;");
        monthRadioButton->setChecked(true);

        durationLayout->addWidget(monthRadioButton);
        durationLayout->addWidget(yearRadioButton);
        durationLayout->addStretch();

        auto voucherGroupBox = new QGroupBox("代金券");
        auto voucherLayout = new QVBoxLayout(voucherGroupBox);

        auto balanceLayout = new QHBoxLayout();
        balanceLayout->addWidget(new QLabel("可用余额:"));
        balanceLabel = new QLabel();
        updateBalanceLabel();
        balanceLabel->setStyleSheet("font-weight: bold; color: #E65100; font-size: 14px;");
        balanceLayout->addWidget(balanceLabel);
        balanceLayout->addStretch();

        auto redeemLayout = new QHBoxLayout();
        voucherPlainTextEdit = new QPlainTextEdit();
        voucherPlainTextEdit->setPlaceholderText("请输入兑换码，每行一个...");
        voucherPlainTextEdit->setFixedHeight(80);
        voucherPlainTextEdit->setStyleSheet("QPlainTextEdit { border: 1px solid #ccc; border-radius: 4px; padding: 4px; }");

        redeemButton = new QPushButton("批量兑换");
        redeemButton->setFixedHeight(80);

        redeemLayout->addWidget(voucherPlainTextEdit, 1);
        redeemLayout->addWidget(redeemButton);

        voucherLayout->addLayout(balanceLayout);
        voucherLayout->addLayout(redeemLayout);

        auto paymentGroupBox = new QGroupBox("付款方式");
        auto paymentMainLayout = new QVBoxLayout(paymentGroupBox);

        auto paymentRadiosLayout = new QHBoxLayout();
        wechatRadioButton = new QRadioButton("微信支付");
        voucherRadioButton = new QRadioButton("余额支付");
        wechatRadioButton->setChecked(true);
        paymentRadiosLayout->addWidget(wechatRadioButton);
        paymentRadiosLayout->addWidget(voucherRadioButton);
        paymentRadiosLayout->addStretch();

        auto totalLayout = new QHBoxLayout();
        auto totalTitleLabel = new QLabel("应付总额:");
        totalTitleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
        totalAmountLabel = new QLabel("¥ 0.00");
        totalAmountLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #d32f2f;");
        totalLayout->addStretch();
        totalLayout->addWidget(totalTitleLabel);
        totalLayout->addWidget(totalAmountLabel);

        paymentMainLayout->addLayout(paymentRadiosLayout);
        paymentMainLayout->addLayout(totalLayout);

        optionsLayout->addWidget(durationGroupBox);
        optionsLayout->addWidget(voucherGroupBox);
        optionsLayout->addWidget(paymentGroupBox);

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
            if (wechatRadioButton->isChecked()) {
                new ToastWidget("暂不支持微信支付", this);
                return;
            }

            if (voucherBalance < currentTotalPrice) {
                new ToastWidget("余额不足", this);
                return;
            }

            QList<QString> selectedIds = getSelectedDeviceIds();
            if (selectedIds.isEmpty()) {
                new ToastWidget("请至少选择一台设备", this);
                return;
            }

            QJsonObject payload;
            payload["ids"] = QJsonArray::fromStringList(selectedIds);
            payload["isYearly"] = getDuration() == Yearly;

            setEnabled(false); 
            setCursor(Qt::WaitCursor);

            webSocketClient.emitEvent("deviceRenew", payload, [=](const QJsonValue &res) {
                setEnabled(true);
                unsetCursor();

                if (res.isString()) {
                    new ToastWidget(res.toString(), this);
                    return;
                }

                setVoucherBalance(res["balance"].toInt());

                const auto& devices = res["devices"].toArray();

                for (const QJsonValue &item : devices) {
                    auto deviceInfo = DeviceInfo::getDevice(item["udid"].toString());
                    deviceInfo->expireAt.set(QDateTime::fromMSecsSinceEpoch(item["expireAt"].toInteger()));
                }

                accept();
            });
        });
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);

        mainLayout->addLayout(filterLayout); // 顶部筛选
        mainLayout->addWidget(tableWidget, 1);
        mainLayout->addLayout(optionsLayout);
        mainLayout->addWidget(buttonBox);

        connect(filterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
            qDebugEx() << "QComboBox::currentIndexChanged" << index;
            if (index < 0)
                return;

            auto bit = filterComboBox->itemData(index).toUInt();
            loadDeviceTable(bit); 
        });

        connect(tableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item){
            if (item->column() == 0) updateTotalPrice();
        });
        
        connect(monthRadioButton, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);

        connect(redeemButton, &QPushButton::clicked, this, [this](){
            QString content = voucherPlainTextEdit->toPlainText();
            if (content.trimmed().isEmpty()) return;

            QStringList lines = content.split('\n', Qt::SkipEmptyParts);
            QStringList validCodes;
            for (const QString& line : lines) {
                QString code = line.trimmed();
                if (!code.isEmpty()) {
                    validCodes.append(code);
                }
            }

            if (!validCodes.isEmpty()) {
                redeemButton->setEnabled(false);
                webSocketClient.emitEvent("redeem", QJsonArray::fromStringList(validCodes), [=](const QJsonValue &res) {
                    redeemButton->setEnabled(true);

                    setVoucherBalance(res["balance"].toInt());
                    new ToastWidget(res["msg"].toString(), this);
                    voucherPlainTextEdit->clear();
                });
            }
        });

        filterComboBox->setCurrentIndex(-1);
        filterComboBox->setCurrentIndex(MainWindow::getInstance()->tabWidget->currentIndex());

        setVoucherBalance(Account::getInstance()->balance);
    }

protected:

    void loadDeviceTable(int bit)
    {
        auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

        tableWidget->blockSignals(true);

        tableWidget->clearContents();
        tableWidget->setRowCount(0);
        tableWidget->setRowCount(devices.size());

        for (int i = 0; i < devices.size(); ++i) {
            const auto &info = devices[i];
            auto checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Checked);
            checkItem->setData(Qt::UserRole, info->deviceId);

            tableWidget->setItem(i, 0, checkItem);
            
            tableWidget->setItem(i, 1, new QTableWidgetItem(info->deviceName));
            tableWidget->setItem(i, 2, new QTableWidgetItem(info->model));
            tableWidget->setItem(i, 3, new QTableWidgetItem(info->expireAt.get().toString("yyyy-MM-dd HH:mm:ss")));
        }

        tableWidget->blockSignals(false);
        updateTotalPrice();
    }

    QList<QString> getSelectedDeviceIds() const {
        QList<QString> result;
        for (int i = 0; i < tableWidget->rowCount(); ++i) {
            auto *item = tableWidget->item(i, 0);
            if (item && item->checkState() == Qt::Checked) {
                result.append(item->data(Qt::UserRole).toString());
            }
        }
        return result;
    }

    DurationType getDuration() const {
        return yearRadioButton->isChecked() ? Yearly : Monthly;
    }

    PaymentMethod getPaymentMethod() const {
        return voucherRadioButton->isChecked() ? Voucher : WeChat;
    }

    int getTotalPrice() const {
        return currentTotalPrice;
    }

    void setVoucherBalance(int balance) {
        voucherBalance = balance;
        Account::getInstance()->balance = balance;
        updateBalanceLabel();
        autoCheckPaymentMethod();
    }

    void updateBalanceLabel() {
        balanceLabel->setText(QString("¥ %1").arg(QString::number(voucherBalance, 'f', 2)));
    }

    void updateTotalPrice() {
        int selectedCount = 0;
        for (int i = 0; i < tableWidget->rowCount(); ++i) {
            if (tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                selectedCount++;
            }
        }

        int unitPrice = BASE_PRICE_PER_MONTH;
        if (yearRadioButton->isChecked()) {
            unitPrice = BASE_PRICE_PER_MONTH * 12 / 2; 
        }

        currentTotalPrice = unitPrice * selectedCount;
        totalAmountLabel->setText(QString("¥ %1").arg(QString::number(currentTotalPrice, 'f', 2)));
        
        autoCheckPaymentMethod();
    }

    void autoCheckPaymentMethod() {
        if (currentTotalPrice > 0 && voucherBalance >= currentTotalPrice) {
            if (!voucherRadioButton->isChecked()) {
                voucherRadioButton->setChecked(true);
            }
        } else {
            if (!wechatRadioButton->isChecked()) {
                wechatRadioButton->setChecked(true);
            }
        }
    }

    QTableWidget *tableWidget;
    QComboBox *filterComboBox; 
    QRadioButton *monthRadioButton;
    QRadioButton *yearRadioButton;
    
    QLabel *balanceLabel;
    QPlainTextEdit *voucherPlainTextEdit;
    QPushButton *redeemButton;

    QRadioButton *wechatRadioButton;
    QRadioButton *voucherRadioButton;
    QLabel *totalAmountLabel;

    int voucherBalance = 0;
    int currentTotalPrice = 0;
};
