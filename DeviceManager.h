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
#include <QLayout>

class FlowLayout : public QLayout {
public:
    FlowLayout(QWidget *parent = nullptr) : QLayout(parent) {}

    ~FlowLayout() {
        QLayoutItem *item;
        while ((item = takeAt(0)))
            delete item;
    }

    void addItem(QLayoutItem *item) override {
        itemList.append(item);
    }

    int count() const override {
        return itemList.count();
    }

    QLayoutItem *itemAt(int index) const override {
        return itemList.value(index);
    }

    QSize sizeHint() const override {
        auto rect = geometry();
        
        int x = rect.x();
        int y = rect.y();
        int lineHeight = 0;
        int rowWidth = rect.width();

        for (int i = 0; i < itemList.size(); ++i) {
            QLayoutItem *item = itemList.at(i);
            item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));

            x += item->sizeHint().width();
            lineHeight = std::max(lineHeight, item->sizeHint().height());

            if (x + item->sizeHint().width() > rowWidth) {
                x = rect.x();
                y += lineHeight;
                lineHeight = 0;
            }
        }

        return QSize(rowWidth, y + lineHeight);
    }

    QLayoutItem *takeAt(int index) override {
        if (index >= 0 && index < itemList.size())
            return itemList.takeAt(index);

        return nullptr;
    }

private:
    QList<QLayoutItem *> itemList;
};

class DeviceManager : public QWidget {
    Q_OBJECT
public:
    explicit DeviceManager(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("设备列表");
        setAttribute(Qt::WA_DeleteOnClose);
        resize(900, 700);

        auto mainLayout = new QVBoxLayout(this);

        deviceTable = new QTableWidget(this);
        deviceTable->setSelectionMode(QAbstractItemView::NoSelection);
        deviceTable->setColumnCount(3);
        deviceTable->setHorizontalHeaderLabels({"设备名", "连接方式", "所属分组"});
        deviceTable->horizontalHeader()->setStretchLastSection(true);
        deviceTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        deviceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        deviceTable->verticalHeader()->setVisible(false);
        deviceTable->setAlternatingRowColors(true);
        deviceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        deviceTable->setStyleSheet(R"(
            QTableWidget {
                background: #ffffff;
                alternate-background-color: #f7f7f7;
                gridline-color: #dcdcdc;
                border: 1px solid #cccccc;
                font-size: 14px;
            }
            QHeaderView::section {
                background: #f0f0f0;
                padding: 4px;
                border: 1px solid #d0d0d0;
                font-weight: bold;
            }
            QRadioButton, QCheckBox {
                font-size: 13px;
            }
        )");

        mainLayout->addWidget(deviceTable);

        for (int i = 0; i < 32; i++)
            groups << QString("组%1").arg(i + 1);

        addDevice("设备A - 测试", "WIFI优先");
        addDevice("设备B", "USB优先");

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
    QStringList groups;

    void addDevice(const QString &name, const QString &connMode) {
        int row = deviceTable->rowCount();
        deviceTable->insertRow(row);

        auto item = new QTableWidgetItem(name);
        deviceTable->setItem(row, 0, item);

        QWidget *modeWidget = new QWidget(this);
        auto modeLayout = new QHBoxLayout(modeWidget);

        QStringList modes = {"WIFI优先", "USB优先", "仅WIFI", "仅USB"};
        QButtonGroup *modeGroup = new QButtonGroup(modeWidget);
        for (const QString &m : modes) {
            auto rb = new QRadioButton(m, modeWidget);
            if (m == connMode) rb->setChecked(true);
            modeLayout->addWidget(rb);
            modeGroup->addButton(rb);
        }
        deviceTable->setCellWidget(row, 1, modeWidget);

        QWidget *groupWidget = new QWidget(this);
        auto groupLayout = new FlowLayout(groupWidget);

        for (int i = 0; i < groups.size(); ++i) {
            auto cb = new QCheckBox(groups[i], groupWidget);
            cb->setChecked(true);
            groupLayout->addWidget(cb);
        }

        groupWidget->setLayout(groupLayout);
        deviceTable->setCellWidget(row, 2, groupWidget);

        deviceTable->resizeRowsToContents();
    }
};
