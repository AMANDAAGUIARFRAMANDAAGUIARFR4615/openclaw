#pragma once

#include "BaseDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QStringList>
#include <QLabel>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QStyleHints>
#include <QSpinBox>

// ============================================================================
// 网段列表项 Widget
// ============================================================================
class NetworkSegmentItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit NetworkSegmentItemWidget(const QString& baseIp, int startVal, int endVal, QWidget* parent = nullptr)
        : QWidget(parent), m_baseIp(baseIp)
    {
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        const int cellWidth = 32;
        const int cellHeight = 30;
        const int spinWidth = 34;
        const int dotWidth = 3;
        const int dashWidth = 12;
        const int itemSpacing = 4;
        const int ipGroupSpacing = 2;
        const int sideMargin = 3;
        const int dotFontSize = 13;
#else
        const int cellWidth = 40;
        const int cellHeight = 32;
        const int spinWidth = 42;
        const int dotWidth = 6;
        const int dashWidth = 24;
        const int itemSpacing = 12;
        const int ipGroupSpacing = 4;
        const int sideMargin = 16;
        const int dotFontSize = 16;
#endif

        setAttribute(Qt::WA_StyledBackground, false);
        auto mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(sideMargin, 8, sideMargin, 8);
        mainLayout->setSpacing(itemSpacing);

        QStringList parts = baseIp.split('.', Qt::SkipEmptyParts);
        while (parts.size() < 3) parts.append("0");

        auto createDot = [this]() {
            auto lbl = new QLabel(".", this);
            lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(QString("font-weight: bold; font-size: %1px;").arg(dotFontSize));
            lbl->setFixedWidth(dotWidth);
            return lbl;
        };

        auto createReadOnlyBox = [this](const QString& text) {
            auto le = new QLineEdit(text, this);
            le->setReadOnly(true);
            le->setAlignment(Qt::AlignCenter);
            le->setFixedSize(cellWidth, cellHeight);
            le->setFocusPolicy(Qt::NoFocus);
            le->setAttribute(Qt::WA_TransparentForMouseEvents);
            return le;
        };

        auto createSpinBox = [this](int value) {
            auto sb = new QSpinBox(this);
            sb->setRange(1, 254);
            sb->setValue(value);
            sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
            sb->setAlignment(Qt::AlignCenter);
            sb->setFixedSize(spinWidth, cellHeight);
            return sb;
        };

        auto createIPGroup = [&](QSpinBox*& editOut, int val) {
            auto layout = new QHBoxLayout();
            layout->setSpacing(ipGroupSpacing);
            layout->addWidget(createReadOnlyBox(parts[0]));
            layout->addWidget(createDot());
            layout->addWidget(createReadOnlyBox(parts[1]));
            layout->addWidget(createDot());
            layout->addWidget(createReadOnlyBox(parts[2]));
            layout->addWidget(createDot());

            editOut = createSpinBox(val);
            layout->addWidget(editOut);
            return layout;
        };

        mainLayout->addStretch();

        mainLayout->addLayout(createIPGroup(m_startEdit, startVal));

        auto lblDash = new QLabel("~", this);
        lblDash->setAttribute(Qt::WA_TransparentForMouseEvents);
        lblDash->setAlignment(Qt::AlignCenter);
        lblDash->setFixedWidth(dashWidth);
        mainLayout->addWidget(lblDash);

        mainLayout->addLayout(createIPGroup(m_endEdit, endVal));
        mainLayout->addStretch();
    }

    QString getBaseIp() const { return m_baseIp; }

    QString getRangeString() const {
        int s = qMin(m_startEdit->value(), m_endEdit->value());
        int e = qMax(m_startEdit->value(), m_endEdit->value());
        return QString("%1%2-%1%3").arg(m_baseIp).arg(s).arg(e);
    }

    QStringList getAllIPs() const {
        QStringList ips;
        int s = qMin(m_startEdit->value(), m_endEdit->value());
        int e = qMax(m_startEdit->value(), m_endEdit->value());
        // 遍历并生成每一个 IP
        for (int i = s; i <= e; ++i) {
            ips.append(m_baseIp + QString::number(i));
        }
        return ips;
    }

private:
    QString m_baseIp;
    QSpinBox* m_startEdit;
    QSpinBox* m_endEdit;
};

// ============================================================================
// 网段扫描配置主对话框
// ============================================================================
class NetworkSegmentEditorDialog : public BaseDialog {
    Q_OBJECT

public:
    explicit NetworkSegmentEditorDialog(QWidget *parent = nullptr) : BaseDialog(tr("网段扫描配置"), parent) {
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
        resize(560, 600);
#endif

        setupUI();
        loadData();
    }

