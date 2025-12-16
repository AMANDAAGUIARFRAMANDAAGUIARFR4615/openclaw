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
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

class AppSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    static AppSettingsDialog* getInstance() { static AppSettingsDialog instance; return &instance; }

    int getValue(const QString &key) {
        return settings.value(key, m_intDefaults.value(key)).toInt();
    }

    QStringList getEnabledList(const QString &key) {
        QStringList defaults = m_listDefaults.value(key);
        QByteArray data = settings.value(key).toByteArray();

        if (data.isEmpty()) return defaults;

        QStringList result;
        QSet<QString> savedKeys;

        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray array = doc.array();

        for (const auto &val : array) {
            QJsonObject obj = val.toObject();
            QString name = obj["name"].toString();
            bool enabled = obj["enable"].toBool();

            if (!name.isEmpty()) {
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
    }

    ~AppSettingsDialog() = default;

signals:
    void configurationChanged(const QString &key);

private:
    void addSortableGroup(QVBoxLayout *parentLayout, const QString &key, const QString &title, const QStringList &defaults, bool checkable)
    {
        m_listDefaults.insert(key, defaults);

        QVBoxLayout *groupLayout = new QVBoxLayout();

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

        QByteArray data = settings.value(key).toByteArray();
        QSet<QString> loadedKeys;

        if (data.isEmpty()) {
            for (const auto &t : defaults) addItem(t, true);
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonArray array = doc.array();
            
            for (const auto &val : array) {
                QJsonObject obj = val.toObject();
                QString name = obj["name"].toString();
                if (!name.isEmpty()) {
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
            settings.setValue(key, QJsonDocument(jsonArray).toJson(QJsonDocument::Compact));
            emit configurationChanged(key);
        };

        connect(listWidget->model(), &QAbstractItemModel::rowsMoved, this, saveFunc);
        connect(listWidget, &QListWidget::itemChanged, this, saveFunc);
    }

    void addSettingGroup(QVBoxLayout *parentLayout, const QString &key, const QString &title, const QStringList &options, int defaultIndex)
    {
        m_intDefaults.insert(key, defaultIndex);

        QVBoxLayout *groupLayout = new QVBoxLayout();

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