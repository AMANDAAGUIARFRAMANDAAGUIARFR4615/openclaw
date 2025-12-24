#pragma once

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

struct DeviceInfo {
    QString id;
    QString name;
    QDateTime expireTime;
};

class RenewalDialog : public QDialog {
    Q_OBJECT

public:
    enum DurationType { Monthly, Yearly };
    enum PaymentMethod { WeChat, Voucher };

    // 基础单价设置
    const int BASE_PRICE_PER_MONTH = 10;

    explicit RenewalDialog(QList<DeviceInfo> devices, int initialBalance, QWidget *parent = nullptr)
        : QDialog(parent), 
          m_tableWidget(new QTableWidget(this)),
          m_voucherBalance(initialBalance)
    {
        setWindowTitle(tr("续费"));
        resize(650, 650);

        // --- 1. 表格部分 ---
        std::sort(devices.begin(), devices.end(), [](const DeviceInfo& a, const DeviceInfo& b) {
            return a.expireTime < b.expireTime;
        });

        m_tableWidget->setColumnCount(3);
        m_tableWidget->setHorizontalHeaderLabels({"", tr("设备名称"), tr("到期时间")});
        m_tableWidget->setFrameShape(QFrame::NoFrame);
        m_tableWidget->setShowGrid(false);
        m_tableWidget->setAlternatingRowColors(true);
        m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_tableWidget->setFocusPolicy(Qt::NoFocus);

        QHeaderView *header = m_tableWidget->horizontalHeader();
        header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(1, QHeaderView::Stretch);
        header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        header->setSectionsClickable(false);

        m_tableWidget->setStyleSheet(R"(
            QTableWidget { font-size: 13px; border: 1px solid #e0e0e0; alternate-background-color: #f9f9f9; }
            QTableWidget::item { border-bottom: 1px solid #f0f0f0; }
            QTableWidget::item:selected { background-color: #e6f7ff; color: #000000; }
            QHeaderView::section { background-color: #f5f5f5; border: none; border-bottom: 1px solid #d0d0d0; padding: 8px; font-weight: bold; color: #555555; }
        )");

        m_tableWidget->setRowCount(devices.size());
        for (int i = 0; i < devices.size(); ++i) {
            const auto &info = devices[i];
            QTableWidgetItem *checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Checked);
            checkItem->setData(Qt::UserRole, info.id);
            checkItem->setTextAlignment(Qt::AlignCenter);
            m_tableWidget->setItem(i, 0, checkItem);
            m_tableWidget->setItem(i, 1, new QTableWidgetItem(info.name));
            m_tableWidget->setItem(i, 2, new QTableWidgetItem(info.expireTime.toString("yyyy-MM-dd HH:mm:ss")));
        }

        // --- 2. 底部选项区域 ---
        QVBoxLayout *optionsLayout = new QVBoxLayout();
        optionsLayout->setSpacing(15);

        // A. 续费方案
        QGroupBox *grpDuration = new QGroupBox(tr("续费方案"));
        QHBoxLayout *layDuration = new QHBoxLayout(grpDuration);
        
        m_radioMonth = new QRadioButton(tr("月付 (¥%1/台)").arg(BASE_PRICE_PER_MONTH));
        int yearPrice = BASE_PRICE_PER_MONTH * 12 / 2; // 年付5折
        m_radioYear = new QRadioButton(tr("年付 (¥%1/台 - 5折特惠)").arg(yearPrice));
        m_radioYear->setStyleSheet("color: #d32f2f; font-weight: bold;");
        m_radioMonth->setChecked(true);

        layDuration->addWidget(m_radioMonth);
        layDuration->addWidget(m_radioYear);
        layDuration->addStretch();
        
        // B. 代金券充值
        QGroupBox *grpVoucher = new QGroupBox(tr("代金券充值"));
        QVBoxLayout *layVoucher = new QVBoxLayout(grpVoucher);
        
        QHBoxLayout *balanceLayout = new QHBoxLayout();
        balanceLayout->addWidget(new QLabel(tr("当前可用余额:")));
        m_lblBalanceAmount = new QLabel();
        updateBalanceLabel(); // 初始化显示余额
        m_lblBalanceAmount->setStyleSheet("font-weight: bold; color: #E65100; font-size: 14px;");
        balanceLayout->addWidget(m_lblBalanceAmount);
        balanceLayout->addStretch();

        QHBoxLayout *redeemLayout = new QHBoxLayout();
        m_editVoucherCode = new QPlainTextEdit();
        m_editVoucherCode->setPlaceholderText(tr("请输入兑换码，每行一个..."));
        m_editVoucherCode->setFixedHeight(80);
        m_editVoucherCode->setStyleSheet("QPlainTextEdit { border: 1px solid #ccc; border-radius: 4px; padding: 4px; }");
        
        m_btnRedeem = new QPushButton(tr("批量兑换"));
        m_btnRedeem->setFixedHeight(80);
        
        redeemLayout->addWidget(m_editVoucherCode, 1);
        redeemLayout->addWidget(m_btnRedeem);
        
        layVoucher->addLayout(balanceLayout);
        layVoucher->addLayout(redeemLayout);
        
        // C. 支付结算
        QGroupBox *grpPay = new QGroupBox(tr("支付结算"));
        QVBoxLayout *layPayMain = new QVBoxLayout(grpPay);
        
        QHBoxLayout *layPayRadios = new QHBoxLayout();
        m_radioWeChat = new QRadioButton(tr("微信支付"));
        m_radioVoucher = new QRadioButton(tr("余额支付"));
        m_radioWeChat->setChecked(true); // 默认先选微信，后面 autoCheckPaymentMethod 会根据余额修正
        layPayRadios->addWidget(m_radioWeChat);
        layPayRadios->addWidget(m_radioVoucher);
        layPayRadios->addStretch();

        QHBoxLayout *layTotal = new QHBoxLayout();
        QLabel *lblTotalTitle = new QLabel(tr("应付总额:"));
        lblTotalTitle->setStyleSheet("font-size: 14px; font-weight: bold;");
        m_lblTotalAmount = new QLabel("¥ 0.00");
        m_lblTotalAmount->setStyleSheet("font-size: 20px; font-weight: bold; color: #d32f2f;");
        layTotal->addStretch();
        layTotal->addWidget(lblTotalTitle);
        layTotal->addWidget(m_lblTotalAmount);

        layPayMain->addLayout(layPayRadios);
        layPayMain->addLayout(layTotal);

        optionsLayout->addWidget(grpDuration);
        optionsLayout->addWidget(grpVoucher);
        optionsLayout->addWidget(grpPay);

        // --- 3. 布局组装 ---
        QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        mainLayout->addWidget(m_tableWidget, 1);
        mainLayout->addLayout(optionsLayout);
        mainLayout->addWidget(btnBox);
        
        // --- 4. 逻辑处理 ---
        
        // 价格更新
        connect(m_tableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item){
            if (item->column() == 0) updateTotalPrice();
        });
        connect(m_radioMonth, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);
        
        // 兑换按钮点击
        connect(m_btnRedeem, &QPushButton::clicked, this, [this](){
            QString content = m_editVoucherCode->toPlainText();
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
                m_editVoucherCode->clear();
            }
        });

        // 初始计算价格并判断支付方式
        updateTotalPrice();
    }

    QList<QString> getSelectedDeviceIds() const {
        QList<QString> result;
        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
            auto *item = m_tableWidget->item(i, 0);
            if (item && item->checkState() == Qt::Checked) {
                result.append(item->data(Qt::UserRole).toString());
            }
        }
        return result;
    }