    static QStringList getAllIPs() {
        QStringList ips;

        // 1. 读取保存的配置 (格式: "192.168.1.2-192.168.1.254")
        if (settings->contains("Network/Segments")) {
            for (const QString& range : settings->value("Network/Segments").toStringList()) {
                QStringList bounds = range.split('-');
                if (bounds.size() == 2) {
                    QString base = bounds[0].left(bounds[0].lastIndexOf('.') + 1); // "192.168.1."
                    int start = bounds[0].mid(bounds[0].lastIndexOf('.') + 1).toInt();
                    int end = bounds[1].mid(bounds[1].lastIndexOf('.') + 1).toInt();
                    
                    for (int i = qMin(start, end); i <= qMax(start, end); ++i) {
                        ips << base + QString::number(i);
                    }
                }
            }
            return ips;
        }

        // 2. 如果没配置过，降级使用默认网卡 IP 展开
        for (const QString& ip : NetworkUtils::getPhysicalIPs()) {
            QString base = ip.left(ip.lastIndexOf('.') + 1);
            if (!base.isEmpty()) {
                for (int i = 2; i <= 254; ++i) {
                    ips << base + QString::number(i);
                }
            }
        }
        return ips;
    }

private:
    QListWidget* m_listWidget;
    QLineEdit* m_ipInput;

    void setupUI() {
        auto mainLayout = contentLayout();
        mainLayout->setSpacing(16);
        mainLayout->setContentsMargins(16, 20, 16, 20);

        m_listWidget = new QListWidget(this);
        m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_listWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        m_listWidget->setMinimumHeight(260);
#endif
        mainLayout->addWidget(m_listWidget, 1);

        auto inputLayout = new QHBoxLayout();
        m_ipInput = new QLineEdit(this);
        m_ipInput->setObjectName("mainIpInput");
        m_ipInput->setMinimumHeight(38);
        m_ipInput->setPlaceholderText(tr("输入要连接的手机IP (如 192.168.1.1)，自动提取并生成网段"));

        auto btnAdd = new QPushButton(tr("确认添加"), this);
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        btnAdd->setMinimumSize(96, 42);
#else
        btnAdd->setFixedSize(90, 38);
#endif
        inputLayout->addWidget(m_ipInput, 1);
        inputLayout->addWidget(btnAdd);
        mainLayout->addLayout(inputLayout);

        auto actionLayout = new QHBoxLayout();
        actionLayout->setSpacing(12);

        auto btnDelete = new QPushButton(tr("删除选中"), this);
        auto btnReset = new QPushButton(tr("恢复默认"), this);
        auto btnSave = new QPushButton(tr("保存配置"), this);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        btnDelete->setMinimumHeight(42);
        btnReset->setMinimumHeight(42);
        btnSave->setMinimumHeight(42);
        btnSave->setMinimumWidth(120);
#else
        btnDelete->setFixedSize(90, 38);
        btnReset->setFixedSize(90, 38);
        btnSave->setFixedSize(120, 38);
#endif
        btnSave->setObjectName("btnSave");

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        actionLayout->addWidget(btnDelete, 1);
        actionLayout->addWidget(btnReset, 1);
        actionLayout->addWidget(btnSave, 1);
#else
        actionLayout->addWidget(btnDelete);
        actionLayout->addWidget(btnReset);
        actionLayout->addStretch();
        actionLayout->addWidget(btnSave);
#endif
        mainLayout->addLayout(actionLayout);

        // 信号槽逻辑
        auto addLogic = [this]() {
            QString input = m_ipInput->text().trimmed();
            if (input.isEmpty()) return;

            QRegularExpression re("^(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})\\.\\d{1,3}$");
            auto match = re.match(input);
            if (!match.hasMatch()) {
                QMessageBox::warning(this, tr("格式错误"), tr("请输入有效的 IP 地址 (如 192.168.1.1)"));
                return;
            }

            QString baseIp = match.captured(1) + ".";
            if (isSegmentExist(baseIp)) {
                QMessageBox::information(this, tr("提示"), tr("该网段已存在列表中。"));
                return;
            }

            addSegmentItem(baseIp, 2, 254);
            m_ipInput->clear();
            m_listWidget->scrollToBottom();
        };

        connect(btnAdd, &QPushButton::clicked, addLogic);
        connect(m_ipInput, &QLineEdit::returnPressed, addLogic);
        connect(btnDelete, &QPushButton::clicked, this, [this](){ qDeleteAll(m_listWidget->selectedItems()); });
        connect(btnReset, &QPushButton::clicked, this, &NetworkSegmentEditorDialog::resetToDefault);
        connect(btnSave, &QPushButton::clicked, this, [this]() {
            saveData();
            accept();
        });

        applyDynamicStyle();
    }

    bool isSegmentExist(const QString& baseIp) const {
        for (int i = 0; i < m_listWidget->count(); ++i) {
            auto widget = qobject_cast<NetworkSegmentItemWidget*>(m_listWidget->itemWidget(m_listWidget->item(i)));
            if (widget && widget->getBaseIp() == baseIp) return true;
        }
        return false;
    }

