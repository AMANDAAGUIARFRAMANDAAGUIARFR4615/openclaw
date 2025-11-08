#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QScrollArea>
#include <QGroupBox>
#include <QSplitter>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>

class DeviceManagerUI : public QWidget {
    Q_OBJECT
public:
    DeviceManagerUI(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("设备管理系统");
        resize(1150, 700);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(8);

        // 上下布局
        auto *splitter = new QSplitter(Qt::Vertical, this);
        mainLayout->addWidget(splitter);

        // === 分组管理 ===
        auto *groupFrame = new QFrame(this);
        auto *groupLayout = new QVBoxLayout(groupFrame);
        groupLayout->setContentsMargins(8, 8, 8, 8);
        groupLayout->setSpacing(6);

        auto *groupTitle = new QLabel("分组管理");
        QFont titleFont;
        titleFont.setBold(true);
        titleFont.setPointSize(11);
        groupTitle->setFont(titleFont);
        groupLayout->addWidget(groupTitle);

        // 分组复选框区（滚动）
        groupScroll = new QScrollArea(this);
        groupScroll->setWidgetResizable(true);
        groupContainer = new QWidget(this);
        groupContainerLayout = new QHBoxLayout(groupContainer);
        groupContainerLayout->setAlignment(Qt::AlignLeft);
        groupContainerLayout->setContentsMargins(5, 5, 5, 5);
        groupScroll->setWidget(groupContainer);
        groupLayout->addWidget(groupScroll, 1);

        // 添加/删除分组
        auto *ctrlLayout = new QHBoxLayout();
        groupEdit = new QLineEdit(this);
        groupEdit->setPlaceholderText("输入分组名称");
        addGroupBtn = new QPushButton("➕ 添加分组", this);
        removeGroupBtn = new QPushButton("🗑 删除选中分组", this);
        ctrlLayout->addWidget(groupEdit);
        ctrlLayout->addWidget(addGroupBtn);
        ctrlLayout->addWidget(removeGroupBtn);
        groupLayout->addLayout(ctrlLayout);

        splitter->addWidget(groupFrame);

        // === 设备管理 ===
        auto *deviceFrame = new QFrame(this);
        auto *deviceLayout = new QVBoxLayout(deviceFrame);
        deviceLayout->setContentsMargins(8, 8, 8, 8);
        deviceLayout->setSpacing(6);

        auto *deviceTitle = new QLabel("设备管理");
        deviceTitle->setFont(titleFont);
        deviceLayout->addWidget(deviceTitle);

        deviceTable = new QTableWidget(this);
        deviceTable->setColumnCount(3);
        deviceTable->setHorizontalHeaderLabels({"设备名", "连接方式", "所属分组"});
        deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
        deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        deviceTable->verticalHeader()->setVisible(false);
        deviceTable->setAlternatingRowColors(true);
        deviceTable->setStyleSheet("QTableWidget { selection-background-color: #b0d8ff; }");
        deviceLayout->addWidget(deviceTable, 1);

        splitter->addWidget(deviceFrame);
        splitter->setStretchFactor(1, 2);

        // === 初始化分组与设备 ===
        for (int i = 1; i <= 16; ++i)
            addGroup(QString("组%1").arg(i));

        addDevice("设备A", "WIFI优先");
        addDevice("设备B", "USB优先");
        addDevice("设备C", "仅WIFI");
        addDevice("设备D", "仅USB");

        // === 信号槽 ===
        connect(addGroupBtn, &QPushButton::clicked, this, &DeviceManagerUI::onAddGroupClicked);
        connect(removeGroupBtn, &QPushButton::clicked, this, &DeviceManagerUI::onRemoveGroupClicked);
    }

private:
    QLineEdit *groupEdit;
    QPushButton *addGroupBtn;
    QPushButton *removeGroupBtn;
    QScrollArea *groupScroll;
    QWidget *groupContainer;
    QHBoxLayout *groupContainerLayout;
    QTableWidget *deviceTable;
    QStringList groups;

    // ===== 分组逻辑 =====
    void addGroup(const QString &name) {
        if (name.trimmed().isEmpty() || groups.contains(name)) return;
        groups << name;
        auto *cb = new QCheckBox(name, this);
        cb->setChecked(true);
        groupContainerLayout->addWidget(cb);
        updateDeviceGroupColumns();
    }

    void removeGroup(const QString &name) {
        for (int i = 0; i < groupContainerLayout->count(); ++i) {
            auto *w = groupContainerLayout->itemAt(i)->widget();
            if (auto *cb = qobject_cast<QCheckBox *>(w)) {
                if (cb->text() == name) {
                    groups.removeAll(name);
                    cb->deleteLater();
                    break;
                }
            }
        }
        updateDeviceGroupColumns();
    }

    void updateDeviceGroupColumns() {
        for (int row = 0; row < deviceTable->rowCount(); ++row) {
            QWidget *old = deviceTable->cellWidget(row, 2);
            if (old) old->deleteLater();

            QWidget *groupWidget = new QWidget(this);
            auto *layout = new QHBoxLayout(groupWidget);
            layout->setAlignment(Qt::AlignLeft);
            layout->setContentsMargins(0, 0, 0, 0);

            for (const QString &g : groups) {
                QCheckBox *cb = new QCheckBox(g, this);
                layout->addWidget(cb);
            }
            deviceTable->setCellWidget(row, 2, groupWidget);
        }

        // 让表格列宽自适应复选框内容
        deviceTable->resizeColumnsToContents();
    }

    // ===== 设备逻辑 =====
    void addDevice(const QString &name, const QString &connMode) {
        int row = deviceTable->rowCount();
        deviceTable->insertRow(row);
        deviceTable->setItem(row, 0, new QTableWidgetItem(name));

        // 连接方式 - 单选按钮组
        QWidget *modeWidget = new QWidget(this);
        auto *modeLayout = new QHBoxLayout(modeWidget);
        modeLayout->setAlignment(Qt::AlignLeft);
        modeLayout->setContentsMargins(0, 0, 0, 0);

        QStringList modes = {"WIFI优先", "USB优先", "仅WIFI", "仅USB"};
        QButtonGroup *modeGroup = new QButtonGroup(modeWidget);
        for (const QString &m : modes) {
            auto *rb = new QRadioButton(m, this);
            if (m == connMode) rb->setChecked(true);
            modeLayout->addWidget(rb);
            modeGroup->addButton(rb);
        }
        deviceTable->setCellWidget(row, 1, modeWidget);

        // 分组复选框
        QWidget *groupWidget = new QWidget(this);
        auto *layout = new QHBoxLayout(groupWidget);
        layout->setAlignment(Qt::AlignLeft);
        layout->setContentsMargins(0, 0, 0, 0);
        for (const QString &g : groups) {
            layout->addWidget(new QCheckBox(g, this));
        }
        deviceTable->setCellWidget(row, 2, groupWidget);
        deviceTable->resizeColumnsToContents();
    }

private slots:
    void onAddGroupClicked() {
        QString name = groupEdit->text().trimmed();
        if (!name.isEmpty()) {
            addGroup(name);
            groupEdit->clear();
        }
    }

    void onRemoveGroupClicked() {
        QList<QString> toRemove;
        for (int i = 0; i < groupContainerLayout->count(); ++i) {
            if (auto *cb = qobject_cast<QCheckBox *>(groupContainerLayout->itemAt(i)->widget())) {
                if (cb->isChecked()) toRemove << cb->text();
            }
        }
        for (const QString &name : toRemove)
            removeGroup(name);
    }
};
