#pragma once

#include "MainWindow.h"
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <algorithm>
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
        setWindowTitle("续费");
        resize(650, 700);

        QHBoxLayout *filterLayout = new QHBoxLayout();
        QLabel *filterLabel = new QLabel("分组筛选:");
        filterComboBox = new QComboBox();

        const auto& items = MainWindow::getInstance()->getTabs();
        for (const auto& item : items) {
            filterComboBox->addItem(item.name, item.bit);
        }

        filterLayout->addWidget(filterLabel);
        filterLayout->addWidget(filterComboBox);
        filterLayout->addStretch();

        tableWidget = new QTableWidget(this);
        tableWidget->setColumnCount(3);
        tableWidget->setHorizontalHeaderLabels({"", "设备名称", "到期时间"});
        tableWidget->setFrameShape(QFrame::NoFrame);
        tableWidget->setShowGrid(false);
        tableWidget->setAlternatingRowColors(true);
        tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableWidget->setFocusPolicy(Qt::NoFocus);

        QHeaderView *headerView = tableWidget->horizontalHeader();
        headerView->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        headerView->setSectionResizeMode(1, QHeaderView::Stretch);
        headerView->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        headerView->setSectionsClickable(false);

        tableWidget->setStyleSheet(R"(
            QTableWidget { font-size: 13px; border: 1px solid #e0e0e0; alternate-background-color: #f9f9f9; }
            QTableWidget::item { border-bottom: 1px solid #f0f0f0; }
            QTableWidget::item:selected { background-color: #e6f7ff; color: #000000; }
            QHeaderView::section { background-color: #f5f5f5; border: none; border-bottom: 1px solid #d0d0d0; padding: 8px; font-weight: bold; color: #555555; }
        )");

        QVBoxLayout *optionsLayout = new QVBoxLayout();
        optionsLayout->setSpacing(15);

        QGroupBox *durationGroupBox = new QGroupBox("续费周期");
        QHBoxLayout *durationLayout = new QHBoxLayout(durationGroupBox);

        monthRadioButton = new QRadioButton(QString("月付 (¥%1/台)").arg(BASE_PRICE_PER_MONTH));
        int yearPrice = BASE_PRICE_PER_MONTH * 12 / 2; // 年付5折
        yearRadioButton = new QRadioButton(QString("年付 (¥%1/台 - 5折特惠)").arg(yearPrice));
        yearRadioButton->setStyleSheet("color: #d32f2f; font-weight: bold;");
        monthRadioButton->setChecked(true);

        durationLayout->addWidget(monthRadioButton);
        durationLayout->addWidget(yearRadioButton);
        durationLayout->addStretch();

        QGroupBox *voucherGroupBox = new QGroupBox("代金券");
        QVBoxLayout *voucherLayout = new QVBoxLayout(voucherGroupBox);

        QHBoxLayout *balanceLayout = new QHBoxLayout();
        balanceLayout->addWidget(new QLabel("可用余额:"));
        balanceLabel = new QLabel();
        updateBalanceLabel();
        balanceLabel->setStyleSheet("font-weight: bold; color: #E65100; font-size: 14px;");
        balanceLayout->addWidget(balanceLabel);
        balanceLayout->addStretch();

        QHBoxLayout *redeemLayout = new QHBoxLayout();
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

        QGroupBox *paymentGroupBox = new QGroupBox("付款方式");
        QVBoxLayout *paymentMainLayout = new QVBoxLayout(paymentGroupBox);

        QHBoxLayout *paymentRadiosLayout = new QHBoxLayout();
        wechatRadioButton = new QRadioButton("微信支付");
        voucherRadioButton = new QRadioButton("余额支付");
        wechatRadioButton->setChecked(true);
        paymentRadiosLayout->addWidget(wechatRadioButton);
        paymentRadiosLayout->addWidget(voucherRadioButton);
        paymentRadiosLayout->addStretch();

        QHBoxLayout *totalLayout = new QHBoxLayout();
        QLabel *totalTitleLabel = new QLabel("应付总额:");
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

        // --- 4. 布局组装 ---
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);

        mainLayout->addLayout(filterLayout); // 顶部筛选
        mainLayout->addWidget(tableWidget, 1);
        mainLayout->addLayout(optionsLayout);
        mainLayout->addWidget(buttonBox);

        // --- 5. 逻辑处理与连接 ---

        // 筛选框改变时，调用独立的加载函数
        connect(filterComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
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
                emit redeemVouchersRequested(validCodes);
                voucherPlainTextEdit->clear();
            }
        });

        // --- 6. 初始加载 ---
        // 加载默认选项（通常是第一个选项）的数据
        if (filterComboBox->count() > 0) {
            loadDeviceTable(filterComboBox->itemData(0).toUInt());
        }

        setModal(true);
        exec();
    }

    // --- 新提取的函数实现 ---
    void loadDeviceTable(int bit)
    {
        auto devices = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));

        tableWidget->blockSignals(true);

        tableWidget->clearContents();
        tableWidget->setRowCount(0);
        tableWidget->setRowCount(devices.size());

        for (int i = 0; i < devices.size(); ++i) {
            const auto &info = devices[i];
            QTableWidgetItem *checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Checked);
            // checkItem->setData(Qt::UserRole, info.id);
            
            // 存储分组信息 (UserRole + 1)
            // checkItem->setData(Qt::UserRole + 1, info.group);

            checkItem->setTextAlignment(Qt::AlignCenter);
            tableWidget->setItem(i, 0, checkItem);
            
            tableWidget->setItem(i, 1, new QTableWidgetItem(info->deviceName));
            tableWidget->setItem(i, 2, new QTableWidgetItem(info->expireAt.toString("yyyy-MM-dd HH:mm:ss")));
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

public slots:
    void setVoucherBalance(int newBalance) {
        voucherBalance = newBalance;
        updateBalanceLabel();
        autoCheckPaymentMethod();
    }

signals:
    void redeemVouchersRequested(const QStringList& codes);

private:
    void updateBalanceLabel() {
        balanceLabel->setText(QString("¥ %1").arg(QString::number(voucherBalance, 'f', 2)));
    }

    void updateTotalPrice() {
        int selectedCount = 0;
        for (int i = 0; i < tableWidget->rowCount(); ++i) {
            // 只要勾选了就计算价格，不管是否被筛选器隐藏
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