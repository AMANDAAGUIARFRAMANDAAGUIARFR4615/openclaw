#pragma once

#include "global.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFont>
#include <QStringList>
#include <QListWidget>
#include <QSettings>
#include <QCoreApplication>

class AppSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    static AppSettingsDialog* getInstance() { static AppSettingsDialog instance; return &instance; }

    int getValue(const QString &key, int defaultValue = 0) {
        return settings.value(key, defaultValue).toInt();
    }

    // 获取排序且选中的菜单列表
    QStringList getEnabledList(const QString &key, const QStringList &defaults) {
        QStringList result;
        QVariantList saved = settings.value(key).toList();
        
        if (saved.isEmpty()) return defaults; // 首次运行返回默认

        for (const auto& var : saved) {
            QStringList itemData = var.toStringList();
            // 格式: [名称, 状态("1"/"0")]。只返回状态为"1"的
            if (itemData.size() == 2 && itemData[1] == "1") {
                result.append(itemData[0]);
            }
        }
        
        // 检查是否有默认项在更新后被遗漏（追加到末尾）
        QStringList allSavedKeys;
        for(const auto& var : saved) allSavedKeys << var.toStringList().first();
        for(const auto& def : defaults) {
            if(!allSavedKeys.contains(def)) result.append(def);
        }

        return result;
    }

private:
    explicit AppSettingsDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("设置");

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(15);
        mainLayout->setContentsMargins(25, 20, 25, 20);

        addSettingGroup(mainLayout, "diaplayMode", "默认投屏显示【分组单独设置后不受此选项影响】", {"竖屏显示", "横屏显示"}, 0);
        addSettingGroup(mainLayout, "connectionMethod", "默认连接方式【分组单独设置后不受此选项影响】", {"WIFI优先", "USB优先"}, 0);
        addSettingGroup(mainLayout, "videoQuality", "默认视频清晰度【分组单独设置后不受此选项影响】", {"图片流", "标清", "高清", "超清"}, 1);
        addSettingGroup(mainLayout, "autoScan", "自动扫描局域网设备【分组单独设置后不受此选项影响】", {"开启", "关闭"}, 0);

        addSortableGroup(mainLayout, "sideBarMenu", "左侧栏 (拖拽调整)", 
            {"🔗设备连接", "🕹️同屏操作", "⚙️设置", "💡帮助", "📲越狱助手", "💬客服", "📜日志", "🛠️开发者"}, false);

        addSortableGroup(mainLayout, "windowMenu", "投屏窗口右键菜单 (拖拽调整)", 
            {"🏠主屏幕", "🧹清理应用", "📁文件管理", "⏺️录制+回放", "🧩应用列表", "📸截图", "🔄重启", "🔒锁屏", "🗑️清空相册", "🔊音量+", "🔈 音量-", "🔧修改分组"}, true);

        addSortableGroup(mainLayout, "tabMenu", "分组标签页右键菜单 (拖拽调整)", 
            {"横竖屏切换", "重命名分组", "添加分组", "删除分组"}, true);

        setModal(true);
    }

    ~AppSettingsDialog() = default;

signals:
    void configurationChanged(const QString &key);

private:
    void addSortableGroup(QVBoxLayout *parentLayout, const QString &key, const QString &title, const QStringList &defaults, bool checkable)
    {
        QVBoxLayout *groupLayout = new QVBoxLayout();
        groupLayout->setSpacing(5);

        QLabel *titleLabel = new QLabel(title, this);
        QFont font = titleLabel->font();
        font.setBold(true);
        font.setPointSize(9);
        titleLabel->setFont(font);

        QListWidget *listWidget = new QListWidget(this);
        listWidget->setDragDropMode(QAbstractItemView::InternalMove);
        listWidget->setDefaultDropAction(Qt::MoveAction);
        listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

        // 加载数据：需要显示所有项(包括未勾选的)以便用户管理
        QVariantList saved = settings.value(key).toList();
        struct ItemState { QString text; bool checked; };
        QList<ItemState> items;

        if (saved.isEmpty()) {
            for(const auto &t : defaults) items.append({t, true});
        } else {
            QStringList loadedKeys;
            for(const auto &var : saved) {
                QStringList data = var.toStringList();
                if(data.size() == 2) {
                    items.append({data[0], data[1] == "1"});
                    loadedKeys << data[0];
                }
            }
            // 补全新增的默认项
            for(const auto &def : defaults) {
                if(!loadedKeys.contains(def)) items.append({def, true});
            }
        }

        for (const auto &itemState : items) {
            QListWidgetItem *item = new QListWidgetItem(itemState.text);
            if (checkable) {
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(itemState.checked ? Qt::Checked : Qt::Unchecked);
            }
            listWidget->addItem(item);
        }

        int totalHeight = (listWidget->fontMetrics().height() + 4) * defaults.size();
        listWidget->setFixedHeight(totalHeight);

        groupLayout->addWidget(titleLabel);
        groupLayout->addWidget(listWidget);
        parentLayout->addLayout(groupLayout);

        auto saveFunc = [=]() {
            QVariantList saveList;
            for(int i = 0; i < listWidget->count(); ++i) {
                QListWidgetItem *it = listWidget->item(i);
                saveList << QStringList{it->text(), it->checkState() == Qt::Checked ? "1" : "0"};
            }
            settings.setValue(key, saveList);
            emit configurationChanged(key);
        };

        connect(listWidget->model(), &QAbstractItemModel::rowsMoved, this, saveFunc);
        connect(listWidget, &QListWidget::itemChanged, this, saveFunc);
    }

    void addSettingGroup(QVBoxLayout *parentLayout, const QString &key, const QString &title, const QStringList &options, int defaultIndex)
    {
        QVBoxLayout *groupLayout = new QVBoxLayout();
        groupLayout->setSpacing(5);

        QLabel *titleLabel = new QLabel(title, this);
        QFont font = titleLabel->font();
        font.setBold(true);
        font.setPointSize(9);
        titleLabel->setFont(font);

        QHBoxLayout *optionsLayout = new QHBoxLayout();
        optionsLayout->setSpacing(20);

        QButtonGroup *group = new QButtonGroup(this);
        int currentVal = settings.value(key, defaultIndex).toInt();

        for (int i = 0; i < options.size(); ++i) {
            QRadioButton *radio = new QRadioButton(options[i], this);
            if (i == currentVal) radio->setChecked(true);
            group->addButton(radio, i);
            optionsLayout->addWidget(radio);
        }

        connect(group, QOverload<int>::of(&QButtonGroup::idClicked), [=](int id){
            settings.setValue(key, id);
            emit configurationChanged(key);
        });

        optionsLayout->addStretch();
        groupLayout->addWidget(titleLabel);
        groupLayout->addLayout(optionsLayout);
        parentLayout->addLayout(groupLayout);
    }
};
