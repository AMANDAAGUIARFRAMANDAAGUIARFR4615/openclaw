#pragma once

#include "MainWindow.h"
#include "Account.h"
#include "Tools.h"
#include "Safe.h"
#include "BaseDialog.h"
#include "EventHub.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
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
#include <QScroller>
#include <QScrollerProperties>
#include <QScrollArea>
#include <QScrollBar>
#include <QButtonGroup>
#include <QDialog>
#include <QVBoxLayout>

class RenewalDialog : public BaseDialog {
    Q_OBJECT

public:
    enum PaymentMethod { Balance, Code }; 

    const int PRICE_WEEKLY = 5;
    const int PRICE_MONTHLY = 10;
    const int PRICE_QUARTERLY = 25;
    const int PRICE_YEARLY = 60;

    explicit RenewalDialog(QWidget *parent) : BaseDialog("续费", parent)
    {
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
        // 桌面端初始大小设置
        resize(560, 700);
#endif

        auto mainLayout = contentLayout();

        // 红色 (强调/警告/价格) - 统一使用全局语义色，自动适配明暗
        alertColor = QColor(Theme::danger());

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

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        QScroller::grabGesture(tableWidget->viewport(), QScroller::LeftMouseButtonGesture);
        QScrollerProperties props = QScroller::scroller(tableWidget->viewport())->scrollerProperties();
        props.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.1);
        QScroller::scroller(tableWidget->viewport())->setScrollerProperties(props);

        tableWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        tableWidget->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
#endif

        auto headerView = tableWidget->horizontalHeader();
        
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
        headerView->setSectionResizeMode(QHeaderView::ResizeToContents);
        headerView->setSectionResizeMode(1, QHeaderView::Stretch);
#endif
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

        // --- 付款方式 ---
        auto paymentGroupBox = new QGroupBox("付款方式");
        auto paymentMainLayout = new QVBoxLayout(paymentGroupBox);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        auto paymentRadiosLayout = new QVBoxLayout(); 
        paymentRadiosLayout->setSpacing(8);
#else
        auto paymentRadiosLayout = new QHBoxLayout();
#endif
        codePayRadioButton = new QRadioButton("兑换码支付");
        balancePayRadioButton = new QRadioButton("余额支付");
        codePayRadioButton->setChecked(true); // 默认兑换码支付
        paymentRadiosLayout->addWidget(codePayRadioButton);
        paymentRadiosLayout->addWidget(balancePayRadioButton);
        paymentRadiosLayout->addStretch();
        paymentMainLayout->addLayout(paymentRadiosLayout);

        // --- 续费周期 ---
        durationGroupBox = new QGroupBox("续费周期");
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        auto durationLayout = new QVBoxLayout(durationGroupBox);
#else
        auto durationLayout = new QHBoxLayout(durationGroupBox);
#endif

        weekRadioButton = new QRadioButton(QString("周付 (¥%1/台)").arg(PRICE_WEEKLY));
        monthRadioButton = new QRadioButton(QString("月付 (¥%1/台)").arg(PRICE_MONTHLY));
        quarterRadioButton = new QRadioButton(QString("季付 (¥%1/台)").arg(PRICE_QUARTERLY));
        yearRadioButton = new QRadioButton(QString("年付 (¥%1/台 - 特惠)").arg(PRICE_YEARLY));
        
        QPalette yearPalette = yearRadioButton->palette();
        yearPalette.setColor(QPalette::WindowText, alertColor);
        yearRadioButton->setPalette(yearPalette);
        QFont yearFont = yearRadioButton->font();
        yearFont.setBold(true);
        yearRadioButton->setFont(yearFont);

        // 使用 QButtonGroup 管理整数 ID，直接绑定 1-4
        durationButtonGroup = new QButtonGroup(this);
        durationButtonGroup->addButton(weekRadioButton, 1);
        durationButtonGroup->addButton(monthRadioButton, 2);
        durationButtonGroup->addButton(quarterRadioButton, 3);
        durationButtonGroup->addButton(yearRadioButton, 4);

