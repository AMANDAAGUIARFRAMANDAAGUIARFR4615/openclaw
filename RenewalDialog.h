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
#include <QLineEdit>
#include <QGroupBox>
#include <QRegularExpression>

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

    // 这里设定基础单价，你可以根据实际业务传入或修改
    const double BASE_PRICE_PER_MONTH = 30.0; 

    explicit RenewalDialog(QList<DeviceInfo> devices, double initialBalance = 0.0, QWidget *parent = nullptr)
        : QDialog(parent), 
          m_tableWidget(new QTableWidget(this)),
          m_voucherBalance(initialBalance)
    {
        setWindowTitle(tr("设备续费"));
        resize(650, 600);

        // --- 表格部分 ---
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

        // --- 底部选项区域 ---
        QVBoxLayout *optionsLayout = new QVBoxLayout();
        optionsLayout->setSpacing(10);

        // 1. 续费时长 & 价格预览
        QGroupBox *grpDuration = new QGroupBox(tr("续费方案"));
        QHBoxLayout *layDuration = new QHBoxLayout(grpDuration);
        
        m_radioMonth = new QRadioButton(tr("月付 (¥%1/台)").arg(BASE_PRICE_PER_MONTH));
        // 年付计算：月费 * 12 * 0.5
        double yearPrice = BASE_PRICE_PER_MONTH * 12.0 * 0.5;
        m_radioYear = new QRadioButton(tr("年付 (¥%1/台 - 5折特惠)").arg(yearPrice));
        m_radioYear->setStyleSheet("color: #d32f2f; font-weight: bold;"); // 突出优惠
        m_radioMonth->setChecked(true);

        layDuration->addWidget(m_radioMonth);
        layDuration->addWidget(m_radioYear);
        layDuration->addStretch();
        
        // 2. 代金券 (支持批量)
        QGroupBox *grpVoucher = new QGroupBox(tr("代金券充值"));
        QVBoxLayout *layVoucher = new QVBoxLayout(grpVoucher);
        QHBoxLayout *balanceLayout = new QHBoxLayout();
        
        balanceLayout->addWidget(new QLabel(tr("当前可用余额:")));
        m_lblBalanceAmount = new QLabel();
        updateBalanceLabel();
        m_lblBalanceAmount->setStyleSheet("font-weight: bold; color: #E65100; font-size: 14px;");
        balanceLayout->addWidget(m_lblBalanceAmount);
        balanceLayout->addStretch();

        QHBoxLayout *redeemLayout = new QHBoxLayout();
        m_editVoucherCode = new QLineEdit();
        m_editVoucherCode->setPlaceholderText(tr("输入兑换码，支持批量(逗号隔开)"));
        m_btnRedeem = new QPushButton(tr("立即兑换"));
        redeemLayout->addWidget(m_editVoucherCode, 1);
        redeemLayout->addWidget(m_btnRedeem);
        
        layVoucher->addLayout(balanceLayout);
        layVoucher->addLayout(redeemLayout);
        
        // 3. 支付方式与总金额
        QGroupBox *grpPay = new QGroupBox(tr("支付结算"));
        QVBoxLayout *layPayMain = new QVBoxLayout(grpPay);
        
        // 支付单选
        QHBoxLayout *layPayRadios = new QHBoxLayout();
        m_radioWeChat = new QRadioButton(tr("微信支付"));
        m_radioVoucher = new QRadioButton(tr("余额支付"));
        m_radioWeChat->setChecked(true);
        layPayRadios->addWidget(m_radioWeChat);
        layPayRadios->addWidget(m_radioVoucher);
        layPayRadios->addStretch();

        // 总金额显示
        QHBoxLayout *layTotal = new QHBoxLayout();
        QLabel *lblTotalTitle = new QLabel(tr("应付总额:"));
        lblTotalTitle->setStyleSheet("font-size: 14px; font-weight: bold;");
        m_lblTotalAmount = new QLabel("¥ 0.00");
        m_lblTotalAmount->setStyleSheet("font-size: 20px; font-weight: bold; color: #d32f2f;");
        layTotal->addStretch();
        layTotal->addWidget(lblTotalTitle);
        layTotal->addWidget(m_lblTotalAmount);

        layPayMain->addLayout(layPayRadios);
        layPayMain->addLayout(layTotal); // 把总金额放在支付方式下面

        optionsLayout->addWidget(grpDuration);
        optionsLayout->addWidget(grpVoucher);
        optionsLayout->addWidget(grpPay);

        // --- 底部按钮 ---
        QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

        // --- 主布局 ---
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        mainLayout->addWidget(m_tableWidget, 1);
        mainLayout->addLayout(optionsLayout);
        mainLayout->addWidget(btnBox);
        
        // --- 逻辑连接 ---
        connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        // 监控复选框变化 -> 更新价格
        connect(m_tableWidget, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item){
            if (item->column() == 0) updateTotalPrice();
        });

        // 监控时长变化 -> 更新价格
        connect(m_radioMonth, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);
        
        // 批量兑换逻辑
        connect(m_btnRedeem, &QPushButton::clicked, this, [this](){
            QString text = m_editVoucherCode->text().trimmed();
            if (text.isEmpty()) return;

            // 使用正则分割：逗号、分号、空格、换行
            QStringList codes = text.split(QRegularExpression("[,;\\s\\n]+"), Qt::SkipEmptyParts);
            if (!codes.isEmpty()) {
                emit redeemVouchersRequested(codes);
                m_editVoucherCode->clear();
            }
        });

        // 初始化计算一次
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

    // 获取当前计算后的总价 (供外部使用)
    double getTotalPrice() const {
        return m_currentTotalPrice;
    }

public slots:
    void setVoucherBalance(double newBalance) {
        m_voucherBalance = newBalance;
        updateBalanceLabel();
    }

signals:
    // 发送多个兑换码
    void redeemVouchersRequested(const QStringList& codes);

private:
    void updateBalanceLabel() {
        m_lblBalanceAmount->setText(QString("¥ %1").arg(QString::number(m_voucherBalance, 'f', 2)));
    }

    void updateTotalPrice() {
        int selectedCount = 0;
        for (int i = 0; i < m_tableWidget->rowCount(); ++i) {
            if (m_tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                selectedCount++;
            }
        }

        double unitPrice = BASE_PRICE_PER_MONTH;
        if (m_radioYear->isChecked()) {
            unitPrice = BASE_PRICE_PER_MONTH * 12.0 * 0.5; // 年付5折
        }

        m_currentTotalPrice = unitPrice * selectedCount;
        m_lblTotalAmount->setText(QString("¥ %1").arg(QString::number(m_currentTotalPrice, 'f', 2)));
    }

    QTableWidget *m_tableWidget;
    
    QRadioButton *m_radioMonth;
    QRadioButton *m_radioYear;
    
    QLabel *m_lblBalanceAmount;
    QLineEdit *m_editVoucherCode;
    QPushButton *m_btnRedeem;

    QRadioButton *m_radioWeChat;
    QRadioButton *m_radioVoucher;
    QLabel *m_lblTotalAmount;

    double m_voucherBalance;
    double m_currentTotalPrice = 0.0;
};