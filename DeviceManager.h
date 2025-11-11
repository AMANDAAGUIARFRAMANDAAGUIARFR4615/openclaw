#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFrame>
#include <QCheckBox>
#include <QKeyEvent>
#include <QSpacerItem>

class DeviceManager : public QWidget {
    Q_OBJECT
public:
    explicit DeviceManager(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("设备列表");
        setAttribute(Qt::WA_DeleteOnClose);
        resize(900, 700);

        auto *mainLayout = new QVBoxLayout(this);

        deviceTable = new QTableWidget(this);
        deviceTable->setSelectionMode(QAbstractItemView::SingleSelection);
        deviceTable->setColumnCount(3);
        deviceTable->setHorizontalHeaderLabels({"设备名", "连接方式", "所属分组"});
        deviceTable->horizontalHeader()->setStretchLastSection(true);
        deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        deviceTable->verticalHeader()->setVisible(false);
        deviceTable->setAlternatingRowColors(true);
        // deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        deviceTable->setStyleSheet(R"(
            QTableWidget {
                selection-background-color: #cbe8ff;
                gridline-color: #ccc;
                font-size: 14px;
            }
            QHeaderView::section {
                background-color: #f2f2f2;
                font-weight: bold;
                border: 1px solid #ddd;
                padding: 6px;
            }
        )");

        mainLayout->addWidget(deviceTable);

        groups = {"组1", "组2", "组3", "组4", "组5", "组6"};
        addDevice("设备A", "WIFI优先");
        addDevice("设备B", "USB优先");
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
    QStringList groups;

    void addDevice(const QString &name, const QString &connMode) {
        int row = deviceTable->rowCount();
        deviceTable->insertRow(row);

        // === 设备名 ===
        auto *item = new QTableWidgetItem(name);
        item->setTextAlignment(Qt::AlignCenter);
        deviceTable->setItem(row, 0, item);

        // === 连接方式 ===
        QWidget *modeWidget = new QWidget(this);
        auto *modeLayout = new QHBoxLayout(modeWidget);
        modeLayout->setContentsMargins(5, 0, 5, 0);
        modeLayout->setSpacing(10);
        modeLayout->setAlignment(Qt::AlignLeft);

        QStringList modes = {"WIFI优先", "USB优先", "仅WIFI", "仅USB"};
        QButtonGroup *modeGroup = new QButtonGroup(modeWidget);
        for (const QString &m : modes) {
            auto *rb = new QRadioButton(m, modeWidget);
            rb->setStyleSheet("font-size: 13px;");
            if (m == connMode) rb->setChecked(true);
            modeLayout->addWidget(rb);
            modeGroup->addButton(rb);
        }
        modeLayout->addStretch();
        deviceTable->setCellWidget(row, 1, modeWidget);

        // === 所属分组 ===
        QWidget *groupWidget = new QWidget(this);
        auto *groupLayout = new QHBoxLayout(groupWidget);
        groupLayout->setContentsMargins(5, 0, 5, 0);
        groupLayout->setSpacing(8);
        groupLayout->setAlignment(Qt::AlignLeft);

        for (const QString &g : groups) {
            auto *cb = new QCheckBox(g, groupWidget);
            cb->setChecked(true);
            cb->setStyleSheet("font-size: 13px;");
            groupLayout->addWidget(cb);
        }
        groupLayout->addStretch();
        deviceTable->setCellWidget(row, 2, groupWidget);

        deviceTable->resizeRowsToContents();
    }
};