        monthRadioButton->setChecked(true);

        durationLayout->addWidget(weekRadioButton);
        durationLayout->addWidget(monthRadioButton);
        durationLayout->addWidget(quarterRadioButton);
        durationLayout->addWidget(yearRadioButton);
        durationLayout->addStretch();

        // --- 兑换码/充值区块 ---
        voucherGroupBox = new QGroupBox("兑换码");
        auto voucherLayout = new QVBoxLayout(voucherGroupBox);

        balanceWidget = new QWidget();
        auto balanceLayout = new QHBoxLayout(balanceWidget);
        balanceLayout->setContentsMargins(0, 0, 0, 0);
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

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        auto redeemLayout = new QVBoxLayout();
#else
        auto redeemLayout = new QHBoxLayout();
#endif
        voucherPlainTextEdit = new QPlainTextEdit();
        voucherPlainTextEdit->setFixedHeight(80);

        queryRedeemButton = new QPushButton("查询信息");
        queryRedeemButton->setFixedHeight(80);

        redeemButton = new QPushButton("批量兑换");
        redeemButton->setFixedHeight(80);

        redeemLayout->addWidget(voucherPlainTextEdit, 1);
        redeemLayout->addWidget(queryRedeemButton);
        redeemLayout->addWidget(redeemButton);

        voucherLayout->addWidget(balanceWidget);
        voucherLayout->addLayout(redeemLayout);

        // --- 应付总额区块 ---
        totalWidget = new QWidget();
        auto totalLayout = new QHBoxLayout(totalWidget);
        totalLayout->setContentsMargins(0, 0, 0, 0);
        auto totalTitleLabel = new QLabel("支付合计:");
        totalTitleLabel->setStyleSheet("font-size: 14px; font-weight: bold;");
        totalAmountLabel = new QLabel("¥ 0");
        
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

        // 组装Options部分
        optionsLayout->addWidget(paymentGroupBox);
        optionsLayout->addWidget(durationGroupBox);
        optionsLayout->addWidget(voucherGroupBox);
        optionsLayout->addWidget(totalWidget);

        // 动态显隐逻辑
        auto updatePaymentUI = [=]() {
            bool isCode = codePayRadioButton->isChecked();
            durationGroupBox->setVisible(!isCode);
            balanceWidget->setVisible(!isCode);
            redeemButton->setVisible(!isCode); // 兑换码直接支付时不需要兑换按钮
            totalWidget->setVisible(!isCode);  // 兑换码直接支付时隐藏"支付合计"

            if (isCode) {
                voucherGroupBox->setTitle("兑换码 (直接抵扣)");
                voucherPlainTextEdit->setPlaceholderText("请输入兑换码，每行一个。数量须与勾选设备数量一致...");
            } else {
                voucherGroupBox->setTitle("代金券充值");
                voucherPlainTextEdit->setPlaceholderText("请输入兑换码，每行一个...");
            }

            updateTotalPrice();
        };

