#pragma once

#include "MainWindow.h"
#include "Account.h"
#include "Safe.h"
#include <QDialog>
#include <QTableWidget>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSplitter>
#include <QToolTip>
#include <algorithm> // 必须包含这个以使用 std::partition

class SwapExpirationDialog : public QDialog {
    Q_OBJECT

public:
    explicit SwapExpirationDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("互换到期时间");
        setMinimumSize(960, 600);
        auto layout = new QVBoxLayout(this);

        auto splitter = new QSplitter(Qt::Horizontal);
        createPanel(leftWidget = new QWidget, leftFilter, leftAll, leftTable, "源设备 (A)");
        createPanel(rightWidget = new QWidget, rightFilter, rightAll, rightTable, "目标设备 (B)");
        splitter->addWidget(leftWidget);
        splitter->addWidget(rightWidget);
        layout->addWidget(splitter, 1);

        layout->addWidget(new QLabel("提示: 左右设备互斥，不可选设备已置底。"));
        auto box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        layout->addWidget(box);

        // --- 核心联动逻辑 ---
        auto bindLogic = [&](QTableWidget* src, QTableWidget* dst, QCheckBox* srcAll, QCheckBox* dstAll) {
            // 列表项点击 -> 处理互斥 -> 更新两边全选状态
            connect(src, &QTableWidget::itemChanged, [=](QTableWidgetItem *item) {
                if (item->column() == 0) {
                    syncItemState(item, dst); 
                    updateSelectAll(dst, dstAll); 
                    updateSelectAll(src, srcAll); 
                }
            });
            // 筛选改变 -> 重新加载
            connect(src->findChild<QComboBox*>(), QOverload<int>::of(&QComboBox::currentIndexChanged), [=]() {
                loadTable(src, src->findChild<QComboBox*>(), dst, srcAll);
            });
            // 全选点击
            connect(srcAll, &QCheckBox::clicked, [=](bool) {
                bool checked = srcAll->checkState() == Qt::Checked;
                src->blockSignals(true);
                for(int i=0; i<src->rowCount(); ++i) {
                    if(src->item(i, 0)->flags() & Qt::ItemIsEnabled) { 
                        src->item(i, 0)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
                        syncItemState(src->item(i, 0), dst); // 手动触发互斥
                    }
                }
                src->blockSignals(false);
                updateSelectAll(dst, dstAll); 
            });
        };

        bindLogic(leftTable, rightTable, leftAll, rightAll);
        bindLogic(rightTable, leftTable, rightAll, leftAll);

        connect(box, &QDialogButtonBox::accepted, this, &SwapExpirationDialog::onConfirm);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

        initFilter(leftFilter);
        initFilter(rightFilter);
        // 初始加载
        loadTable(leftTable, leftFilter, rightTable, leftAll);
        loadTable(rightTable, rightFilter, leftTable, rightAll);
    }

