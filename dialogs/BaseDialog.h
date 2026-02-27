#pragma once

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
#include <QScroller>

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
    explicit BaseDialog(const QString& title, QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(title);

        bool isDarkMode = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;

        QString bgColor = isDarkMode ? "#121212" : "#F4F5F7";
        this->setStyleSheet(QString("QDialog { background-color: %1; }").arg(bgColor));

        m_mainLayout = new QVBoxLayout(this);

#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
        m_mainLayout->setContentsMargins(0, 0, 0, 0);
        m_mainLayout->setSpacing(0);
        setWindowState(Qt::WindowMaximized);

        // --- 移动端特有：自定义顶部导航栏 ---
        auto navBarWidget = new QWidget(this);
        navBarWidget->setFixedHeight(54);
        
        QString navBgColor = isDarkMode ? "#1E1E1E" : "#ffffff";
        navBarWidget->setStyleSheet(QString("background-color: %1;").arg(navBgColor));

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
            QColor iconColor = isDarkMode ? QColor("#E0E0E0") : QColor("#303133");
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

        QString titleColor = isDarkMode ? "#E0E0E0" : "#303133";
        titleLabel->setStyleSheet(QString("color: %1;").arg(titleColor));

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

        QString lineColor = isDarkMode ? "#2D2D30" : "#dddddd";
        line->setStyleSheet(QString("background-color: %1; border: none;").arg(lineColor));
        
        m_mainLayout->addWidget(line);

        connect(backBtn, &QToolButton::clicked, this, &QDialog::reject);
#else
        m_mainLayout->setContentsMargins(10, 10, 10, 10);
#endif

        auto mainScrollArea = new AdaptiveScrollArea(this);
        mainScrollArea->setWidgetResizable(true);
        mainScrollArea->setFrameShape(QFrame::NoFrame);
        mainScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        // 设置手势滚动（支持触屏或鼠标左键拖拽）
        QScroller::grabGesture(mainScrollArea->viewport(), QScroller::LeftMouseButtonGesture);
        
        QScrollerProperties mainProps = QScroller::scroller(mainScrollArea->viewport())->scrollerProperties();
        mainProps.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.1);
        QScroller::scroller(mainScrollArea->viewport())->setScrollerProperties(mainProps);

        auto scrollContentWidget = new QWidget(mainScrollArea);

        m_contentLayout = new QVBoxLayout(scrollContentWidget);
        m_contentLayout->setContentsMargins(15, 15, 15, 15);
        
        mainScrollArea->setWidget(scrollContentWidget);
        
        m_mainLayout->addWidget(mainScrollArea, 1);
    }

    // 提供给子类获取内容布局的接口
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

private:
    QVBoxLayout* m_mainLayout;
    QVBoxLayout* m_contentLayout;
};
