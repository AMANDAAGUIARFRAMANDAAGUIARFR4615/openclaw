#pragma once

#include <QApplication>
#include <QStyleHints>
#include <QString>
#include <QColor>
#include <QLatin1String>
#include <utility>

/**
 * 全局 UI 主题中心。
 *
 * 设计语言取自登录页与主窗口：主色 #4A86F7、12px 圆角卡片、柔和描边与浅阴影，
 * 同时提供浅色 / 深色两套语义化色板。各处界面统一引用本文件的色板与样式片段，
 * 即可保证整体风格一致并自动适配明暗模式。
 *
 * 颜色主题在启动时根据系统/设置固定（切换需重启），因此色板可安全缓存。
 */
namespace Theme {

/** 语义化色板：所有界面统一引用，避免散落的硬编码颜色。 */
struct Palette {
    // 品牌主色
    QString primary;
    QString primaryHover;
    QString primaryPressed;
    QString primaryDisabled;
    QString primarySoft;      // 主色的半透明背景（悬停/选中态）
    QString primarySoftHover; // 主色的半透明背景（更明显）
    QString onPrimary;        // 主色之上的前景色

    // 语义色
    QString danger;
    QString dangerHover;
    QString dangerSoft;
    QString success;
    QString warning;
    QString info;

    // 背景层级
    QString pageBg;       // 窗口/页面底色
    QString surface;      // 卡片/输入框等前景面
    QString surfaceAlt;   // 次级面（分组、表头、禁用态）
    QString surfaceHover; // 悬停态浅色面

    // 描边与分隔
    QString border;
    QString borderStrong;
    QString divider;

    // 文本
    QString textPrimary;
    QString textSecondary;
    QString textMuted;
    QString placeholder;

