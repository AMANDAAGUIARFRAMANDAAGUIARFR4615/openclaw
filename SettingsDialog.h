#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFont>
#include <QStringList>
#include <QListWidget>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("设置");

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(15);
        mainLayout->setContentsMargins(25, 20, 25, 20);

        addSettingGroup(mainLayout, "主窗口投屏显示【分组单独设置后不受此选项影响】", {"竖屏显示", "横屏显示"}, 0);

        addSortableGroup(mainLayout, "分组标签页右键菜单 (拖拽调整)", {"横竖屏切换", "重命名分组", "添加分组", "删除分组"});
        addSortableGroup(mainLayout, "投屏窗口右键菜单 (拖拽调整)", {"🏠主屏幕", "🧹清理应用", "📁文件管理", "⏺️录制+回放", "🧩应用列表", "📸截图", "🔄重启", "🔒锁屏", "🗑️清空相册", "🔊音量+", "🔈 音量-", "🔧修改分组"});

        addSettingGroup(mainLayout, "局域网自动扫描设备", {"开启", "关闭"}, 0);
        addSettingGroup(mainLayout, "默认连接方式【分组单独设置后不受此选项影响】", {"WIFI优先", "USB优先"}, 0);
        addSettingGroup(mainLayout, "默认视频清晰度【分组单独设置后不受此选项影响】", {"图片流", "标清", "高清", "超清"}, 1);

        setModal(true);
        exec();
    }

    ~SettingsDialog() = default;

private:
    void addSortableGroup(QVBoxLayout *parentLayout, const QString &title, const QStringList &options)
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
        
        for (const QString &text : options) {
            QListWidgetItem *item = new QListWidgetItem(text);
            listWidget->addItem(item);
        }

        int totalHeight = (listWidget->fontMetrics().height() + 4) * options.size();
        listWidget->setFixedHeight(totalHeight);

        groupLayout->addWidget(titleLabel);
        groupLayout->addWidget(listWidget);

        parentLayout->addLayout(groupLayout);
    }

    void addSettingGroup(QVBoxLayout *parentLayout, const QString &title, const QStringList &options, int checkedIndex)
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

        for (int i = 0; i < options.size(); ++i) {
            QRadioButton *radio = new QRadioButton(options[i], this);
            if (i == checkedIndex) {
                radio->setChecked(true);
            }
            group->addButton(radio, i);
            optionsLayout->addWidget(radio);
        }

        optionsLayout->addStretch();

        groupLayout->addWidget(titleLabel);
        groupLayout->addLayout(optionsLayout);

        parentLayout->addLayout(groupLayout);
    }
};