private:
    QWidget *leftWidget, *rightWidget;
    QComboBox *leftFilter, *rightFilter;
    QCheckBox *leftAll, *rightAll;
    QTableWidget *leftTable, *rightTable;

    void createPanel(QWidget* w, QComboBox*& cb, QCheckBox*& all, QTableWidget*& table, QString title) {
        auto l = new QVBoxLayout(w);
        l->addWidget(new QLabel(title));
        
        auto h = new QHBoxLayout();
        h->addWidget(new QLabel("分组:"));
        h->addWidget(cb = new QComboBox(), 1);
        l->addLayout(h);

        l->addWidget(all = new QCheckBox("全选"));
        all->setTristate(true);

        table = new QTableWidget();
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({"", "名称", "机型", "到期"});
        // 修复1: header() -> horizontalHeader()
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setFocusPolicy(Qt::NoFocus);
        l->addWidget(table);
        
        cb->setParent(w); 
    }

    void initFilter(QComboBox* cb) {
        for (const auto& t : MainWindow::getInstance()->getTabs()) cb->addItem(t.name, t.bit);
        cb->setCurrentIndex(MainWindow::getInstance()->tabWidget->currentIndex());
    }

    void syncItemState(QTableWidgetItem* srcItem, QTableWidget* dstTable) {
        QString udid = srcItem->data(Qt::UserRole).toString();
        bool checked = (srcItem->checkState() == Qt::Checked);
        
        dstTable->blockSignals(true); 
        for (int i = 0; i < dstTable->rowCount(); ++i) {
            auto it = dstTable->item(i, 0);
            if (it->data(Qt::UserRole).toString() == udid) {
                if (checked) {
                    it->setCheckState(Qt::Unchecked);
                    it->setFlags(it->flags() & ~Qt::ItemIsEnabled);
                    dstTable->item(i, 1)->setForeground(Qt::gray);
                    dstTable->item(i, 3)->setForeground(Qt::gray);
                } else {
                    it->setFlags(it->flags() | Qt::ItemIsEnabled);
                    dstTable->item(i, 1)->setForeground(Qt::black);
                    bool expired = dstTable->item(i, 3)->data(Qt::UserRole).toBool();
                    dstTable->item(i, 3)->setForeground(expired ? QColor("#d32f2f") : Qt::black);
                }
                break;
            }
        }
        dstTable->blockSignals(false);
    }

    void loadTable(QTableWidget* table, QComboBox* cb, QTableWidget* other, QCheckBox* allBox) {
        if (cb->currentIndex() < 0) return;
        auto bit = cb->currentData().toUInt();
        auto devs = DeviceInfo::getDevices(bit == 0 ? 0 : (1U << bit));
        
        QSet<QString> lockedIds;
        for(int i=0; i<other->rowCount(); ++i) 
            if(other->item(i,0)->checkState() == Qt::Checked) 
                lockedIds.insert(other->item(i,0)->data(Qt::UserRole).toString());

        std::partition(devs.begin(), devs.end(), [&](const auto& d){ return !lockedIds.contains(d->deviceId); });

        table->blockSignals(true);
        table->setRowCount(0);
        table->setRowCount(devs.size());

        for (int i = 0; i < devs.size(); ++i) {
            bool isLocked = lockedIds.contains(devs[i]->deviceId);
            auto item = new QTableWidgetItem();
            item->setData(Qt::UserRole, devs[i]->deviceId);
            item->setCheckState(Qt::Unchecked);
            if (isLocked) item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            table->setItem(i, 0, item);

            auto name = new QTableWidgetItem(devs[i]->deviceName);
            if (isLocked) name->setForeground(Qt::gray);
            table->setItem(i, 1, name);
            
            table->setItem(i, 2, new QTableWidgetItem(devs[i]->model));

            auto timeStr = QDateTime::fromMSecsSinceEpoch(devs[i]->expireAt.get()).toString("yyyy-MM-dd");
            auto time = new QTableWidgetItem(timeStr);
            bool expired = devs[i]->expireAt.get() < QDateTime::currentMSecsSinceEpoch();
            time->setData(Qt::UserRole, expired); 
            
            if (isLocked) time->setForeground(Qt::gray);
            else if (expired) time->setForeground(QColor("#d32f2f"));
            table->setItem(i, 3, time);
        }
        table->blockSignals(false);
        updateSelectAll(table, allBox);
    }

    void updateSelectAll(QTableWidget* table, QCheckBox* box) {
        int checked = 0, enabled = 0;
        for(int i=0; i<table->rowCount(); ++i) {
            if(table->item(i,0)->flags() & Qt::ItemIsEnabled) {
                enabled++;
                if(table->item(i,0)->checkState() == Qt::Checked) checked++;
            }
        }
        box->blockSignals(true);
        box->setCheckState((enabled > 0 && checked == enabled) ? Qt::Checked : 
                           (checked > 0 ? Qt::PartiallyChecked : Qt::Unchecked));
        box->setEnabled(enabled > 0);
        box->blockSignals(false);
    }

    void onConfirm() {
        auto getIds = [](QTableWidget* t) {
            QJsonArray arr;
            for(int i=0; i<t->rowCount(); ++i)
                if(t->item(i,0)->checkState() == Qt::Checked)
                    arr.append(t->item(i,0)->data(Qt::UserRole).toString());
            return arr;
        };

        QJsonArray l = getIds(leftTable), r = getIds(rightTable);
        if (l.isEmpty() || r.isEmpty() || l.size() != r.size()) {
            QToolTip::showText(QCursor::pos(), "左右两边选择的设备数量必须一致且不为空");
            return;
        }

        setEnabled(false);
        QJsonObject json; json["leftIds"] = l; json["rightIds"] = r;
        webSocketClient->emitEvent("swapExpire", json, [=](const QJsonValue &res) {
            setEnabled(true);
            if (res.isString()) { QToolTip::showText(QCursor::pos(), res.toString()); return; }
            
            // 修复2: QJsonValue 必须转成 Object 才能用字符串 Key 访问
            const auto& devicesArr = res["devices"].toArray();
            for (const auto &v : devicesArr) {
                QJsonObject obj = v.toObject(); 
                auto deviceInfo = DeviceInfo::getDevice(obj["udid"].toString());
                if(deviceInfo) deviceInfo->expireAt = obj[HIDE("expireAt")].toInteger();
            }
            accept();
        });
    }
};