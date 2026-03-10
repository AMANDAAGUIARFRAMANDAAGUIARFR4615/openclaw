#pragma once

#include <QDialog>
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

// --- 自定义列表项 Widget (完美对齐的 IP 框组) ---
class NetworkSegmentItemWidget : public QWidget {
    Q_OBJECT
public:
    NetworkSegmentItemWidget(const QString& baseIp, int startVal, int endVal, QWidget* parent = nullptr)
        : QWidget(parent), m_baseIp(baseIp)
    {
        auto mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(16, 10, 16, 10);
        mainLayout->setSpacing(12);
        this->setAttribute(Qt::WA_StyledBackground, false);

        // 解析基础 IP (例如 "192.168.1." -> ["192", "168", "1"])
        QStringList parts = baseIp.split('.', Qt::SkipEmptyParts);
        if (parts.size() < 3) parts = {"0", "0", "0"}; // 兜底

        // 辅助函数：创建连接的 "点"
        auto createDot = [this]() {
            auto lbl = new QLabel(".", this);
            lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            lbl->setAlignment(Qt::AlignCenter);
            QFont f = lbl->font();
            f.setBold(true);
            f.setPointSize(12);
            lbl->setFont(f);
            lbl->setFixedWidth(6);
            return lbl;
        };

        // 辅助函数：创建前三段的只读输入框
        auto createReadOnlyBox = [this](const QString& text) {
            auto le = new QLineEdit(text, this);
            le->setReadOnly(true);
            le->setAlignment(Qt::AlignCenter);
            le->setFixedSize(40, 32);
            le->setFocusPolicy(Qt::NoFocus); // 禁止获取焦点
            // 鼠标穿透，点击时直接选中列表项
            le->setAttribute(Qt::WA_TransparentForMouseEvents);
            return le;
        };

        // 辅助函数：创建最后一段的可编辑数字框
        auto createSpinBox = [this](int value) {
            auto sb = new QSpinBox(this);
            // 限制范围为 1~254，彻底排除 0 和 255
            sb->setRange(1, 254);
            sb->setValue(value);
            sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
            sb->setAlignment(Qt::AlignCenter);
            sb->setFixedSize(42, 32);
            return sb;
        };

        // 辅助函数：生成一组完整的 IP 输入布局
        auto createIPGroup = [&](QSpinBox*& editOut, int val) {
            auto layout = new QHBoxLayout();
            layout->setSpacing(4);
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

        // 1. 起始 IP 组
        mainLayout->addLayout(createIPGroup(m_startEdit, startVal));

        // 2. 波浪号分隔符
        auto labelDash = new QLabel("~", this);
        labelDash->setAttribute(Qt::WA_TransparentForMouseEvents);
        labelDash->setAlignment(Qt::AlignCenter);
        labelDash->setFixedWidth(24);
        mainLayout->addWidget(labelDash);

        // 3. 结束 IP 组
        mainLayout->addLayout(createIPGroup(m_endEdit, endVal));

        mainLayout->addStretch();
    }

    QString getBaseIp() const { return m_baseIp; }

    QString getFullRange() const {
        int start = qMin(m_startEdit->value(), m_endEdit->value());
        int end = qMax(m_startEdit->value(), m_endEdit->value());
        return QString("%1%2 - %1%3").arg(m_baseIp).arg(start).arg(end);
    }

private:
    QString m_baseIp;
    QSpinBox* m_startEdit;
    QSpinBox* m_endEdit;
};


// --- 主对话框 ---
class NetworkSegmentEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NetworkSegmentEditorDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(tr("网段扫描配置"));
        resize(560, 600);

        setupUI();
        loadData();
    }

    QStringList getSegments() const {
        QStringList list;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            auto item = m_listWidget->item(i);
            auto widget = qobject_cast<NetworkSegmentItemWidget*>(m_listWidget->itemWidget(item));
            if (widget) {
                list.append(widget->getFullRange());
            }
        }
        return list;
    }

