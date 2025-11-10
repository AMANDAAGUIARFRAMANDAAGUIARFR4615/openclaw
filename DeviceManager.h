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
#include <QSplitter>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>

class DeviceManager : public QWidget {
    Q_OBJECT
public:
    DeviceManager(QWidget *parent = nullptr) : QWidget(parent) {
        setWindowTitle("设备管理系统");
        resize(1150, 700);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(10, 10, 10, 10);
        mainLayout->setSpacing(8);

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

        groupScroll = new QScrollArea(this);
        groupScroll->setWidgetResizable(true);
        groupContainer = new QWidget(this);
        groupContainerLayout = new QHBoxLayout(groupContainer);
        groupContainerLayout->setAlignment(Qt::AlignLeft);
        groupContainerLayout->setContentsMargins(5, 5, 5, 5);
        groupScroll->setWidget(groupContainer);
        groupLayout->addWidget(groupScroll, 1);

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
        deviceTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        deviceTable->verticalHeader()->setVisible(false);
        deviceTable->setAlternatingRowColors(true);
        deviceTable->setStyleSheet("QTableWidget { selection-background-color: #b0d8ff; }");
        deviceLayout->addWidget(deviceTable, 1);

        splitter->addWidget(deviceFrame);
        splitter->setStretchFactor(1, 2);

        // === 初始化分组与设备 ===
        for (int i = 1; i <= 16; ++i)  // 默认16组
            addGroup(QString("组%1").arg(i));

        addDevice("设备A", "WIFI优先");
        addDevice("设备B", "USB优先");

        // === 信号槽 ===
        connect(addGroupBtn, &QPushButton::clicked, this, &DeviceManager::onAddGroupClicked);
        connect(removeGroupBtn, &QPushButton::clicked, this, &DeviceManager::onRemoveGroupClicked);
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

    void addGroup(const QString &name) {
        if (name.trimmed().isEmpty() || groups.contains(name)) return;
        groups << name;
        auto *cb = new QCheckBox(name, this);
        cb->setChecked(true);
        groupContainerLayout->addWidget(cb);
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
        refreshDeviceTable();
    }

    void refreshDeviceTable() {
        struct DeviceInfo { QString name; QString connMode; };
        QVector<DeviceInfo> devices;

        for (int row = 0; row < deviceTable->rowCount(); ) {
            auto *item = deviceTable->item(row, 0);
            if (item) {
                QString name = item->text();
                QWidget *modeWidget = deviceTable->cellWidget(row, 1);
                QString connMode;
                if (modeWidget) {
                    auto layout = modeWidget->layout();
                    for (int i = 0; i < layout->count(); ++i) {
                        if (auto *rb = qobject_cast<QRadioButton *>(layout->itemAt(i)->widget())) {
                            if (rb->isChecked()) connMode = rb->text();
                        }
                    }
                }
                devices.push_back({name, connMode});
            }
            int groupRows = (groups.size() + 4) / 5; // 每行5个分组
            row += groupRows;
        }

        deviceTable->clearContents();
        deviceTable->setRowCount(0);
        for (auto &d : devices)
            addDevice(d.name, d.connMode);
    }

    void addDevice(const QString &name, const QString &connMode) {
        const int perRow = 5; // 每行5个分组
        int totalRows = (groups.size() + perRow - 1) / perRow;

        for (int i = 0; i < totalRows; ++i) {
            int row = deviceTable->rowCount();
            deviceTable->insertRow(row);

            if (i == 0) {
                deviceTable->setItem(row, 0, new QTableWidgetItem(name));

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
            }

            QWidget *groupWidget = new QWidget(this);
            auto *hLayout = new QHBoxLayout(groupWidget);
            hLayout->setContentsMargins(0, 0, 0, 0);
            hLayout->setSpacing(10);

            for (int j = i * perRow; j < (i + 1) * perRow && j < groups.size(); ++j) {
                QCheckBox *cb = new QCheckBox(groups[j], this);
                cb->setChecked(true);
                hLayout->addWidget(cb);
            }
            hLayout->addStretch();
            deviceTable->setCellWidget(row, 2, groupWidget);
        }
        deviceTable->resizeColumnsToContents();
    }

private slots:
    void onAddGroupClicked() {
        QString name = groupEdit->text().trimmed();
        if (!name.isEmpty()) {
            addGroup(name);
            groupEdit->clear();
            refreshDeviceTable();
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
