#pragma once

#include "global.h"
#include "Tools.h"
#include "BaseDialog.h"
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QListWidget>
#include <QSet>
#include <QJsonArray>
#include <QJsonObject>
#include <QGroupBox>
#include <QToolTip>
#include <QKeySequenceEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QTabWidget>

class SingleKeySequenceEdit : public QKeySequenceEdit {
public:
    using QKeySequenceEdit::QKeySequenceEdit;

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape) {
            window()->close();
            return;
        }
            
        // 如果当前已经有保存的快捷键序列，并且用户按下了新的按键
        // 则先清空旧的，实现"覆盖"效果，而不是追加
        if (!keySequence().isEmpty())
            clear();
        
        QKeySequenceEdit::keyPressEvent(event);
    }
};

class AppSettingsDialog : public BaseDialog
{
    Q_OBJECT

public:
    static AppSettingsDialog* getInstance() { static AppSettingsDialog* instance = new AppSettingsDialog; return instance; }

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

    QHash<QString, QKeySequence> getShortcuts(const QString &key) {
        QHash<QString, QKeySequence> result;
        
        auto defaults = m_shortcutDefaults.value(key);
        QJsonArray array = settings->value(key).toJsonArray();

        if (array.isEmpty()) {
            for (auto it = defaults.constBegin(); it != defaults.constEnd(); ++it) {
                if (!it.value().isEmpty()) result.insert(it.key(), it.value());
            }
            return result;
        }

        for (const auto &val : array) {
            QJsonObject obj = val.toObject();
            QString name = obj["name"].toString();
            QString shortcut = obj["shortcut"].toString();
            bool enable = obj["enable"].toBool();

            if (enable)
                result.insert(name, QKeySequence(shortcut));
        }
        
        return result;
    }

private:
    QHash<QString, QStringList> m_listDefaults;
    QHash<QString, int> m_intDefaults;
    QHash<QString, QHash<QString, QString>> m_shortcutDefaults;

