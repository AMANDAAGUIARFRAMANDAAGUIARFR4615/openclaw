#pragma once

#include "BitMaskEditorDialog.h"
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

        // ---------- 改动开始 ----------
        // 初始化 32 个分组，长度不一，最长 20 个字符
        groupNames.append("组01");
        groupNames.append("组02-测试机");
        groupNames.append("组03-生产设备");
        groupNames.append("组04-名称最长20字符");
        groupNames.append("组05-备用机");
        groupNames.append("组06-实验室");
        groupNames.append("组07-仓库");
        groupNames.append("组08");
        groupNames.append("组09-远程监控");
        groupNames.append("组10-数据中心");
        groupNames.append("组11");
        groupNames.append("组12-办公区");
        groupNames.append("组13-会议室");
        groupNames.append("组14");
        groupNames.append("组15-安全区域");
        groupNames.append("组16-研发部");
        groupNames.append("组17");
        groupNames.append("组18-财务部");
        groupNames.append("组19-人力资源");
        groupNames.append("组20-行政部");
        groupNames.append("组21-市场部");
        groupNames.append("组22-客服部");
        groupNames.append("组23-运维部");
        groupNames.append("组24");
        groupNames.append("组25-测试环境");
        groupNames.append("组26-正式环境");
        groupNames.append("组27-备份服务器");
        groupNames.append("组28-监控中心");
        groupNames.append("组29-物联网设备");
        groupNames.append("组30-智能家居");
        groupNames.append("组31-边缘计算节点");
        groupNames.append("组32-超长名称测试分组名称最长20字符"); // 20 字符
        // ---------- 改动结束 ----------

        // 示例设备（分组掩码保持原样，仅为演示）
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
    void addDevice(const QString &deviceName, const QString &connectionMode, quint32 groupMask)
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
            quint32 currentMask = groupLabel->property("mask").toUInt();
            showGroupEditor(row, currentMask, groupLabel);
        });

        deviceTable->setCellWidget(row, 2, groupLabel);
        deviceTable->resizeRowToContents(row);
    }

    // 计算分组数量文本
    static QString getGroupCountText(quint32 mask)
    {
        int count = __builtin_popcount(mask);
        return QString("<b>%1</b> 个分组").arg(count);
    }

    // 更新分组标签显示
    void updateGroupLabel(QLabel *label, quint32 mask)
    {
        label->setText(QString("<a href='#'>%1</a>").arg(getGroupCountText(mask)));
        label->setProperty("mask", QVariant(mask));
    }

    void showGroupEditor(int row, quint32 currentMask, QLabel *label)
    {
        QVector<BitMaskEditorDialog::Item> items;
        QVector<int> bits = {0, 2, 5, 7, 15};
        for (int bit : bits) {
            items.append({ bit, groupNames.value(bit, tr("分组%1").arg(bit + 1)) });
        }

        BitMaskEditorDialog dlg(items, currentMask, this);
        if (dlg.exec() != QDialog::Accepted) return;

        updateGroupLabel(label, currentMask);
    }
};
