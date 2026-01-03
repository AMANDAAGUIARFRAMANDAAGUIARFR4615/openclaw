#pragma once

#include <QWidget>
#include <QPropertyAnimation>
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

class SwitchButton : public QWidget
{
    Q_OBJECT
    // 声明属性，让动画框架能找到它
    Q_PROPERTY(double offset READ offset WRITE setOffset)

public:
    explicit SwitchButton(QWidget *parent = nullptr) : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor); // 鼠标变手型

        // --- 默认配置 ---
        m_checked = false;
        m_offset = 0.0;
        m_onText = "ON";
        m_offText = "OFF";
        m_onColor = QColor("#2ecc71");    // 绿色
        m_offColor = QColor("#bdc3c7");   // 灰色
        m_sliderColor = Qt::white;        // 滑块颜色
        m_textColor = Qt::white;          // 文字颜色

        m_anim = new QPropertyAnimation(this, "offset", this);
        m_anim->setDuration(200);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);
    }

    // 设置显示的文字
    void setTexts(const QString &onText, const QString &offText) {
        m_onText = onText;
        m_offText = offText;
        updateGeometry(); // 触发 sizeHint 重新计算
        update();
    }

    void setColors(const QColor &on, const QColor &off) {
        m_onColor = on;
        m_offColor = off;
        update();
    }

    // 获取状态
    bool isChecked() const { return m_checked; }

    void setChecked(bool checked) {
        if (m_checked == checked) return;
        m_checked = checked;
        // 不播放动画，直接到位
        m_offset = checked ? 1.0 : 0.0;
        emit toggled(checked);
        update();
    }

    QSize sizeHint() const override {
        QFontMetrics fm(font());
        // 计算两段文字中较长的一段
        int textW = qMax(fm.horizontalAdvance(m_onText), fm.horizontalAdvance(m_offText));

        int h = 30; // 默认高度
        // 宽度 = 高度(即滑块直径) + 文字宽 + 左右边距(20)
        int w = h + textW + 20;

        return QSize(w, h);
    }

    // 动画属性 getter
    double offset() const { return m_offset; }

    // 动画属性 setter (关键：更新数值并重绘)
    void setOffset(double o) {
        m_offset = o;
        update(); // 触发 paintEvent
    }

signals:
    void toggled(bool checked);

protected:
    // 1. 鼠标释放事件 (处理点击)
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            m_checked = !m_checked; // 切换状态
            emit toggled(m_checked);

            // 开始动画
            m_anim->stop();
            m_anim->setStartValue(m_offset);
            m_anim->setEndValue(m_checked ? 1.0 : 0.0);
            m_anim->start();
        }
        QWidget::mouseReleaseEvent(e);
    }

    // 2. 绘图事件
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int w = width();
        int h = height();
        int margin = 3;
        int sliderSize = h - 2 * margin;

        // --- A. 绘制背景 ---
        // 简单逻辑：如果 offset > 0.5 (偏向开)，用绿色；否则用灰色
        // 也可以做颜色插值，这里为了代码简洁直接切换
        QColor bgColor = (m_offset > 0.5) ? m_onColor : m_offColor;

        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, w, h, h/2, h/2);

        // --- B. 绘制文字 ---
        p.setPen(m_textColor);
        p.setFont(font());

        // 逻辑：
        // 状态开 (offset -> 1): 滑块在右，文字在左
        // 状态关 (offset -> 0): 滑块在左，文字在右
        if (m_offset > 0.5) {
            // 绘制 ON 文字 (在左侧空白区域)
            QRect textRect(0, 0, w - h, h);
            p.drawText(textRect, Qt::AlignCenter, m_onText);
        } else {
            // 绘制 OFF 文字 (在右侧空白区域)
            QRect textRect(h, 0, w - h, h);
            p.drawText(textRect, Qt::AlignCenter, m_offText);
        }

        // --- C. 绘制滑块 ---
        // 线性插值计算 X 坐标
        // 0.0 -> margin
        // 1.0 -> w - sliderSize - margin
        double startX = margin;
        double endX = w - sliderSize - margin;
        double currentX = startX + (endX - startX) * m_offset;

        p.setBrush(m_sliderColor);
        p.drawEllipse(QRectF(currentX, margin, sliderSize, sliderSize));
    }

private:
    bool m_checked;
    double m_offset; // 动画进度 0.0 ~ 1.0

    QString m_onText;
    QString m_offText;
    QColor m_onColor;
    QColor m_offColor;
    QColor m_sliderColor;
    QColor m_textColor;

    QPropertyAnimation *m_anim;
};