    explicit AppSettingsDialog(QWidget *parent = nullptr) : BaseDialog("设置", parent)
    {
        auto mainLayout = contentLayout();
        
        QTabWidget *tabWidget = new QTabWidget(this);
        mainLayout->addWidget(tabWidget);

        // ==========================================
        // --- Tab 1: 常规与投屏设置 ---
        // ==========================================

        QWidget *generalTab = new QWidget();
        QVBoxLayout *generalLayout = new QVBoxLayout(generalTab);
        generalLayout->setSpacing(15);
        generalLayout->setContentsMargins(15, 15, 15, 15);

        addSettingGroup(generalLayout, "colorScheme", "颜色主题", {"默认", "浅色", "深色"}, 0);
        addSettingGroup(generalLayout, "autoSyncClipboard", "手机剪切板自动同步到电脑", {"关闭", "开启"}, 0);
        addSettingGroup(generalLayout, "sortSelectedToTop", "选中的设备排列在前", {"关闭", "开启"}, 0);
        addSettingGroup(generalLayout, "screenshotTo", "截图保存", {"剪切板", "文件", "剪切板+文件"}, 0);
        addSettingGroup(generalLayout, "standaloneAlwaysUltraHD", "独立窗口始终超高清", {"关闭", "开启"}, 0);
        addSettingGroup(generalLayout, "doubleClickOpenStandalone", "双击打开独立窗口", {"关闭", "开启"}, 1);
        addSettingGroup(generalLayout, "hideStandaloneToolbar", "隐藏独立窗口右侧按钮", {"关闭", "开启"}, 0);

        QGroupBox *defaultBox = new QGroupBox("投屏设置 (分组单独设置优先)", generalTab);
        defaultBox->setStyleSheet(R"(
            QGroupBox {
                border: 2px solid palette(mid);
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
        boxLayout->setContentsMargins(15, 25, 15, 15);

        addSettingGroup(boxLayout, "isLandscape", "投屏显示", {"竖屏显示", "横屏显示"}, 0);
        addSettingGroup(boxLayout, "videoFps", "视频帧率", {"", "5秒1帧", "1秒1帧", "1秒15帧", "1秒30帧", "1秒60帧", "1秒120帧"}, 4);
        addSettingGroup(boxLayout, "videoQuality", "视频清晰度", {"", "低清", "标清", "高清", "超清"}, 2);
        addSettingGroup(boxLayout, "connectionMethod", "连接方式", {"USB优先", "WIFI优先"}, 0);
        addSettingGroup(boxLayout, "autoScanLANDevices", "自动连接局域网设备", {"关闭", "开启"}, 1);
        addSettingGroup(boxLayout, "autoConnectUSBDevices", "自动连接USB设备", {"关闭", "开启"}, 1);

        generalLayout->addWidget(defaultBox);
        generalLayout->addStretch();

        tabWidget->addTab(generalTab, "常规与投屏");


        // ==========================================
        // --- Tab 2: 菜单排序与快捷键 ---
        // ==========================================

        QWidget *menuTab = new QWidget();
        QVBoxLayout *menuLayout = new QVBoxLayout(menuTab);
        menuLayout->setSpacing(15);
        menuLayout->setContentsMargins(10, 10, 10, 10);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        QVBoxLayout *topMenusLayout = new QVBoxLayout();
#else
        QHBoxLayout *topMenusLayout = new QHBoxLayout();
#endif
        topMenusLayout->setSpacing(20);
        topMenusLayout->setAlignment(Qt::AlignTop);

        QStringList sideBarMenu{"🔗设备连接", "⚙️设置", "💡帮助", "📲越狱助手", "📱手机软件源", "💿USB驱动", "⏳续费", "🤝换绑", "🌐软件更新"};
        
        if (Tools::isAppleMobileDeviceSupportInstalled())
            sideBarMenu.removeOne("💿USB驱动");
        
        if (Account::getInstance()->hasRedeemCode)
            sideBarMenu.append("🎫兑换码");
        
        if (QFile::exists(qApp->applicationDirPath() + "/imageformats/qpng.dll"))
            sideBarMenu.append("💬客服");

#ifndef QT_NO_DEBUG_OUTPUT
        sideBarMenu.append("📜日志");
#else
        if (Tools::isStartedByQtCreator())
            sideBarMenu.append("📜日志");
#endif

        if (Tools::isStartedByQtCreator())
            sideBarMenu.append("🛠️开发者");

        // 左侧：Sidebar
        addSortableGroup(topMenusLayout, "sideBarMenu", "左侧栏 (拖拽调整顺序)", sideBarMenu);

        // 右侧：TabBar Menu
        addSortableGroup(topMenusLayout, "tabBarMenu", "分组标签页右键菜单 (拖拽调整顺序)", 
            {"重命名分组", "添加分组", "删除分组", "投屏显示", "视频清晰度", "连接方式", "自动连接局域网设备", "自动连接USB设备"});

        menuLayout->addLayout(topMenusLayout);

        // 第二排：窗口右键菜单
        QStringList windowMenuItems = {
            "🏠主屏幕", "🎛️控制中心", "↕️应用切换", "🧹清理应用", "📁文件管理", 
            "⏺️录制+回放", "🧩应用管理", "📸截图", "🖼️截图库", "📹投屏录像", "📹录像库", "🔄重启", "🔒锁屏", 
            "📋获取剪切板", "🗑️清空相册", "🔊音量+", "🔈音量-", "🏹主控", "🎯被控",
            "📌置顶", "🔧修改分组", "🚀更新手机端", "🚩开启独占"
        };
        
        QHash<QString, QString> windowShortcuts;
        windowShortcuts["🏠主屏幕"] = "Ctrl+H";
        windowShortcuts["↕️应用切换"] = "Ctrl+Tab";
        windowShortcuts["🔊音量+"] = "Ctrl+Up";
        windowShortcuts["🔈音量-"] = "Ctrl+Down";
        windowShortcuts["📌置顶"] = "Ctrl+T";

        addSortableGroup(menuLayout, "windowMenu", "投屏窗口右键菜单 (拖拽调整顺序 / 双击设置快捷键)", 
            windowMenuItems, windowShortcuts);

        menuLayout->addStretch();

        tabWidget->addTab(menuTab, "菜单与快捷键");
    }

    ~AppSettingsDialog() = default;

signals:
    void configurationChanged(const QString &key);

private:
    void addSortableGroup(QBoxLayout *parentLayout, const QString &key, const QString &title, 
                          const QStringList &defaults, const QHash<QString, QString> &defaultShortcuts = {}, bool checkable = true)
    {
        m_listDefaults.insert(key, defaults);
        m_shortcutDefaults.insert(key, defaultShortcuts);

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
        listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        if (!defaultShortcuts.isEmpty()) {
            listWidget->setToolTip("双击即可修改快捷键");
            connect(listWidget, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem *item) {
                QString name = item->data(Qt::UserRole).toString();
                QString shortcut = item->data(Qt::UserRole + 1).toString();

                QDialog dialog(this);
                dialog.setWindowTitle("设置快捷键 - " + name);
                dialog.setWindowFlags(dialog.windowFlags() & ~Qt::WindowContextHelpButtonHint);
                dialog.resize(300, 120);

                QVBoxLayout *vLayout = new QVBoxLayout(&dialog);
                vLayout->addWidget(new QLabel("请按下键盘输入新的快捷键:", &dialog));

                QKeySequenceEdit *keyEdit = new SingleKeySequenceEdit(QKeySequence(shortcut), &dialog);
                keyEdit->setClearButtonEnabled(true);
                keyEdit->setMaximumSequenceLength(1);
                vLayout->addWidget(keyEdit);

                QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                
                connect(box, &QDialogButtonBox::accepted, [&](){
                    QString shortcut = keyEdit->keySequence().toString(QKeySequence::NativeText);
                    
                    // 如果设置了新的快捷键，检查是否有重复
                    if (!shortcut.isEmpty()) {
                        for (int i = 0; i < listWidget->count(); ++i) {
                            QListWidgetItem *otherItem = listWidget->item(i);
                            if (otherItem == item) continue;

                            QString otherShortcut = otherItem->data(Qt::UserRole + 1).toString();
                            if (otherShortcut == shortcut) {
                                QString conflictName = otherItem->data(Qt::UserRole).toString();
                                QToolTip::showText(QCursor::pos(), QString("快捷键 '%1' 已被 '%2' 占用，请更换").arg(shortcut, conflictName));
                                return;
                            }
                        }
                    }

                    dialog.accept();
                });

                connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

                vLayout->addWidget(box);

                if (dialog.exec() == QDialog::Accepted) {
                    QString keySequence = keyEdit->keySequence().toString(QKeySequence::NativeText);
                    item->setData(Qt::UserRole + 1, keySequence);
                    
                    QString displayText = name;
                    if (!keySequence.isEmpty()) displayText += QString(" [%1]").arg(keySequence);
                    item->setText(displayText);
                }
            });
        }

        auto addItem = [&](const QString &name, bool checked, const QString &shortcut) {
            QString displayText = name;
            if (!shortcut.isEmpty())
                displayText += QString(" [%1]").arg(shortcut);

            QListWidgetItem *item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, name);
            item->setData(Qt::UserRole + 1, shortcut);

            item->setFlags(item->flags() ^ (checkable ? Qt::NoItemFlags : Qt::ItemIsUserCheckable));
            if (checkable) item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
            listWidget->addItem(item);
        };

        QJsonArray array = settings->value(key).toJsonArray();
        QSet<QString> loadedKeys;

        if (array.isEmpty()) {
            for (const auto &t : defaults) addItem(t, true, defaultShortcuts.value(t));
        } else {
            for (const auto &val : array) {
                QJsonObject obj = val.toObject();
                QString name = obj["name"].toString();
                QString savedShortcut = obj["shortcut"].toString(defaultShortcuts.value(name));
                
                if (!name.isEmpty() && defaults.contains(name)) {
                    addItem(name, obj["enable"].toBool(), savedShortcut);
                    loadedKeys.insert(name);
                }
            }
            // 补充缺失的默认项
            for (const auto &def : defaults) {
                if (!loadedKeys.contains(def)) addItem(def, true, defaultShortcuts.value(def));
            }
        }

        if (listWidget->count() > 0) {
            int h = listWidget->sizeHintForRow(0) * listWidget->count() + listWidget->frameWidth() * 2;
            listWidget->setFixedHeight(h);
        }

        groupLayout->addWidget(titleLabel);
        groupLayout->addWidget(listWidget);
        groupLayout->addStretch();

        parentLayout->addLayout(groupLayout);

        auto saveFunc = [=]() {
            QJsonArray jsonArray;
            for (int i = 0; i < listWidget->count(); ++i) {
                QListWidgetItem *it = listWidget->item(i);
                bool checked = !checkable || it->checkState() == Qt::Checked;
                
                QJsonObject obj;
                obj["name"] = it->data(Qt::UserRole).toString();
                QString shortcut = it->data(Qt::UserRole + 1).toString();
                if(!shortcut.isEmpty()) obj["shortcut"] = shortcut;

                obj["enable"] = checked;
                jsonArray.append(obj);
            }
            settings->setValue(key, jsonArray);
            emit configurationChanged(key);
        };

        connect(listWidget->model(), &QAbstractItemModel::rowsMoved, saveFunc);
        connect(listWidget, &QListWidget::itemChanged, [=](QListWidgetItem *item) {
            QString originalName = item->data(Qt::UserRole).toString();
            if (originalName == "⚙️设置" && item->checkState() == Qt::Unchecked) {
                QToolTip::showText(QCursor::pos(), "[⚙️设置]不可隐藏");
                const QSignalBlocker blocker(listWidget); 
                item->setCheckState(Qt::Checked);
            } else {
                saveFunc();
            }
        });
    }

    void addSettingGroup(QBoxLayout *parentLayout, const QString &key, const QString &title, const QStringList &options, int defaultIndex)
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
        optionsLayout->setSpacing(5);

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

        connect(group, &QButtonGroup::idClicked, [=](int id){
            settings->setValue(key, id);
            emit configurationChanged(key);
        });

        optionsLayout->addStretch();
        groupLayout->addWidget(titleLabel);
        groupLayout->addLayout(optionsLayout);
        parentLayout->addLayout(groupLayout);
    }
};