        connect(codePayRadioButton, &QRadioButton::toggled, updatePaymentUI);
        // 初始化一次界面显隐状态
        updatePaymentUI(); 

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttonBox, &QDialogButtonBox::accepted, [this]() {
            QList<QString> selectedIds = getSelectedDeviceIds();
            if (selectedIds.isEmpty()) {
                Tools::showToast(QStringLiteral("请至少选择一台设备"), this);
                return;
            }

            QJsonObject payload;
            payload["ids"] = QJsonArray::fromStringList(selectedIds);

            if (balancePayRadioButton->isChecked()) {
                // 余额支付校验
                if (voucherBalance < currentTotalPrice) {
                    Tools::showToast(QStringLiteral("余额不足"), this);
                    return;
                }
                payload["type"] = durationButtonGroup->checkedId();
                payload["payMethod"] = "balance"; 
            } else {
                // 兑换码支付校验：个数须等于待支付设备数
                QString content = voucherPlainTextEdit->toPlainText();
                QStringList lines = content.split('\n', Qt::SkipEmptyParts);
                QStringList validCodes;
                for (const QString& line : lines) {
                    QString code = line.trimmed();
                    if (!code.isEmpty()) {
                        validCodes.append(code);
                    }
                }

                if (validCodes.isEmpty() || validCodes.size() != selectedIds.size()) {
                    Tools::showToast(QStringLiteral("兑换码个数(%1)须与待支付设备数量(%2)相等")
                                         .arg(validCodes.size()).arg(selectedIds.size()),
                                     this);
                    return;
                }

                payload["codes"] = QJsonArray::fromStringList(validCodes);
                payload["payMethod"] = "code";
            }

            setEnabled(false); 
            setCursor(Qt::WaitCursor);

            // 发起统一的设备续费事件
            webSocketClient->emitEvent("deviceRenew", payload, [=](const QJsonValue &res) {
                setEnabled(true);
                unsetCursor();

                if (res.isString()) {
                    Tools::showToast(res.toString(), this);
                    return;
                }

                if (res.toObject().contains("balance")) {
                    setVoucherBalance(res["balance"].toInt());
                }

                for (const QJsonValue &item : res[HIDE_STR("devices")].toArray()) {
                    auto deviceInfo = DeviceInfo::getDevice(item[HIDE_STR("udid")].toString());
                    if (!deviceInfo) continue;
                    deviceInfo->expireAt = item[HIDE_STR("expireAt")].toInteger();
                    DeviceInfo::expirations[deviceInfo->deviceId] = deviceInfo->expireAt;
                }

                accept();
            });
        });
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        mainLayout->setContentsMargins(20, 20, 20, 20);
        mainLayout->setSpacing(15);

        mainLayout->addLayout(filterLayout);
        mainLayout->addLayout(selectionLayout);
        
        mainLayout->addWidget(tableWidget);
        mainLayout->addStretch(); 

        mainLayout->addLayout(optionsLayout);
        mainLayout->addWidget(buttonBox);

        connect(queryRedeemButton, &QPushButton::clicked, [this]() {
            QString content = voucherPlainTextEdit->toPlainText();
            QStringList lines = content.split('\n', Qt::SkipEmptyParts);
            QStringList codes;
            for (const QString &line : lines) {
                QString t = line.trimmed();
                if (!t.isEmpty())
                    codes.append(t);
            }
            if (codes.isEmpty()) {
                Tools::showToast(QStringLiteral("请先输入要查询的兑换码"), this);
                return;
            }
            const int maxBatch = 100;
            if (codes.size() > maxBatch) {
                Tools::showToast(QStringLiteral("一次最多查询 %1 个兑换码").arg(maxBatch), this);
                return;
            }

            queryRedeemButton->setEnabled(false);
            webSocketClient->emitEvent("query_redeem_code", QJsonArray::fromStringList(codes),
                                       [=](const QJsonValue &res) {
                                           queryRedeemButton->setEnabled(true);
                                           if (!res.isObject()) {
                                               Tools::showToast(QStringLiteral("查询失败"), this);
                                               return;
                                           }
                                           QJsonObject root = res.toObject();
                                           if (root.contains(QStringLiteral("msg"))
                                               && root[QStringLiteral("results")].toArray().isEmpty()) {
                                               Tools::showToast(root[QStringLiteral("msg")].toString(), this);
                                               return;
                                           }
                                           QJsonArray rows = root[QStringLiteral("results")].toArray();
                                           if (rows.isEmpty()) {
                                               Tools::showToast(QStringLiteral("未收到查询结果"), this);
                                               return;
                                           }

                                           auto *dlg = new QDialog(this);
                                           dlg->setWindowTitle(QStringLiteral("兑换码查询结果（%1 条）").arg(rows.size()));
                                           dlg->setAttribute(Qt::WA_DeleteOnClose);
                                           auto *vLay = new QVBoxLayout(dlg);
                                           auto *resultTable = new QTableWidget(dlg);
                                           resultTable->setColumnCount(6);
                                           resultTable->setHorizontalHeaderLabels(
                                               {QStringLiteral("兑换码"), QStringLiteral("面值"), QStringLiteral("续费天数"),
                                                QStringLiteral("状态"), QStringLiteral("兑换时间"), QStringLiteral("兑换账号")});
                                           resultTable->setRowCount(rows.size());
                                           resultTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
                                           resultTable->setSelectionBehavior(QAbstractItemView::SelectRows);
                                           resultTable->setSelectionMode(QAbstractItemView::NoSelection);
                                           resultTable->verticalHeader()->setVisible(false);
                                           resultTable->setWordWrap(false);

                                           for (int r = 0; r < rows.size(); ++r) {
                                               QJsonObject o = rows[r].toObject();
                                               QString code = o[QStringLiteral("code")].toString();
                                               resultTable->setItem(r, 0, new QTableWidgetItem(code));

                                               if (!o[QStringLiteral("found")].toBool()) {
                                                   resultTable->setItem(r, 1, new QTableWidgetItem(QStringLiteral("—")));
                                                   resultTable->setItem(r, 2, new QTableWidgetItem(QStringLiteral("—")));
                                                   resultTable->setItem(
                                                       r, 3,
                                                       new QTableWidgetItem(
                                                           o[QStringLiteral("msg")].toString(QStringLiteral("不存在"))));
                                                   resultTable->setItem(r, 4, new QTableWidgetItem(QStringLiteral("—")));
                                                   resultTable->setItem(r, 5, new QTableWidgetItem(QStringLiteral("—")));
                                                   continue;
                                               }

                                               resultTable->setItem(r, 1,
                                                                    new QTableWidgetItem(o[QStringLiteral("typeLabel")].toString()));
                                               int days = o[QStringLiteral("durationDays")].toInt();
                                               resultTable->setItem(
                                                   r, 2, new QTableWidgetItem(days > 0 ? QString::number(days)
                                                                                       : QStringLiteral("—")));

                                               QString status;
                                               if (o[QStringLiteral("disabled")].toBool())
                                                   status = QStringLiteral("已禁用");
                                               else if (o[QStringLiteral("redeemed")].toBool())
                                                   status = QStringLiteral("已兑换");
                                               else
                                                   status = QStringLiteral("未兑换");
                                               resultTable->setItem(r, 3, new QTableWidgetItem(status));

                                               QString timeStr = QStringLiteral("—");
                                               if (o[QStringLiteral("redeemed")].toBool()) {
                                                   QString raw = o[QStringLiteral("redeemAt")].toString();
                                                   QDateTime dt = QDateTime::fromString(raw, Qt::ISODateWithMs);
                                                   if (!dt.isValid())
                                                       dt = QDateTime::fromString(raw, Qt::ISODate);
                                                   timeStr = dt.isValid()
                                                                 ? dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                                                 : raw;
                                               }
                                               resultTable->setItem(r, 4, new QTableWidgetItem(timeStr));
                                               resultTable->setItem(
                                                   r, 5,
                                                   new QTableWidgetItem(
                                                       o[QStringLiteral("redeemed")].toBool()
                                                           ? o[QStringLiteral("redeemedBy")].toString()
                                                           : QStringLiteral("—")));
                                           }

                                           auto *hdr = resultTable->horizontalHeader();
                                           hdr->setMinimumSectionSize(72);
                                           for (int c = 0; c < 6; ++c)
                                               resultTable->resizeColumnToContents(c);
                                           for (int c = 0; c < 6; ++c)
                                               hdr->setSectionResizeMode(c, QHeaderView::ResizeToContents);
                                           hdr->setStretchLastSection(true);
                                           resultTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

                                           vLay->addWidget(resultTable);
                                           auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
                                           QObject::connect(bb, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
                                           vLay->addWidget(bb);
                                           dlg->resize(880, qMin(520, 48 + rows.size() * 36));
                                           dlg->exec();
                                       });
        });

        // 恢复原有兑换按钮的逻辑，仅在“余额支付”下点击时充值余额
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
                    Tools::showToast(res["msg"].toString(), this);
                    voucherPlainTextEdit->clear();
                });
            }
        });

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
        
        connect(weekRadioButton, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);
        connect(monthRadioButton, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);
        connect(quarterRadioButton, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);
        connect(yearRadioButton, &QRadioButton::toggled, this, &RenewalDialog::updateTotalPrice);

        filterComboBox->setCurrentIndex(-1);
        filterComboBox->setCurrentIndex(MainWindow::getInstance()->tabWidget->currentIndex());

        setVoucherBalance(Account::getInstance()->balance);

        EventHub::on(this, "balanceUpdated", [this](const QJsonValue &data, DeviceConnection*) {
            setVoucherBalance(data["balance"].toInt());
        });
    }

    ~RenewalDialog() override {
        EventHub::off(this, "balanceUpdated");
    }