    // 控件细节
    QString inputBg;
    QString selection;
    QString selectionText;
    QString scrollHandle;
    QString scrollHandleHover;
    QString menuBg;
};

inline bool isDark()
{
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

inline const Palette &lightPalette()
{
    static const Palette p{
        .primary = "#4A86F7",
        .primaryHover = "#3B78EE",
        .primaryPressed = "#2F6AE0",
        .primaryDisabled = "#AFC6F8",
        .primarySoft = "rgba(74, 134, 247, 28)",
        .primarySoftHover = "rgba(74, 134, 247, 48)",
        .onPrimary = "#FFFFFF",

        .danger = "#E74C3C",
        .dangerHover = "#D44334",
        .dangerSoft = "rgba(231, 76, 60, 28)",
        .success = "#22C55E",
        .warning = "#F59E0B",
        .info = "#4A86F7",

        .pageBg = "#F0F3F8",
        .surface = "#FFFFFF",
        .surfaceAlt = "#F4F7FB",
        .surfaceHover = "#EAF1FB",

        .border = "#E2E8F0",
        .borderStrong = "#CBD5E1",
        .divider = "#EAEEF4",

        .textPrimary = "#1E293B",
        .textSecondary = "#475569",
        .textMuted = "#94A3B8",
        .placeholder = "#9AA8BC",

        .inputBg = "#FFFFFF",
        .selection = "#4A86F7",
        .selectionText = "#FFFFFF",
        .scrollHandle = "#CBD5E1",
        .scrollHandleHover = "#A9B7C9",
        .menuBg = "#FFFFFF",
    };
    return p;
}

inline const Palette &darkPalette()
{
    static const Palette p{
        .primary = "#4A86F7",
        .primaryHover = "#5C95F9",
        .primaryPressed = "#3B78EE",
        .primaryDisabled = "#3A4A6B",
        .primarySoft = "rgba(74, 134, 247, 40)",
        .primarySoftHover = "rgba(74, 134, 247, 64)",
        .onPrimary = "#FFFFFF",

        .danger = "#F87171",
        .dangerHover = "#EF5350",
        .dangerSoft = "rgba(248, 113, 113, 40)",
        .success = "#34D399",
        .warning = "#FBBF24",
        .info = "#4A86F7",

        .pageBg = "#0F1117",
        .surface = "#1F232B",
        .surfaceAlt = "#252B35",
        .surfaceHover = "#2A2F3A",

        .border = "#3A3F4B",
        .borderStrong = "#4A5568",
        .divider = "#2A2F3A",

        .textPrimary = "#E2E8F0",
        .textSecondary = "#C8D1DC",
        .textMuted = "#8B95A8",
        .placeholder = "#6B7C93",

        .inputBg = "#252B35",
        .selection = "#4A86F7",
        .selectionText = "#FFFFFF",
        .scrollHandle = "#3A3F4B",
        .scrollHandleHover = "#4A5568",
        .menuBg = "#1F232B",
    };
    return p;
}

inline const Palette &palette()
{
    return isDark() ? darkPalette() : lightPalette();
}

// —— 便捷访问器 ——
inline QString primary() { return palette().primary; }
inline QString primaryHover() { return palette().primaryHover; }
inline QString primaryPressed() { return palette().primaryPressed; }
inline QString danger() { return palette().danger; }
inline QString success() { return palette().success; }
inline QString warning() { return palette().warning; }
inline QString pageBg() { return palette().pageBg; }
inline QString surface() { return palette().surface; }
inline QString surfaceAlt() { return palette().surfaceAlt; }
inline QString border() { return palette().border; }
inline QString textPrimary() { return palette().textPrimary; }
inline QString textSecondary() { return palette().textSecondary; }
inline QString textMuted() { return palette().textMuted; }

/** 用 @{token} 占位符把模板里的颜色替换为当前色板。 */
inline QString fill(QString tpl)
{
    const Palette &p = palette();
    const std::pair<const char *, QString> map[]{
        {"primary", p.primary},
        {"primaryHover", p.primaryHover},
        {"primaryPressed", p.primaryPressed},
        {"primaryDisabled", p.primaryDisabled},
        {"primarySoft", p.primarySoft},
        {"primarySoftHover", p.primarySoftHover},
        {"onPrimary", p.onPrimary},
        {"danger", p.danger},
        {"dangerHover", p.dangerHover},
        {"dangerSoft", p.dangerSoft},
        {"success", p.success},
        {"warning", p.warning},
        {"info", p.info},
        {"pageBg", p.pageBg},
        {"surface", p.surface},
        {"surfaceAlt", p.surfaceAlt},
        {"surfaceHover", p.surfaceHover},
        {"border", p.border},
        {"borderStrong", p.borderStrong},
        {"divider", p.divider},
        {"textPrimary", p.textPrimary},
        {"textSecondary", p.textSecondary},
        {"textMuted", p.textMuted},
        {"placeholder", p.placeholder},
        {"inputBg", p.inputBg},
        {"selection", p.selection},
        {"selectionText", p.selectionText},
        {"scrollHandle", p.scrollHandle},
        {"scrollHandleHover", p.scrollHandleHover},
        {"menuBg", p.menuBg},
    };
    for (const auto &entry : map)
        tpl.replace(QLatin1String("@{") + QLatin1String(entry.first) + QLatin1String("}"), entry.second);
    return tpl;
}

/** 主行动按钮（蓝色填充）。可传入对象名以限定作用域。 */
inline QString primaryButtonQss(const QString &selector = QStringLiteral("QPushButton"))
{
    return fill(QStringLiteral(R"(
        %1 {
            background-color: @{primary};
            color: @{onPrimary};
            border: 1px solid @{primary};
            border-radius: 8px;
            padding: 7px 18px;
            font-weight: 600;
        }
        %1:hover { background-color: @{primaryHover}; border-color: @{primaryHover}; }
        %1:pressed { background-color: @{primaryPressed}; border-color: @{primaryPressed}; }
        %1:disabled { background-color: @{primaryDisabled}; border-color: @{primaryDisabled}; color: @{onPrimary}; }
    )").arg(selector));
}

/** 危险/删除按钮（红色填充）。 */
inline QString dangerButtonQss(const QString &selector = QStringLiteral("QPushButton"))
{
    return fill(QStringLiteral(R"(
        %1 {
            background-color: @{danger};
            color: #FFFFFF;
            border: 1px solid @{danger};
            border-radius: 8px;
            padding: 7px 18px;
            font-weight: 600;
        }
        %1:hover { background-color: @{dangerHover}; border-color: @{dangerHover}; }
        %1:disabled { background-color: @{dangerSoft}; border-color: @{dangerSoft}; }
    )").arg(selector));
}

/** 卡片容器样式。 */
inline QString cardQss(const QString &selector, int radius = 12)
{
    return fill(QStringLiteral(R"(
        %1 {
            background-color: @{surface};
            border: 1px solid @{border};
            border-radius: %2px;
        }
    )").arg(selector).arg(radius));
}

/**
 * 应用级全局样式表：统一所有标准控件（按钮、输入框、下拉、复选/单选、分组框、
 * 选项卡、滚动条、菜单、提示、进度条、滑块、表头、列表等）的外观与明暗适配。
 * 各窗口自定义的 setStyleSheet 仍可在此基础上覆盖局部样式。
 */
inline QString globalStyleSheet()
{
    return fill(QStringLiteral(R"QSS(
        /* ===== 通用对话框底色 ===== */
        QDialog, QMessageBox, QInputDialog, QFileDialog, QColorDialog, QProgressDialog {
            background-color: @{pageBg};
        }

        /* ===== 工具提示 ===== */
        QToolTip {
            background-color: @{surface};
            color: @{textPrimary};
            border: 1px solid @{border};
            border-radius: 6px;
            padding: 5px 8px;
        }

        /* ===== 按钮 ===== */
        QPushButton {
            background-color: @{surface};
            color: @{textPrimary};
            border: 1px solid @{borderStrong};
            border-radius: 8px;
            padding: 6px 16px;
            min-height: 18px;
        }
        QPushButton:hover { background-color: @{surfaceHover}; border-color: @{primary}; }
        QPushButton:pressed { background-color: @{primarySoft}; }
        QPushButton:disabled { color: @{textMuted}; background-color: @{surfaceAlt}; border-color: @{border}; }
        QPushButton:checked { background-color: @{primarySoft}; border-color: @{primary}; color: @{primary}; }
        QPushButton:default {
            background-color: @{primary};
            color: @{onPrimary};
            border: 1px solid @{primary};
            font-weight: 600;
        }
        QPushButton:default:hover { background-color: @{primaryHover}; border-color: @{primaryHover}; }
        QPushButton:default:pressed { background-color: @{primaryPressed}; border-color: @{primaryPressed}; }
        QPushButton:default:disabled { background-color: @{primaryDisabled}; border-color: @{primaryDisabled}; color: @{onPrimary}; }

        /* ===== 工具按钮 ===== */
        QToolButton {
            background: transparent;
            border: none;
            border-radius: 6px;
            padding: 4px;
        }
        QToolButton:hover { background-color: @{primarySoft}; }
        QToolButton:pressed, QToolButton:checked { background-color: @{primarySoftHover}; }

        /* ===== 文本输入 ===== */
        QLineEdit {
            background-color: @{inputBg};
            color: @{textPrimary};
            border: 1px solid @{borderStrong};
            border-radius: 8px;
            padding: 5px 10px;
            selection-background-color: @{primary};
            selection-color: @{selectionText};
        }
        QLineEdit:focus { border: 1px solid @{primary}; }
        QLineEdit:disabled { background-color: @{surfaceAlt}; color: @{textMuted}; }
        QLineEdit[readOnly="true"] { background-color: @{surfaceAlt}; }

        QPlainTextEdit, QTextEdit, QTextBrowser {
            background-color: @{inputBg};
            color: @{textPrimary};
            border: 1px solid @{border};
            border-radius: 8px;
            selection-background-color: @{primary};
            selection-color: @{selectionText};
        }
        QPlainTextEdit:focus, QTextEdit:focus { border: 1px solid @{primary}; }

        /* 复合控件内部编辑器不再叠加边框/背景，避免出现双重描边 */
        QComboBox QLineEdit, QAbstractSpinBox QLineEdit {
            border: none; background: transparent; padding: 0; border-radius: 0;
            selection-background-color: @{primary}; selection-color: @{selectionText};
        }

        /* ===== 数字输入框 ===== */
        QSpinBox, QDoubleSpinBox {
            background-color: @{inputBg};
            color: @{textPrimary};
            border: 1px solid @{borderStrong};
            border-radius: 8px;
            padding: 4px 8px;
            selection-background-color: @{primary};
            selection-color: @{selectionText};
        }
        QSpinBox:focus, QDoubleSpinBox:focus { border: 1px solid @{primary}; }
        QSpinBox::up-button, QDoubleSpinBox::up-button {
            subcontrol-origin: border; subcontrol-position: top right;
            width: 18px; border-left: 1px solid @{border}; border-top-right-radius: 8px;
        }
        QSpinBox::down-button, QDoubleSpinBox::down-button {
            subcontrol-origin: border; subcontrol-position: bottom right;
            width: 18px; border-left: 1px solid @{border}; border-bottom-right-radius: 8px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover,
        QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover { background-color: @{primarySoft}; }
        QSpinBox::up-arrow, QDoubleSpinBox::up-arrow {
            width: 0; height: 0; image: none;
            border-left: 4px solid transparent; border-right: 4px solid transparent;
            border-bottom: 5px solid @{textSecondary};
        }
        QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {
            width: 0; height: 0; image: none;
            border-left: 4px solid transparent; border-right: 4px solid transparent;
            border-top: 5px solid @{textSecondary};
        }

        /* ===== 下拉框 ===== */
        QComboBox {
            background-color: @{inputBg};
            color: @{textPrimary};
            border: 1px solid @{borderStrong};
            border-radius: 8px;
            padding: 5px 10px;
            min-height: 18px;
        }
        QComboBox:hover { border-color: @{primary}; }
        QComboBox:focus, QComboBox:on { border: 1px solid @{primary}; }
        QComboBox:disabled { background-color: @{surfaceAlt}; color: @{textMuted}; }
        QComboBox::drop-down { border: none; width: 24px; }
        QComboBox::down-arrow {
            width: 0; height: 0; image: none;
            border-left: 5px solid transparent; border-right: 5px solid transparent;
            border-top: 6px solid @{textSecondary}; margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: @{menuBg};
            color: @{textPrimary};
            border: 1px solid @{border};
            border-radius: 8px;
            padding: 4px;
            selection-background-color: @{primary};
            selection-color: @{selectionText};
            outline: none;
        }

        /* ===== 复选框 / 单选框 ===== */
        QCheckBox, QRadioButton { color: @{textPrimary}; spacing: 8px; }
        QCheckBox::indicator, QRadioButton::indicator { width: 18px; height: 18px; }
        QCheckBox::indicator {
            border-radius: 4px; border: 1px solid @{borderStrong}; background: @{inputBg};
        }
        QCheckBox::indicator:hover { border-color: @{primary}; }
        QCheckBox::indicator:checked {
            background: @{primary}; border-color: @{primary}; image: url(:/icons/check_white.svg);
        }
        QCheckBox::indicator:disabled { background: @{surfaceAlt}; border-color: @{border}; }
        QRadioButton::indicator {
            border-radius: 9px; border: 1px solid @{borderStrong}; background: @{inputBg};
        }
        QRadioButton::indicator:hover { border-color: @{primary}; }
        QRadioButton::indicator:checked { border: 5px solid @{primary}; background: #FFFFFF; }

        /* ===== 分组框 ===== */
        QGroupBox {
            background-color: @{surface};
            border: 1px solid @{border};
            border-radius: 10px;
            margin-top: 14px;
            padding: 12px;
            font-weight: 600;
            color: @{textPrimary};
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px; padding: 0 6px;
            color: @{textSecondary};
        }

        /* ===== 选项卡 ===== */
        QTabWidget::pane {
            border: 1px solid @{border};
            border-radius: 10px;
            top: -1px;
            background: @{surface};
        }
        QTabBar::tab {
            background: @{surfaceAlt};
            color: @{textSecondary};
            border: 1px solid @{border};
            border-bottom: none;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            padding: 7px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:hover { color: @{textPrimary}; background: @{surfaceHover}; }
        QTabBar::tab:selected { background: @{surface}; color: @{primary}; font-weight: 600; }

        /* ===== 滚动条 ===== */
        QScrollBar:vertical { background: transparent; width: 11px; margin: 2px; }
        QScrollBar::handle:vertical { background: @{scrollHandle}; border-radius: 5px; min-height: 30px; }
        QScrollBar::handle:vertical:hover { background: @{scrollHandleHover}; }
        QScrollBar:horizontal { background: transparent; height: 11px; margin: 2px; }
        QScrollBar::handle:horizontal { background: @{scrollHandle}; border-radius: 5px; min-width: 30px; }
        QScrollBar::handle:horizontal:hover { background: @{scrollHandleHover}; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: none; border: none; }
        QScrollBar::add-page, QScrollBar::sub-page { background: none; }
        QScrollBar::corner { background: transparent; }

        /* ===== 菜单 ===== */
        QMenuBar { background-color: @{surface}; color: @{textPrimary}; border-bottom: 1px solid @{border}; }
        QMenuBar::item { padding: 6px 12px; background: transparent; border-radius: 6px; }
        QMenuBar::item:selected { background: @{primarySoft}; color: @{primary}; }
        QMenu {
            background-color: @{menuBg};
            border: 1px solid @{border};
            border-radius: 8px;
            padding: 6px;
        }
        QMenu::item { padding: 6px 28px 6px 14px; border-radius: 6px; color: @{textPrimary}; }
        QMenu::item:selected { background-color: @{primary}; color: @{onPrimary}; }
        QMenu::item:disabled { color: @{textMuted}; }
        QMenu::separator { height: 1px; background: @{divider}; margin: 5px 8px; }

        /* ===== 进度条 ===== */
        QProgressBar {
            border: 1px solid @{border};
            border-radius: 8px;
            background: @{surfaceAlt};
            text-align: center;
            color: @{textPrimary};
            min-height: 16px;
        }
        QProgressBar::chunk { background-color: @{primary}; border-radius: 7px; }

        /* ===== 滑块 ===== */
        QSlider::groove:horizontal { height: 4px; background: @{surfaceAlt}; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: @{primary}; border-radius: 2px; }
        QSlider::handle:horizontal {
            width: 14px; height: 14px; margin: -6px 0;
            background: @{surface}; border: 2px solid @{primary}; border-radius: 8px;
        }
        QSlider::groove:vertical { width: 4px; background: @{surfaceAlt}; border-radius: 2px; }
        QSlider::add-page:vertical { background: @{primary}; border-radius: 2px; }
        QSlider::handle:vertical {
            width: 14px; height: 14px; margin: 0 -6px;
            background: @{surface}; border: 2px solid @{primary}; border-radius: 8px;
        }

        /* ===== 表格 / 列表 / 树 ===== */
        QHeaderView::section {
            background-color: @{surfaceAlt};
            color: @{textSecondary};
            border: none;
            border-bottom: 1px solid @{border};
            padding: 6px 10px;
            font-weight: 600;
        }
        QTableView, QTableWidget {
            gridline-color: @{divider};
            selection-background-color: @{primary};
            selection-color: @{selectionText};
            alternate-background-color: @{surfaceAlt};
        }
        QTableCornerButton::section { background: @{surfaceAlt}; border: none; }
        QAbstractItemView { selection-background-color: @{primary}; selection-color: @{selectionText}; outline: none; }
    )QSS"));
}

} // namespace Theme
