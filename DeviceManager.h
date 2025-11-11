#pragma once

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QDialog>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QKeyEvent>
#include <QFrame>
#include <QPushButton>
#include <QFont>
#include <QCheckBox>

class DeviceManager : public QWidget {
    Q_OBJECT
public:
    explicit DeviceManager(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("设备管理器");
        setAttribute(Qt::WA_DeleteOnClose);
        resize(1000, 720);

        // 主布局
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(12, 12, 12, 12);
        mainLayout->setSpacing(10);

        // 设备表格
        deviceTable = new QTableWidget(this);
        deviceTable->setColumnCount(3);
        deviceTable->setHorizontalHeaderLabels({"设备名称", "连接方式", "所属分组"});
        deviceTable->setSelectionMode(QAbstractItemView::NoSelection);
        deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        deviceTable->setAlternatingRowColors(true);
        deviceTable->verticalHeader()->setVisible(false);
        deviceTable->horizontalHeader()->setStretchLastSection(true);
        deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

        // 美化样式
        deviceTable->setStyleSheet(R"(
            QTableWidget {
                background-color: #ffffff;
                alternate-background-color: #f9f9fb;
                gridline-color: #e0e0e0;
                border: 1px solid #d0d0d0;
                font-family: "Microsoft YaHei", "Segoe UI", Arial;
                font-size: 14px;
                selection-background-color: #e6f0ff;
            }
            QHeaderView::section {
                background-color: #f0f4f8;
                color: #2c3e50;
                padding: 8px;
                border: none;
                border-bottom: 1px solid #d0d0d0;
                font-weight: bold;
                font-size: 14px;
            }
            QRadioButton, QCheckBox {
                font-size: 13px;
                color: #333333;
            }
            QRadioButton::indicator {
                width: 16px;
                height: 16px;
            }
            QCheckBox::indicator {
                width: 16px;
                height: 16px;
            }
            QLabel[clickable="true"] {
                color: #2980b9;
                text-decoration: underline;
                padding: 4px;
            }
            QLabel[clickable="true"]:hover {
                color: #3498db;
            }
        )");

        mainLayout->addWidget(deviceTable);

        // 初始化32个分组
        for (int i = 0; i < 32; ++i) {
            groupNames.append(QString("组%1").arg(i + 1, 2, 10, QChar('0')));
        }

        // 示例设备
        addDevice("设备A - 测试机", "WIFI优先", 0x0000000F);  // 前4组
        addDevice("设备B - 生产设备", "USB优先", 0xFFFFFFFF);   // 全部
        addDevice("设备C - 备用机", "仅WIFI", 0x00000101);     // 组1 和 组9

        deviceTable->setCurrentItem(nullptr);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape)
            close();
        else
            QWidget::keyPressEvent(event);
    }

private:
    QTableWidget *deviceTable;
    QStringList groupNames;

    // 添加设备：名称、连接方式、32位分组掩码
    void addDevice(const QString &deviceName, const QString &connectionMode, uint32_t groupMask)
    {
        int row = deviceTable->rowCount();
        deviceTable->insertRow(row);

        // 列1：设备名称
        QTableWidgetItem *nameItem = new QTableWidgetItem(deviceName);
        nameItem->setTextAlignment(Qt::AlignCenter);
        deviceTable->setItem(row, 0, nameItem);

        // 列2：连接方式（单选按钮）
        QWidget *modeWidget = new QWidget(this);
        QHBoxLayout *modeLayout = new QHBoxLayout(modeWidget);
        modeLayout->setContentsMargins(8, 4, 8, 4);
        modeLayout->setSpacing(12);

        QStringList modes = {"WIFI优先", "USB优先", "仅WIFI", "仅USB"};
        QButtonGroup *modeGroup = new QButtonGroup(modeWidget);
        modeGroup->setExclusive(true);

        for (const QString &mode : modes) {
            QRadioButton *radio = new QRadioButton(mode, modeWidget);
            radio->setChecked(mode == connectionMode);
            modeLayout->addWidget(radio);
            modeGroup->addButton(radio);
        }
        modeLayout->addStretch();
        deviceTable->setCellWidget(row, 1, modeWidget);

        // 列3：所属分组（显示数量，点击编辑）
        QLabel *groupLabel = new QLabel(this);
        groupLabel->setProperty("clickable", true);
        groupLabel->setCursor(Qt::PointingHandCursor);
        groupLabel->setAlignment(Qt::AlignCenter);
        groupLabel->setTextFormat(Qt::RichText);
        groupLabel->setOpenExternalLinks(false);

        updateGroupLabel(groupLabel, groupMask);
        groupLabel->setProperty("row", row);
        groupLabel->setProperty("mask", QVariant(groupMask));

        connect(groupLabel, &QLabel::linkActivated, this, [this, groupLabel]() {
            int row = groupLabel->property("row").toInt();
            uint32_t currentMask = groupLabel->property("mask").toUInt();
            showGroupEditor(row, currentMask, groupLabel);
        });

        deviceTable->setCellWidget(row, 2, groupLabel);
        deviceTable->resizeRowToContents(row);
    }

    // 计算分组数量文本
    static QString getGroupCountText(uint32_t mask)
    {
        int count = __builtin_popcount(mask);
        return QString("<b>%1</b> 个分组").arg(count);
    }

    // 更新分组标签显示
    void updateGroupLabel(QLabel *label, uint32_t mask)
    {
        label->setText(QString("<a href='#'>%1</a>").arg(getGroupCountText(mask)));
        label->setProperty("mask", QVariant(mask));
    }

    void showGroupEditor(int row, uint32_t currentMask, QLabel *label)
    {
        QDialog dialog(this);
        dialog.setWindowTitle("编辑设备分组");
        dialog.setModal(true);

        QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);
        dialogLayout->setContentsMargins(16, 16, 16, 16);
        dialogLayout->setSpacing(12);

        // 直接使用网格布局作为内容
        QGridLayout *gridLayout = new QGridLayout;
        gridLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->setHorizontalSpacing(16);
        gridLayout->setVerticalSpacing(10);

        QVector<QCheckBox*> checkBoxes(32);
        for (int i = 0; i < 32; ++i) {
            QCheckBox *cb = new QCheckBox(groupNames[i]);
            cb->setChecked(currentMask & (1U << i));
            cb->setStyleSheet("QCheckBox { spacing: 8px; }");
            checkBoxes[i] = cb;

            int col = i % 4;
            int r   = i / 4;
            gridLayout->addWidget(cb, r, col);
        }

        dialogLayout->addLayout(gridLayout);

        // 按钮
        QDialogButtonBox *buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        buttonBox->button(QDialogButtonBox::Ok)->setText("确定");
        buttonBox->button(QDialogButtonBox::Cancel)->setText("取消");

        QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
        okButton->setStyleSheet(R"(
            QPushButton {
                background-color: #3498db;
                color: white;
                border: none;
                padding: 8px 20px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #2980b9; }
            QPushButton:pressed { background-color: #1c6ba0; }
        )");

        dialogLayout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted)
            return;

        // 收集新掩码
        uint32_t newMask = 0;
        for (int i = 0; i < 32; ++i) {
            if (checkBoxes[i]->isChecked())
                newMask |= (1U << i);
        }

        // 更新界面
        updateGroupLabel(label, newMask);
    }
};