    DurationType getDuration() const {
        return m_radioYear->isChecked() ? Yearly : Monthly;
    }

    PaymentMethod getPaymentMethod() const {
        return m_radioVoucher->isChecked() ? Voucher : WeChat;
    }

    int getTotalPrice() const {
        return m_currentTotalPrice;
    }

public slots:
    // 外部调用此函数更新余额（例如兑换成功后）
    void setVoucherBalance(int newBalance) {
        m_voucherBalance = newBalance;
        updateBalanceLabel();
        // 余额变化了，尝试自动选择支付方式
        autoCheckPaymentMethod();
    }

signals:
    void redeemVouchersRequested(const QStringList& codes);

private:
    void updateBalanceLabel() {
        m_lblBalanceAmount->setText(QString("¥ %1").arg(QString::number(m_voucherBalance, 'f', 2)));
    }

    // 更新总价并触发支付方式检查
    void updateTotalPrice() {
        int selectedCount = 0;
        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
            if (m_tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                selectedCount++;
            }
        }

        int unitPrice = BASE_PRICE_PER_MONTH;
        if (m_radioYear->isChecked()) {
            unitPrice = BASE_PRICE_PER_MONTH * 12 / 2; // 5折
        }

        m_currentTotalPrice = unitPrice * selectedCount;
        m_lblTotalAmount->setText(QString("¥ %1").arg(QString::number(m_currentTotalPrice, 'f', 2)));
        
        // 价格变化了，尝试自动选择支付方式
        autoCheckPaymentMethod();
    }

    // 核心逻辑：自动选择支付方式
    void autoCheckPaymentMethod() {
        if (m_currentTotalPrice > 0 && m_voucherBalance >= m_currentTotalPrice) {
            // 余额充足，自动勾选余额支付
            if (!m_radioVoucher->isChecked()) {
                m_radioVoucher->setChecked(true);
            }
        } else {
            // 余额不足，自动勾选微信支付
            if (!m_radioWeChat->isChecked()) {
                m_radioWeChat->setChecked(true);
            }
        }
    }

    QTableWidget *m_tableWidget;
    QRadioButton *m_radioMonth;
    QRadioButton *m_radioYear;
    
    QLabel *m_lblBalanceAmount;
    QPlainTextEdit *m_editVoucherCode;
    QPushButton *m_btnRedeem;

    QRadioButton *m_radioWeChat;
    QRadioButton *m_radioVoucher;
    QLabel *m_lblTotalAmount;

    int m_voucherBalance;
    int m_currentTotalPrice = 0;
};