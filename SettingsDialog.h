#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QFont>
#include <QStringList>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        // 设置窗口基本属性
        setWindowTitle("设置");
        resize(350, 520); 

        // 主垂直布局
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(15);               // 组与组之间的间距
        mainLayout->setContentsMargins(25, 20, 25, 20); // 边距

        // 1. 主界面投屏显示设置
        addSettingGroup(mainLayout, "主界面投屏显示设置", 
                        {"竖屏显示", "横屏显示"}, 0);

        // 2. 脚本运行设置
        addSettingGroup(mainLayout, "脚本运行设置", 
                        {"开启震动", "关闭震动"}, 0);

        // 3. 日志保存设置
        addSettingGroup(mainLayout, "日志保存设置", 
                        {"开启", "关闭"}, 1); // 默认选第2项(关闭)

        // 4. 脚本日志设置
        addSettingGroup(mainLayout, "脚本日志设置", 
                        {"1天内", "7天内", "30天内"}, 1); // 默认选第2项(7天内)

        // 5. 跨屏窗口大小记录
        addSettingGroup(mainLayout, "跨屏窗口大小记录", 
                        {"开启", "关闭"}, 1); // 默认选第2项(关闭)

        // 6. 跨屏窗口自动进横竖屏旋转
        addSettingGroup(mainLayout, "跨屏窗口自动进横竖屏旋转", 
                        {"开启", "关闭"}, 0);

        // 7. 局域网自动扫描内外设备设置
        // 图片中红圈圈了“关闭”，但蓝色选中点是在“开启”。
        // 这里 checkedIndex 设为 0 (开启)。如果需要默认关闭，改为 1 即可。
        addSettingGroup(mainLayout, "局域网自动扫描内外设备设置", 
                        {"开启", "关闭"}, 0); 

        // 8. 主界面投屏方式设置
        addSettingGroup(mainLayout, "主界面投屏方式设置", 
                        {"图片展示", "视频流展示"}, 1); // 默认选第2项(视频流)

        // 底部弹簧，将控件向上顶
        mainLayout->addStretch();

        setModal(true);
        exec();
    }

    ~SettingsDialog() = default;

private:
    /**
     * @brief 辅助函数：添加一组设置项
     * @param parentLayout 主布局指针
     * @param title 标题文本
     * @param options 选项文本列表
     * @param checkedIndex 默认选中的索引 (0开始)
     */
    void addSettingGroup(QVBoxLayout *parentLayout, const QString &title, const QStringList &options, int checkedIndex)
    {
        // 1. 创建标题
        QLabel *titleLabel = new QLabel(title, this);
        QFont font = titleLabel->font();
        font.setBold(true);      // 粗体
        font.setPointSize(9);    // 字号，可按需调整
        titleLabel->setFont(font);

        // 2. 创建选项布局 (水平)
        QHBoxLayout *optionsLayout = new QHBoxLayout();
        optionsLayout->setContentsMargins(0, 5, 0, 0); // 标题和选项间稍微留点空隙
        optionsLayout->setSpacing(20);                 // 选项之间的间距

        // 3. 创建按钮组 (互斥逻辑)
        // QButtonGroup 不需要添加到布局中，它只是逻辑分组
        QButtonGroup *group = new QButtonGroup(this);

        for (int i = 0; i < options.size(); ++i) {
            QRadioButton *radio = new QRadioButton(options[i], this);
            
            // 设置选中状态
            if (i == checkedIndex) {
                radio->setChecked(true);
            }

            // 添加到逻辑组和UI布局
            group->addButton(radio, i);
            optionsLayout->addWidget(radio);
        }

        // 添加水平弹簧，确保单选框靠左对齐
        optionsLayout->addStretch();

        // 3. 添加到父布局
        parentLayout->addWidget(titleLabel);
        parentLayout->addLayout(optionsLayout);
    }
};
