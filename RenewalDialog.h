#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QDateTime>
#include <algorithm>

struct DeviceInfo {
    QString id;
    QString name;
    QDateTime expireTime;
};

class RenewalDialog : public QDialog {
    Q_OBJECT

public:
    explicit RenewalDialog(QList<DeviceInfo> devices, QWidget *parent = nullptr)
        : QDialog(parent), m_tableWidget(new QTableWidget(this))
    {
        setWindowTitle(tr("设备续费列表"));
        resize(600, 450);

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

        QDialogButtonBox *btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(15);
        layout->addWidget(m_tableWidget);
        layout->addWidget(btnBox);

        connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
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

private:
    QTableWidget *m_tableWidget;
};