protected:
    
    void adjustTableHeight() {
        int actualRows = tableWidget->rowCount();
        int displayRows = actualRows == 0 ? 1 : qMin(actualRows, 5);

        int totalHeight = tableWidget->horizontalHeader()->height() + 2; 

        for (int i = 0; i < displayRows; ++i) {
            totalHeight += tableWidget->rowHeight(i);
        }

        totalHeight += tableWidget->horizontalScrollBar()->sizeHint().height();

        tableWidget->setFixedHeight(totalHeight);
    }

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

            auto expireAt = QDateTime::fromMSecsSinceEpoch(deviceInfo->expireAt.get()).toString(HIDE_STR("yyyy-MM-dd HH:mm:ss"));
            auto expireItem = new QTableWidgetItem(expireAt);
            tableWidget->setItem(i, 3, expireItem);

            if (deviceInfo->expireAt.get() < currentTimestamp)
                expireItem->setForeground(QBrush(alertColor));
        }

        tableWidget->blockSignals(false);
        updateSelectAllState();
        updateTotalPrice();
        
        adjustTableHeight();
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

    PaymentMethod getPaymentMethod() const {
        return balancePayRadioButton->isChecked() ? Balance : Code;
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
        if (balanceLabel) {
            balanceLabel->setText(QString("¥%1").arg(QString::number(voucherBalance)));
        }
    }

    void updateTotalPrice() {
        // 如果是兑换码支付模式，不需要计算金额，直接返回
        if (codePayRadioButton && codePayRadioButton->isChecked()) {
            currentTotalPrice = 0;
            return;
        }

        int selectedCount = 0;
        for (int i = 0; i < tableWidget->rowCount(); ++i) {
            if (tableWidget->item(i, 0)->checkState() == Qt::Checked) {
                selectedCount++;
            }
        }

        // 余额支付模式下，通过选中的ID (1-4) 直接映射单价数组，索引 0 占位
        int unitPrice = (int[]){ 0, PRICE_WEEKLY, PRICE_MONTHLY, PRICE_QUARTERLY, PRICE_YEARLY }[durationButtonGroup->checkedId()];

        currentTotalPrice = unitPrice * selectedCount;
        totalAmountLabel->setText(QString("¥%1").arg(QString::number(currentTotalPrice)));
    }

    QTableWidget *tableWidget;
    QComboBox *filterComboBox;
    QCheckBox *expiredFilterCheckBox;
    QCheckBox *selectAllCheckBox;
    
    QGroupBox *durationGroupBox;
    QButtonGroup *durationButtonGroup; 
    QRadioButton *weekRadioButton;
    QRadioButton *monthRadioButton;
    QRadioButton *quarterRadioButton;
    QRadioButton *yearRadioButton;
    
    QGroupBox *voucherGroupBox;
    QPlainTextEdit *voucherPlainTextEdit;
    QPushButton *queryRedeemButton;
    QPushButton *redeemButton;
    
    QWidget *balanceWidget;
    QLabel *balanceLabel;
    QWidget *totalWidget; 

    QRadioButton *codePayRadioButton;
    QRadioButton *balancePayRadioButton;
    QLabel *totalAmountLabel;

    int voucherBalance = 0;
    int currentTotalPrice = 0;

    QColor alertColor;
};