private:
    QListWidget* m_listWidget;
    QLineEdit* m_ipInput;
    QPushButton* m_btnSave;

    void applyDynamicStyle() {
        bool isDark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;

        // 颜色定义
        QString winBg = isDark ? "#252526" : "#F5F5F7"; // 浅色模式背景稍微柔和一点
        QString panelBg = isDark ? "#2D2D30" : "#FFFFFF";
        QString textColor = isDark ? "#E0E0E0" : "#333333";
        QString subTextColor = isDark ? "#888888" : "#999999";
        QString borderColor = isDark ? "#454545" : "#DCDCDC";
        QString accentColor = isDark ? "#007ACC" : "#0078D7";

        // Item 颜色
        QString itemBg = isDark ? "#333337" : "#FFFFFF";
        QString itemHoverBg = isDark ? "#3E3E42" : "#F7F9FA";
        QString itemSelectedBg = isDark ? "#3F3F46" : "#EDF4FC";

        // 输入框颜色区分
        QString readOnlyBg = isDark ? "#2A2A2D" : "#F8F8F8"; // 亮色模式下只读框稍灰
        QString readOnlyColor = isDark ? "#999999" : "#888888";
        QString editBg = isDark ? "#1E1E1E" : "#FFFFFF";

        // 按钮颜色
        QString btnBg = isDark ? "#3E3E42" : "#FFFFFF";
        QString btnHoverBg = isDark ? "#4D4D52" : "#F0F0F0";
        QString saveBtnBg = "#28a745";
        QString saveBtnHoverBg = "#218838";

        QString styleSheet;
        styleSheet += QString("QDialog { background-color: %1; color: %2; font-family: 'Segoe UI', Arial, sans-serif; font-size: 13px; }").arg(winBg, textColor);
        styleSheet += QString("QLabel { color: %1; }").arg(textColor);

        // 列表样式
        styleSheet += QString(
                          "QListWidget { background-color: transparent; border: none; outline: none; padding: 4px; }"
                          "QListWidget::item { background-color: %1; border: 1px solid %2; border-radius: 6px; margin: 5px 8px; }"
                          "QListWidget::item:hover { background-color: %3; border-color: %5; }"
                          "QListWidget::item:selected { background-color: %4; border: 1px solid %6; }"
                          ).arg(itemBg, borderColor, itemHoverBg, itemSelectedBg, borderColor, accentColor);

        // 底部主输入框样式
        styleSheet += QString(
                          "QLineEdit#mainIpInput {"
                          "   border: 1px solid %1; border-radius: 4px; padding: 6px 10px; background: %2; color: %3;"
                          "}"
                          "QLineEdit#mainIpInput:focus { border: 1px solid %4; }"
                          "QLineEdit::placeholder { color: %5; }"
                          ).arg(borderColor, panelBg, textColor, accentColor, subTextColor);

        // 小框样式
        styleSheet += QString(
                          "QSpinBox {"
                          "   border: 1px solid %1; border-radius: 4px; background: %2; color: %3;"
                          "}"
                          "QSpinBox:focus { border: 2px solid %4; padding: -1px; }"

                          "QLineEdit[readOnly=\"true\"] {"
                          "   border: 1px solid %1; border-radius: 4px; background: %5; color: %6;"
                          "}"
                          ).arg(borderColor, editBg, textColor, accentColor, readOnlyBg, readOnlyColor);

        // 普通按钮样式 (增加左右内边距保证文字不拥挤)
        styleSheet += QString(
                          "QPushButton { border: 1px solid %1; border-radius: 4px; background-color: %2; color: %3; outline: none; }"
                          "QPushButton:hover { background-color: %4; }"
                          "QPushButton:pressed { background-color: %1; }"
                          ).arg(borderColor, btnBg, textColor, btnHoverBg);

        // 保存按钮特殊样式
        m_btnSave->setStyleSheet(QString(
                                     "QPushButton { background-color: %1; color: white; border: none; font-weight: bold; font-size: 14px; }"
                                     "QPushButton:hover { background-color: %2; }"
                                     ).arg(saveBtnBg, saveBtnHoverBg));

        this->setStyleSheet(styleSheet);
    }

    void setupUI() {
        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(16);
        mainLayout->setContentsMargins(16, 20, 16, 20);

        // 1. 列表展示区
        m_listWidget = new QListWidget(this);
        m_listWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_listWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
        mainLayout->addWidget(m_listWidget, 1);

        // 2. 输入区
        auto inputLayout = new QHBoxLayout();
        m_ipInput = new QLineEdit(this);
        m_ipInput->setObjectName("mainIpInput");
        m_ipInput->setFixedHeight(38);
        m_ipInput->setPlaceholderText(tr("输入基础 IP (如 192.168.1.1)，自动提取并生成网段"));

        auto btnAdd = new QPushButton(tr("确认添加"), this);
        btnAdd->setFixedSize(90, 38);

        inputLayout->addWidget(m_ipInput, 1);
        inputLayout->addWidget(btnAdd);
        mainLayout->addLayout(inputLayout);

        // 3. 底部操作区
        auto actionLayout = new QHBoxLayout();
        actionLayout->setSpacing(12); // 给底部按钮之间加一点合理的间距

        auto btnDelete = new QPushButton(tr("删除选中"), this);
        btnDelete->setFixedSize(90, 38); // 统一固定大小，解决拥挤问题

        auto btnReset = new QPushButton(tr("恢复默认"), this);
        btnReset->setFixedSize(90, 38);  // 统一固定大小，解决拥挤问题

        m_btnSave = new QPushButton(tr("保存配置"), this);
        m_btnSave->setFixedSize(120, 38);

        actionLayout->addWidget(btnDelete);
        actionLayout->addWidget(btnReset);
        actionLayout->addStretch();
        actionLayout->addWidget(m_btnSave);
        mainLayout->addLayout(actionLayout);

        // --- 信号槽 ---
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

            for (int i = 0; i < m_listWidget->count(); ++i) {
                auto item = m_listWidget->item(i);
                auto widget = qobject_cast<NetworkSegmentItemWidget*>(m_listWidget->itemWidget(item));
                if (widget && widget->getBaseIp() == baseIp) {
                    QMessageBox::information(this, tr("提示"), tr("该网段已存在列表中。"));
                    return;
                }
            }

            // 新增网段时，默认范围为 2 ~ 254
            addSegmentItem(baseIp, 2, 254);
            m_ipInput->clear();
            m_listWidget->scrollToBottom();
        };

        connect(btnAdd, &QPushButton::clicked, addLogic);
        connect(m_ipInput, &QLineEdit::returnPressed, addLogic);
        connect(btnDelete, &QPushButton::clicked, this, [this](){ qDeleteAll(m_listWidget->selectedItems()); });
        connect(btnReset, &QPushButton::clicked, this, &NetworkSegmentEditorDialog::resetToDefault);
        connect(m_btnSave, &QPushButton::clicked, this, &QDialog::accept);

        applyDynamicStyle();
    }

    void addSegmentItem(const QString& baseIp, int start, int end) {
        auto item = new QListWidgetItem(m_listWidget);
        auto widget = new NetworkSegmentItemWidget(baseIp, start, end, m_listWidget);
        item->setSizeHint(QSize(0, 64));
        m_listWidget->addItem(item);
        m_listWidget->setItemWidget(item, widget);
    }

    void loadData() {
        resetToDefault();
    }

    void resetToDefault() {
        m_listWidget->clear();
        QRegularExpression re("^(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})\\.");
        for (const QString& ip : NetworkUtils::getPhysicalIPs()) {
            auto match = re.match(ip);
            addSegmentItem(match.captured(1) + ".", 2, 254);
        }
    }
};
