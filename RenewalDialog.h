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
#include <QRadioButton>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSet>
#include <QStyleHints>

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

        bool isDarkMode = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        
        // 红色 (强调/警告/价格) - 深色模式下用亮红，浅色模式下用深红
        alertColor = isDarkMode ? QColor("#ff5252") : QColor("#d32f2f");

        auto filterLayout = new QHBoxLayout();
        auto filterLabel = new QLabel("分组筛选:");
        filterComboBox = new QComboBox();

        for (const auto& item : MainWindow::getInstance()->getTabs()) {
            filterComboBox->addItem(item.name, item.bit);
        }

        expiredFilterCheckBox = new QCheckBox("只显示已到期");

        filterLayout->addWidget(filterLabel);
        filterLayout->addWidget(filterComboBox);
        filterLayout->addWidget(expiredFilterCheckBox);
        filterLayout->addStretch();

        auto selectionLayout = new QHBoxLayout();
        selectAllCheckBox = new QCheckBox("全选");
        selectAllCheckBox->setTristate(true); // 开启三态，支持半选显示
        selectionLayout->addWidget(selectAllCheckBox);
        selectionLayout->addStretch();

        tableWidget = new QTableWidget(this);
        tableWidget->setColumnCount(4);
        tableWidget->setHorizontalHeaderLabels({"", "设备名称", "机型", "到期时间"});
        tableWidget->setFrameShape(QFrame::StyledPanel);
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
            QTableWidget { border: 1px solid palette(mid); }
            QHeaderView::section { 
                background-color: palette(button); 
                border: none; 
                border-bottom: 1px solid palette(mid); 
                padding: 4px; 
            }
        )");

        auto optionsLayout = new QVBoxLayout();
        optionsLayout->setSpacing(15);

        auto durationGroupBox = new QGroupBox("续费周期");
        QHBoxLayout *durationLayout = new QHBoxLayout(durationGroupBox);

        monthRadioButton = new QRadioButton(QString("月付 (¥%1/台)").arg(BASE_PRICE_PER_MONTH));
        int yearPrice = BASE_PRICE_PER_MONTH * 12 / 2; // 年付5折
        yearRadioButton = new QRadioButton(QString("年付 (¥%1/台 - 5折特惠)").arg(yearPrice));
        
        QPalette yearPalette = yearRadioButton->palette();
        yearPalette.setColor(QPalette::WindowText, alertColor);
        yearRadioButton->setPalette(yearPalette);
        QFont yearFont = yearRadioButton->font();
        yearFont.setBold(true);
        yearRadioButton->setFont(yearFont);

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
        
        QPalette balancePalette = balanceLabel->palette();
        balancePalette.setColor(QPalette::WindowText, alertColor);
        balanceLabel->setPalette(balancePalette);
        QFont balanceFont = balanceLabel->font();
        balanceFont.setBold(true);
        balanceFont.setPixelSize(14);
        balanceLabel->setFont(balanceFont);

        balanceLayout->addWidget(balanceLabel);
        balanceLayout->addStretch();

        auto redeemLayout = new QHBoxLayout();
        voucherPlainTextEdit = new QPlainTextEdit();
        voucherPlainTextEdit->setPlaceholderText("请输入兑换码，每行一个...");
        voucherPlainTextEdit->setFixedHeight(80);

        redeemButton = new QPushButton("批量兑换");
        redeemButton->setFixedHeight(80);

        redeemLayout->addWidget(voucherPlainTextEdit, 1);
        redeemLayout->addWidget(redeemButton);

        voucherLayout->addLayout(balanceLayout);
        voucherLayout->addLayout(redeemLayout);

        auto paymentGroupBox = new QGroupBox("付款方式");
        auto paymentMainLayout = new QVBoxLayout(paymentGroupBox);

        auto paymentRadiosLayout = new QHBoxLayout();
        voucherRadioButton = new QRadioButton("余额支付");
        auto wechatRadioButton = new QRadioButton("微信支付（联系客服）");
        auto alipayRadioButton = new QRadioButton("支付宝支付（联系客服）");
        voucherRadioButton->setChecked(true);
        paymentRadiosLayout->addWidget(voucherRadioButton);
        paymentRadiosLayout->addWidget(wechatRadioButton);
        paymentRadiosLayout->addWidget(alipayRadioButton);
        paymentRadiosLayout->addStretch();

        auto totalLayout = new QHBoxLayout();
        auto totalTitleLabel = new QLabel("应付总额:");
        totalTitleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
        totalAmountLabel = new QLabel("¥ 0.00");
        
        QPalette totalAmountPalette = totalAmountLabel->palette();
        totalAmountPalette.setColor(QPalette::WindowText, alertColor);
        totalAmountLabel->setPalette(totalAmountPalette);
        QFont totalAmountFont = totalAmountLabel->font();
        totalAmountFont.setBold(true);
        totalAmountFont.setPixelSize(20);
        totalAmountLabel->setFont(totalAmountFont);

        totalLayout->addStretch();
        totalLayout->addWidget(totalTitleLabel);
        totalLayout->addWidget(totalAmountLabel);

        paymentMainLayout->addLayout(paymentRadiosLayout);
        paymentMainLayout->addLayout(totalLayout);

        optionsLayout->addWidget(durationGroupBox);
        optionsLayout->addWidget(voucherGroupBox);
        optionsLayout->addWidget(paymentGroupBox);

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, [this]() {
            if (!voucherRadioButton->isChecked()) {
                MainWindow::getInstance()->showSupportDialog();
                return;
            }

            if (voucherBalance < currentTotalPrice) {
                QToolTip::showText(QCursor::pos(), "余额不足");
                return;
            }

            QList<QString> selectedIds = getSelectedDeviceIds();
            if (selectedIds.isEmpty()) {
                QToolTip::showText(QCursor::pos(), "请至少选择一台设备");
                return;
            }

            QJsonObject payload;
            payload["ids"] = QJsonArray::fromStringList(selectedIds);
            payload["isYearly"] = getDuration() == Yearly;

            setEnabled(false); 
            setCursor(Qt::WaitCursor);

            webSocketClient->emitEvent("deviceRenew", payload, [=](const QJsonValue &res) {
                setEnabled(true);
                unsetCursor();

                if (res.isString()) {
                    QToolTip::showText(QCursor::pos(), res.toString());
                    return;
                }

                setVoucherBalance(res["balance"].toInt());

                for (const QJsonValue &item : res[HIDE("devices")].toArray()) {
                    auto deviceInfo = DeviceInfo::getDevice(item[HIDE("udid")].toString());
                    deviceInfo->expireAt = item[HIDE("expireAt")].toInteger();
                    DeviceInfo::expirations[deviceInfo->deviceId] = deviceInfo->expireAt;
                }

                accept();
            });
        });
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);

        mainLayout->addLayout(filterLayout);
        mainLayout->addLayout(selectionLayout);
        mainLayout->addWidget(tableWidget, 1);
        mainLayout->addLayout(optionsLayout);
        mainLayout->addWidget(buttonBox);

        connect(selectAllCheckBox, &QCheckBox::clicked, [this](bool) {
            Qt::CheckState state = selectAllCheckBox->checkState();
            
            // 用户点击时，如果是半选状态，通常预期是变为全选
            if (state == Qt::PartiallyChecked) {
                state = Qt::Checked;
                selectAllCheckBox->setCheckState(state);
            }

            tableWidget->blockSignals(true); // 暂时屏蔽表格信号，防止递归
            for (int i = 0; i < tableWidget->rowCount(); ++i) {
                auto item = tableWidget->item(i, 0);
                if (item) item->setCheckState(state);
            }
            tableWidget->blockSignals(false);
            
            updateTotalPrice();
        });

        connect(filterComboBox, &QComboBox::currentIndexChanged, [this](int index) {
            qDebugEx() << "QComboBox::currentIndexChanged" << index;
            if (index < 0)
                return;

            auto bit = filterComboBox->itemData(index).toUInt();
            loadDeviceTable(bit); 
        });

        connect(expiredFilterCheckBox, &QCheckBox::stateChanged, [this](int) {
             // 触发当前选中的分组刷新
             int index = filterComboBox->currentIndex();
             if (index >= 0) {
                 auto bit = filterComboBox->itemData(index).toUInt();
                 loadDeviceTable(bit);
             }
        });

        connect(tableWidget, &QTableWidget::itemChanged, [this](QTableWidgetItem *item){
            if (item->column() == 0) {
                updateTotalPrice();
                updateSelectAllState();
            }
        });
        
        connect(monthRadioButton, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);

        connect(redeemButton, &QPushButton::clicked, [this](){
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
                webSocketClient->emitEvent("redeem", QJsonArray::fromStringList(validCodes), [=](const QJsonValue &res) {
                    redeemButton->setEnabled(true);

                    setVoucherBalance(res["balance"].toInt());
                    QToolTip::showText(QCursor::pos(), res["msg"].toString());
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
        auto devices = DeviceInfo::getDevices(1U << bit);
        
        qint64 currentTimestamp = Account::getInstance()->loginTime.get() + elapsedTimer->elapsed();

        if (expiredFilterCheckBox->isChecked()) {
            QList<DeviceInfo*> filteredDevices;

            for (const auto& device : devices) {
                if (device->expireAt.get() < currentTimestamp)
                    filteredDevices.append(device);
            }

            devices = filteredDevices;
        }

        tableWidget->blockSignals(true);

        tableWidget->clearContents();
        tableWidget->setRowCount(0);
        tableWidget->setRowCount(devices.size());

        for (int i = 0; i < devices.size(); ++i) {
            const auto &deviceInfo = devices[i];
            auto checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Checked);
            checkItem->setData(Qt::UserRole, deviceInfo->deviceId);

            tableWidget->setItem(i, 0, checkItem);
            
            tableWidget->setItem(i, 1, new QTableWidgetItem(deviceInfo->deviceName));
            tableWidget->setItem(i, 2, new QTableWidgetItem(deviceInfo->model));

            auto expireAt = QDateTime::fromMSecsSinceEpoch(deviceInfo->expireAt.get()).toString(HIDE("yyyy-MM-dd HH:mm:ss"));
            auto expireItem = new QTableWidgetItem(expireAt);
            tableWidget->setItem(i, 3, expireItem);

            if (deviceInfo->expireAt.get() < currentTimestamp)
                expireItem->setForeground(QBrush(alertColor));
        }

        tableWidget->blockSignals(false);
        updateSelectAllState();
        updateTotalPrice();
    }

    void updateSelectAllState() {
        int checkedCount = 0;
        int rowCount = tableWidget->rowCount();

        for (int i = 0; i < rowCount; ++i) {
            if (tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                checkedCount++;
            }
        }

        selectAllCheckBox->blockSignals(true);
        
        if (rowCount > 0 && checkedCount == rowCount)
            selectAllCheckBox->setCheckState(Qt::Checked);
        else if (checkedCount == 0)
            selectAllCheckBox->setCheckState(Qt::Unchecked);
        else
            selectAllCheckBox->setCheckState(Qt::PartiallyChecked);
        
        selectAllCheckBox->blockSignals(false);
    }

    QList<QString> getSelectedDeviceIds() const {
        QList<QString> result;
        for (int i = 0; i < tableWidget->rowCount(); ++i) {
            auto item = tableWidget->item(i, 0);
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
    }

    void updateBalanceLabel() {
        balanceLabel->setText(QString("¥%1").arg(QString::number(voucherBalance)));
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
        totalAmountLabel->setText(QString("¥%1").arg(QString::number(currentTotalPrice)));
    }

    QTableWidget *tableWidget;
    QComboBox *filterComboBox;
    QCheckBox *expiredFilterCheckBox;
    QCheckBox *selectAllCheckBox;
    QRadioButton *monthRadioButton;
    QRadioButton *yearRadioButton;
    
    QLabel *balanceLabel;
    QPlainTextEdit *voucherPlainTextEdit;
    QPushButton *redeemButton;

    QRadioButton *voucherRadioButton;
    QLabel *totalAmountLabel;

    int voucherBalance = 0;
    int currentTotalPrice = 0;

    QColor alertColor;
};
