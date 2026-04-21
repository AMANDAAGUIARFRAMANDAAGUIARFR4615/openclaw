#pragma once

#include "BaseDialog.h"
#include <QTextBrowser>
#include <QStyleHints>

class HelpDialog : public BaseDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget *parent = nullptr) : BaseDialog("帮助", parent) {
        setMinimumSize(780, 640);

        auto textBrowser = new QTextBrowser(this);
        textBrowser->setOpenExternalLinks(true);
        textBrowser->setFrameShape(QFrame::NoFrame);

        const bool isDarkMode = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        // 暗色模式使用纯色背景，避免出现条纹/渐变感。
        const QString bgColor = isDarkMode ? "#14171D" : "#F5F8FD";
        const QString cardColor = isDarkMode ? "#14171D" : "#FFFFFF";
        const QString borderColor = isDarkMode ? "#2A3140" : "#DEE6F2";
        const QString titleColor = isDarkMode ? "#E8EEF9" : "#0F1D33";
        const QString subTitleColor = isDarkMode ? "#8CB7FF" : "#1B62D1";
        const QString bodyColor = isDarkMode ? "#C9D4E4" : "#2B3A4F";
        const QString strongColor = isDarkMode ? "#F2F6FF" : "#122540";
        const QString tipColor = isDarkMode ? "#8FB3EC" : "#1B4FB8";

        textBrowser->setStyleSheet(QString(R"(
            QTextBrowser {
                background-color: %1;
                border: none;
            }
            QTextBrowser::viewport {
                background-color: %1;
            }
            QTextBrowser > QWidget > QWidget {
                background-color: %1;
            }
        )").arg(bgColor));

        const QString helpContent = QString(R"(
            <style>
                body {
                    background: %1;
                    color: %2;
                    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "PingFang SC", "Microsoft YaHei", sans-serif;
                    margin: 0;
                }
                .wrap {
                    padding: 10px 14px 18px 14px;
                    background: %1;
                }
                .header {
                    margin-bottom: 10px;
                    padding: 2px 0 10px 0;
                    border-radius: 0;
                    border: none;
                    background: %1;
                }
                .header h2 {
                    margin: 0 0 6px 0;
                    color: %5;
                    font-size: 20px;
                }
                .header p {
                    margin: 0;
                    color: %2;
                    line-height: 1.6;
                }
                .card {
                    margin-top: 8px;
                    padding: 2px 0 2px 0;
                    border-radius: 0;
                    border: none;
                    background: %1;
                }
                .card + .card {
                    margin-top: 14px;
                    padding-top: 12px;
                    border-top: 1px solid %3;
                }
                .card h3 {
                    margin: 0 0 6px 0;
                    color: %6;
                    font-size: 16px;
                    font-weight: 700;
                }
                ul {
                    margin: 0;
                    padding-left: 24px;
                }
                li {
                    margin: 8px 0;
                    color: %2;
                    line-height: 1.55;
                }
                strong {
                    color: %7;
                }
                .tip {
                    margin-top: 10px;
                    padding: 0;
                    border: none;
                    color: %8;
                    font-weight: 700;
                    font-size: 14px;
                    line-height: 1.5;
                }
            </style>

            <div class="wrap">
                <div class="card">
                    <h3>1. 设备连接与管理</h3>
                    <ul>
                        <li><strong>连接方式：</strong>支持 USB 有线与 Wi-Fi 无线连接，可同时管理多台设备。</li>
                        <li><strong>分组管理：</strong>支持自定义分组，便于按场景、项目或机型分类。</li>
                        <li><strong>多分组归属：</strong>同一台设备可以加入多个分组，减少重复维护。</li>
                    </ul>
                    <div class="tip">提示：可在「设置 - 常规与投屏」中配置默认连接方式和自动连接策略。</div>
                </div>

                <div class="card">
                    <h3>2. 投屏显示与交互控制</h3>
                    <ul>
                        <li><strong>显示参数：</strong>支持竖屏/横屏、帧率、清晰度等维度设置。</li>
                        <li><strong>鼠标操作：</strong>左键可点击与滑动，滚轮可快速翻页浏览内容。</li>
                        <li><strong>键盘导航：</strong>方向键移动焦点，<strong>Shift + 方向键</strong> 可连续选中内容。</li>
                    </ul>
                </div>

                <div class="card">
                    <h3>3. 输入与剪贴板协同</h3>
                    <ul>
                        <li><strong>多语言输入：</strong>通过电脑键盘直接向手机输入中英文及更多语言内容。</li>
                        <li><strong>输入法切换：</strong>可使用 <strong>Ctrl + Space</strong> 快速切换手机端输入法状态。</li>
                        <li><strong>剪贴板互通：</strong><strong>Ctrl + C / V</strong> 支持电脑与手机间双向复制粘贴（文本/图片）。</li>
                        <li><strong>文本编辑：</strong><strong>Ctrl + Z / Y</strong> 支持撤销与重做，提升编辑效率。</li>
                    </ul>
                </div>

                <div class="card">
                    <h3>4. 文件与多媒体处理</h3>
                    <ul>
                        <li><strong>文件管理：</strong>支持重命名、删除、新建文件夹等常见操作。</li>
                        <li><strong>压缩处理：</strong>支持 Zip/Rar 文件压缩与解压，便于批量传输与归档。</li>
                        <li><strong>媒体流转：</strong>支持图片/视频导入手机相册，截图快速粘贴，投屏录像归档。</li>
                    </ul>
                </div>

                <div class="card">
                    <h3>5. 效率与自动化能力</h3>
                    <ul>
                        <li><strong>快捷键体系：</strong>常用功能支持快捷键，且可在设置中自定义。</li>
                        <li><strong>脚本录制回放：</strong>支持动作录制与复用，适合重复流程自动化执行。</li>
                        <li><strong>菜单自定义：</strong>可拖拽调整侧边栏和右键菜单顺序，贴合你的操作习惯。</li>
                    </ul>
                </div>
            </div>
        )")
            .arg(bgColor, bodyColor, borderColor, cardColor, titleColor, subTitleColor, strongColor, tipColor);

        textBrowser->setHtml(helpContent);
        contentLayout()->addWidget(textBrowser);
    }

    ~HelpDialog() override = default;
};
