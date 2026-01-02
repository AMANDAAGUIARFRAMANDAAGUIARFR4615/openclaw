#pragma once

#include "global.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QListWidget>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>
#include <QGroupBox>
#include <QToolTip>

class AppSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    static AppSettingsDialog* getInstance() { static AppSettingsDialog instance; return &instance; }

    int getValue(const QString &key) {
        return settings->value(key, m_intDefaults.value(key)).toInt();
    }

    QStringList getEnabledList(const QString &key) {
        QStringList defaults = m_listDefaults.value(key);
        QJsonArray array = settings->value(key).toJsonArray();

        if (array.isEmpty()) return defaults;

        QStringList result;
        QSet<QString> savedKeys;

        for (const auto &val : array) {
            QJsonObject obj = val.toObject();
            QString name = obj["name"].toString();
            bool enabled = obj["enable"].toBool();

            if (!name.isEmpty() && defaults.contains(name)) {
                savedKeys.insert(name);
                if (enabled) result.append(name);
            }
        }

        for (const auto& def : defaults) {
            if (!savedKeys.contains(def)) result.append(def);
        }

        return result;
    }

private:
    QMap<QString, QStringList> m_listDefaults;
    QMap<QString, int> m_intDefaults;

    explicit AppSettingsDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("设置");
        setModal(true);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(15);
        mainLayout->setContentsMargins(25, 20, 25, 20);

        QGroupBox *defaultBox = new QGroupBox("全局默认配置 (分组单独设置优先)", this);
        defaultBox->setStyleSheet(R"(
            QGroupBox {
                border: 2px solid #AAAAAA;
                border-radius: 5px;
                margin-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 10px;
                padding: 0 5px;
            }
        )");

        QVBoxLayout *boxLayout = new QVBoxLayout(defaultBox);
        boxLayout->setSpacing(5);

        addSettingGroup(boxLayout, "isLandscape", "投屏显示", {"竖屏显示", "横屏显示"}, 0);
        addSettingGroup(boxLayout, "videoQuality", "视频清晰度", {"", "图片流", "标清", "高清", "超清"}, 2);
        addSettingGroup(boxLayout, "connectionMethod", "连接方式", {"USB优先", "WIFI优先"}, 0);
        addSettingGroup(boxLayout, "autoScanLANDevices", "自动连接局域网设备", {"关闭", "开启"}, 1);
        addSettingGroup(boxLayout, "autoConnectUSBDevices", "自动连接USB设备", {"关闭", "开启"}, 1);

        mainLayout->addWidget(defaultBox);

        QStringList sideBarMenu{"🔗设备连接", "🕹️同屏操作", "⚙️设置", "💡帮助", "📲越狱助手", "📱手机软件源", "⏳续费"};

        if (QFile::exists(qApp->applicationDirPath() + "/support.jpg"))
            sideBarMenu.append("💬客服");

        if (qEnvironmentVariableIsSet("FROM_QT_CREATOR")) {
            sideBarMenu.append("📜日志");
            sideBarMenu.append("🛠️开发者");
        }

        addSortableGroup(mainLayout, "sideBarMenu", "左侧栏 (拖拽调整)", sideBarMenu);

        addSortableGroup(mainLayout, "windowMenu", "投屏窗口右键菜单 (拖拽调整)", 
            {"🏠主屏幕", "🎛️控制中心", "↕️应用切换", "🧹清理应用", "📁文件管理", "⏺️录制+回放", "🧩应用管理", "📸截图", "🔄重启", "🔒锁屏", "🗑️清空相册", "🔊音量+", "🔈音量-", "🕹️同屏操作", "🔧修改分组"});

        addSortableGroup(mainLayout, "tabBarMenu", "分组标签页右键菜单 (拖拽调整)", 
            {"重命名分组", "添加分组", "删除分组", "投屏显示", "视频清晰度", "连接方式", "自动连接局域网设备", "自动连接USB设备"});
    }

    ~AppSettingsDialog() = default;

signals:
    void configurationChanged(const QString &key);

private:
    void addSortableGroup(QVBoxLayout *parentLayout, const QString &key, const QString &title, const QStringList &defaults, bool checkable = true)
    {
        m_listDefaults.insert(key, defaults);

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

        auto addItem = [&](const QString &text, bool checked) {
            QListWidgetItem *item = new QListWidgetItem(text);
            item->setFlags(item->flags() ^ (checkable ? Qt::NoItemFlags : Qt::ItemIsUserCheckable));
            if (checkable) item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            listWidget->addItem(item);
        };

        QJsonArray array = settings->value(key).toJsonArray();
        QSet<QString> loadedKeys;

        if (array.isEmpty()) {
            for (const auto &t : defaults) addItem(t, true);
        } else {
            for (const auto &val : array) {
                QJsonObject obj = val.toObject();
                QString name = obj["name"].toString();
                if (!name.isEmpty() && defaults.contains(name)) {
                    addItem(name, obj["enable"].toBool());
                    loadedKeys.insert(name);
                }
            }
            // 补充缺失的默认项
            for (const auto &def : defaults) {
                if (!loadedKeys.contains(def)) addItem(def, true);
            }
        }

        if (listWidget->count() > 0) {
            int h = listWidget->sizeHintForRow(0) * listWidget->count() + listWidget->frameWidth() * 2;
            listWidget->setFixedHeight(h);
        }

        groupLayout->addWidget(titleLabel);
        groupLayout->addWidget(listWidget);
        parentLayout->addLayout(groupLayout);

        auto saveFunc = [=]() {
            QJsonArray jsonArray;
            for (int i = 0; i < listWidget->count(); ++i) {
                QListWidgetItem *it = listWidget->item(i);
                bool checked = !checkable || it->checkState() == Qt::Checked;
                
                QJsonObject obj;
                obj["name"] = it->text();
                obj["enable"] = checked;
                jsonArray.append(obj);
            }
            // 使用 Compact 模式保存为紧凑的 JSON 字符串/字节数组
            settings->setValue(key, jsonArray);
            emit configurationChanged(key);
        };

        connect(listWidget->model(), &QAbstractItemModel::rowsMoved, saveFunc);
        connect(listWidget, &QListWidget::itemChanged, [=](QListWidgetItem *item) {
            if (item->text() == "⚙️设置" && item->checkState() == Qt::Unchecked) {
                QToolTip::showText(QCursor::pos(), "[⚙️设置]不可隐藏");
                // 🚫 立即强制改回选中
                QSignalBlocker blocker(listWidget); // 暂时屏蔽信号，防止递归调用
                item->setCheckState(Qt::Checked);
            } else {
                // 其他情况正常保存
                saveFunc();
            }
        });
    }

    void addSettingGroup(QVBoxLayout *parentLayout, const QString &key, const QString &title, const QStringList &options, int defaultIndex)
    {
        m_intDefaults.insert(key, defaultIndex);

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
        int currentVal = settings->value(key, defaultIndex).toInt();

        for (int i = 0; i < options.size(); ++i) {
            if (options[i].isEmpty())
                continue;

            QRadioButton *radio = new QRadioButton(options[i], this);
            if (i == currentVal) radio->setChecked(true);
            group->addButton(radio, i);
            optionsLayout->addWidget(radio);
        }

        connect(group, QOverload<int>::of(&QButtonGroup::idClicked), [=](int id){
            settings->setValue(key, id);
            emit configurationChanged(key);
        });

        optionsLayout->addStretch();
        groupLayout->addWidget(titleLabel);
        groupLayout->addLayout(optionsLayout);
        parentLayout->addLayout(groupLayout);
    }
};
