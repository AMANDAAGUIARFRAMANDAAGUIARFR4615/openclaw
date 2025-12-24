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
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
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

    explicit RenewalDialog(QList<DeviceInfo> devices, double initialBalance = 0.0, QWidget *parent = nullptr)
        : QDialog(parent), 
          m_tableWidget(new QTableWidget(this)),
          m_voucherBalance(initialBalance)
    {
        setWindowTitle(tr("设备续费"));
        resize(600, 600);

        // --- 表格部分 (与原代码基本一致) ---
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

        // --- 新增：底部选项与操作区域 ---
        QVBoxLayout *optionsLayout = new QVBoxLayout();
        optionsLayout->setSpacing(10);

        // 1. 续费时长
        QGroupBox *grpDuration = new QGroupBox(tr("续费时长"));
        QHBoxLayout *layDuration = new QHBoxLayout(grpDuration);
        m_radioMonth = new QRadioButton(tr("月付"));
        m_radioYear = new QRadioButton(tr("年付"));
        m_radioMonth->setChecked(true);
        layDuration->addWidget(m_radioMonth);
        layDuration->addWidget(m_radioYear);
        layDuration->addStretch();
        
        // 2. 代金券
        QGroupBox *grpVoucher = new QGroupBox(tr("代金券"));
        QVBoxLayout *layVoucher = new QVBoxLayout(grpVoucher);
        QHBoxLayout *balanceLayout = new QHBoxLayout();
        balanceLayout->addWidget(new QLabel(tr("当前余额:")));
        m_lblBalanceAmount = new QLabel(QString("¥ %1").arg(QString::number(m_voucherBalance, 'f', 2)));
        m_lblBalanceAmount->setStyleSheet("font-weight: bold; color: #d32f2f;");
        balanceLayout->addWidget(m_lblBalanceAmount);
        balanceLayout->addStretch();

        QHBoxLayout *redeemLayout = new QHBoxLayout();
        m_editVoucherCode = new QLineEdit();
        m_editVoucherCode->setPlaceholderText(tr("请输入代金券兑换码"));
        m_btnRedeem = new QPushButton(tr("兑换"));
        redeemLayout->addWidget(m_editVoucherCode, 1);
        redeemLayout->addWidget(m_btnRedeem);
        
        layVoucher->addLayout(balanceLayout);
        layVoucher->addLayout(redeemLayout);
        
        // 3. 支付方式
        QGroupBox *grpPay = new QGroupBox(tr("支付方式"));
        QHBoxLayout *layPay = new QHBoxLayout(grpPay);
        m_radioWeChat = new QRadioButton(tr("微信支付"));
        m_radioVoucher = new QRadioButton(tr("余额支付"));
        m_radioWeChat->setChecked(true);
        layPay->addWidget(m_radioWeChat);
        layPay->addWidget(m_radioVoucher);
        layPay->addStretch();
        
        optionsLayout->addWidget(grpDuration);
        optionsLayout->addWidget(grpVoucher);
        optionsLayout->addWidget(grpPay);

        // --- 底部确认/取消按钮 ---
        QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

        // --- 整体布局 ---
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);
        mainLayout->addWidget(m_tableWidget, 1);
        mainLayout->addLayout(optionsLayout);
        mainLayout->addWidget(btnBox);
        
        // --- 信号连接 ---
        connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        
        connect(m_btnRedeem, &QPushButton::clicked, this, [this](){
            QString code = m_editVoucherCode->text().trimmed();
            if (!code.isEmpty()) {
                emit redeemVoucherRequested(code);
                m_editVoucherCode->clear();
            }
        });
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

public slots:
    void setVoucherBalance(double newBalance) {
        m_voucherBalance = newBalance;
        m_lblBalanceAmount->setText(QString("¥ %1").arg(QString::number(m_voucherBalance, 'f', 2)));
    }

signals:
    void redeemVoucherRequested(const QString& code);

private:
    QTableWidget *m_tableWidget;
    
    QRadioButton *m_radioMonth;
    QRadioButton *m_radioYear;
    
    QLabel *m_lblBalanceAmount;
    QLineEdit *m_editVoucherCode;
    QPushButton *m_btnRedeem;

    QRadioButton *m_radioWeChat;
    QRadioButton *m_radioVoucher;

    double m_voucherBalance;
};