    void addSegmentItem(const QString& baseIp, int start, int end) {
        auto item = new QListWidgetItem(m_listWidget);
        auto widget = new NetworkSegmentItemWidget(baseIp, start, end, m_listWidget);
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        item->setSizeHint(QSize(0, 54));
#else
        item->setSizeHint(QSize(0, 64));
#endif
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, widget);
    }

    void saveData() const {
        QStringList compactRanges;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            if (auto widget = qobject_cast<NetworkSegmentItemWidget*>(m_listWidget->itemWidget(m_listWidget->item(i)))) {
                compactRanges.append(widget->getRangeString());
            }
        }
        settings->setValue("Network/Segments", compactRanges);
    }

    void loadData() {
        if (settings->contains("Network/Segments")) {
            m_listWidget->clear();
            for (const QString& line : settings->value("Network/Segments").toStringList()) {
                QStringList ips = line.split('-'); // line: "192.168.1.2-192.168.1.254"
                if (ips.size() == 2) {
                    int lastDot = ips[0].lastIndexOf('.');
                    if (lastDot != -1) {
                        QString baseIp = ips[0].left(lastDot + 1);
                        int startVal = ips[0].mid(lastDot + 1).toInt();
                        int endVal = ips[1].mid(ips[1].lastIndexOf('.') + 1).toInt();
                        addSegmentItem(baseIp, startVal, endVal);
                    }
                }
            }
        } else {
            resetToDefault();
        }
    }

    void resetToDefault() {
        m_listWidget->clear();
        QRegularExpression re("^(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})\\.");

        for (const QString& ip : NetworkUtils::getPhysicalIPs()) {
            auto match = re.match(ip);
            if (match.hasMatch()) {
                addSegmentItem(match.captured(1) + ".", 2, 254);
            }
        }
    }

    void applyDynamicStyle() {
        bool isDark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;

        QString winBg      = isDark ? "#252526" : "#F5F5F7";
        QString panelBg    = isDark ? "#2D2D30" : "#FFFFFF";
        QString textColor  = isDark ? "#E0E0E0" : "#333333";
        QString subText    = isDark ? "#888888" : "#999999";
        QString border     = isDark ? "#454545" : "#DCDCDC";
        QString accent     = isDark ? "#007ACC" : "#0078D7";
        QString itemBg     = isDark ? "#333337" : "#FFFFFF";
        QString itemHover  = isDark ? "#3E3E42" : "#F7F9FA";
        QString itemSel    = isDark ? "#3F3F46" : "#EDF4FC";
        QString readOnlyBg = isDark ? "#2A2A2D" : "#F8F8F8";
        QString editBg     = isDark ? "#1E1E1E" : "#FFFFFF";
        QString btnBg      = isDark ? "#3E3E42" : "#FFFFFF";

        QString css = QString(R"(
            NetworkSegmentEditorDialog { background-color: %1; color: %2; font-family: 'Segoe UI', Arial; font-size: 13px; }
            QLabel { color: %2; }

            QListWidget { background: transparent; border: none; outline: none; padding: 4px; }
            QListWidget::item { background: %7; border: 1px solid %5; border-radius: 6px; margin: 5px 8px; }
            QListWidget::item:hover { background: %8; border-color: %5; }
            QListWidget::item:selected { background: %9; border: 1px solid %6; }

            QLineEdit#mainIpInput { border: 1px solid %5; border-radius: 4px; padding: 6px 10px; background: %3; color: %2; }
            QLineEdit#mainIpInput:focus { border: 1px solid %6; }
            QLineEdit::placeholder { color: %4; }

            QSpinBox { border: 1px solid %5; border-radius: 4px; background: %11; color: %2; }
            QSpinBox:focus { border: 2px solid %6; padding: -1px; }
            QLineEdit[readOnly="true"] { border: 1px solid %5; border-radius: 4px; background: %10; color: %4; }

            QPushButton { border: 1px solid %5; border-radius: 4px; background: %12; color: %2; outline: none; }
            QPushButton:hover { background: %8; }
            QPushButton:pressed { background: %5; }

            QPushButton#btnSave { background-color: #28a745; color: white; border: none; font-weight: bold; font-size: 14px; }
            QPushButton#btnSave:hover { background-color: #218838; }
        )").arg(winBg, textColor, panelBg, subText, border)
                          .arg(accent, itemBg, itemHover, itemSel, readOnlyBg)
                          .arg(editBg, btnBg);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        css += QString(R"(
            QListWidget { padding: 2px; }
            QListWidget::item { margin: 3px 0px; }
            QLineEdit#mainIpInput { min-height: 42px; font-size: 14px; }
            QSpinBox { min-height: 30px; font-size: 13px; }
            QLineEdit[readOnly="true"] { font-size: 13px; padding: 3px 4px; }
            QPushButton { min-height: 42px; font-size: 14px; }
        )");
#endif

        this->setStyleSheet(css);
    }
};
