#pragma once

#include "../Theme.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QToolButton>
#include <QLabel>
#include <QFrame>
#include <QStyle>
#include <QApplication>
#include <QStyleHints>
#include <QPainter>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QScroller>
#include <QShowEvent>
#include <QEvent>
#include <QScopedValueRollback>

class AdaptiveScrollArea : public QScrollArea {
public:
    using QScrollArea::QScrollArea;

    // 强制让 ScrollArea 的推荐大小等于其内部部件的大小
    QSize sizeHint() const override {
        if (widget())
            return widget()->sizeHint() + QSize(2, 2); // 加上边框微调
        
        return QScrollArea::sizeHint();
    }
};

class BaseDialog : public QDialog {
    Q_OBJECT
public:
    /// @param withScrollArea 是否在内容区外包一层可滚动区域（默认 true）；为 false 时子类自行处理滚动。
    explicit BaseDialog(const QString& title, QWidget *parent = nullptr, bool withScrollArea = true)
        : QDialog(parent)
    {
        setWindowTitle(title);

        const Theme::Palette &theme = Theme::palette();

        this->setStyleSheet(QString("QDialog { background-color: %1; }").arg(theme.pageBg));

        m_mainLayout = new QVBoxLayout(this);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);
        setWindowState(Qt::WindowMaximized);

        // --- 移动端特有：自定义顶部导航栏 ---
        auto navBarWidget = new QWidget(this);
        navBarWidget->setFixedHeight(54);

        navBarWidget->setStyleSheet(QString("background-color: %1;").arg(theme.surface));

        auto navBarLayout = new QHBoxLayout(navBarWidget);
        navBarLayout->setContentsMargins(10, 0, 10, 0);
        navBarLayout->setSpacing(5);

        auto backBtn = new QToolButton(this);
        
        // 动态重绘系统默认的返回图标颜色
        QIcon defaultIcon = style()->standardIcon(QStyle::SP_ArrowBack);
        QPixmap pixmap = defaultIcon.pixmap(48, 48); // 获取稍大尺寸以保证在移动端高分屏清晰
        if (!pixmap.isNull()) {
            QPainter painter(&pixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            // 暗色模式用亮色图标，浅色模式用深色图标
            QColor iconColor = QColor(theme.textPrimary);
            painter.fillRect(pixmap.rect(), iconColor);
            painter.end();
            backBtn->setIcon(QIcon(pixmap));
        } else {
            backBtn->setIcon(defaultIcon); // 降级处理
        }
        
        backBtn->setIconSize(QSize(28, 28));
        backBtn->setStyleSheet("border: none;"); // 去掉边框
        backBtn->setFixedSize(44, 44); // 保证手指点击区域足够大

        auto titleLabel = new QLabel(title, this);
        titleLabel->setAlignment(Qt::AlignCenter);
        QFont font = titleLabel->font();
        font.setBold(true);
        font.setPointSize(14);
        titleLabel->setFont(font);

        titleLabel->setStyleSheet(QString("color: %1;").arg(theme.textPrimary));

        // 右侧占位符：为了让中间的标题绝对居中，右侧需要一个和左侧按钮等宽的空对象
        auto rightSpacer = new QWidget(this);
        rightSpacer->setFixedSize(44, 44);

        navBarLayout->addWidget(backBtn);
        navBarLayout->addWidget(titleLabel, 1);
        navBarLayout->addWidget(rightSpacer);

        m_mainLayout->addWidget(navBarWidget);

        // 导航栏底部分割线
        auto line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFixedHeight(1);

        line->setStyleSheet(QString("background-color: %1; border: none;").arg(theme.divider));
        
        m_mainLayout->addWidget(line);

        connect(backBtn, &QToolButton::clicked, this, &QDialog::reject);
#else
        m_mainLayout->setContentsMargins(10, 10, 10, 10);
#endif

        AdaptiveScrollArea *scrollArea = withScrollArea ? new AdaptiveScrollArea(this) : nullptr;
        if (scrollArea) {
            scrollArea->setWidgetResizable(true);
            scrollArea->setFrameShape(QFrame::NoFrame);
            scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            // 对话框内容始终保持纵向布局，禁用横向滚动条，避免余额支付等动态显隐导致的水平滚动条
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
            QScrollerProperties props = QScroller::scroller(scrollArea->viewport())->scrollerProperties();
            props.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.1);
            props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
            props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
            QScroller::scroller(scrollArea->viewport())->setScrollerProperties(props);
        }

        auto *contentWidget = new QWidget(this);
        m_contentWidget = contentWidget;
        m_scrollArea = scrollArea;
        m_contentLayout = new QVBoxLayout(contentWidget);
        m_contentLayout->setContentsMargins(15, 15, 15, 15);
        if (scrollArea)
            scrollArea->setWidget(contentWidget);

        m_mainLayout->addWidget(scrollArea ? scrollArea : contentWidget, 1);

        // 监听内容布局变化（如动态显隐控件），实时保证宽度足够，杜绝横向滚动条
        contentWidget->installEventFilter(this);
    }

    // 提供给子类获取内容布局的接口
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

protected:
    // 保证对话框宽度足以容纳内容的最小宽度，从根本上避免出现横向滚动条
    void ensureContentFitsWidth()
    {
#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)
        if (!m_contentWidget || m_adjustingWidth)
            return;

        QScopedValueRollback<bool> guard(m_adjustingWidth, true);

        int needed = m_contentWidget->minimumSizeHint().width();
        if (needed <= 0)
            needed = m_contentWidget->sizeHint().width();

        // 主布局左右边距
        int leftMargin = 0, rightMargin = 0;
        if (m_mainLayout)
            m_mainLayout->getContentsMargins(&leftMargin, nullptr, &rightMargin, nullptr);
        needed += leftMargin + rightMargin;

        // 预留纵向滚动条宽度，纵向滚动出现时也不会挤出横向滚动条
        if (m_scrollArea)
            needed += m_scrollArea->verticalScrollBar()->sizeHint().width() + 2;

        if (minimumWidth() < needed)
            setMinimumWidth(needed);
        if (width() < needed)
            resize(needed, height());
#endif
    }

    void showEvent(QShowEvent *event) override
    {
        QDialog::showEvent(event);
        ensureContentFitsWidth();
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_contentWidget && event->type() == QEvent::LayoutRequest)
            ensureContentFitsWidth();
        return QDialog::eventFilter(watched, event);
    }

private:
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_contentLayout;
    QWidget* m_contentWidget = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    bool m_adjustingWidth = false;
};
