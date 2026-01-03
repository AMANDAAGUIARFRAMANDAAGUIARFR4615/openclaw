#pragma once

#include <QWidget>
#include <QPropertyAnimation>
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

class SwitchButton : public QWidget
{
    Q_OBJECT
    // 动画属性：0.0 (OFF) ~ 1.0 (ON)
    Q_PROPERTY(double offset READ offset WRITE setOffset)

public:
    explicit SwitchButton(QWidget *parent = nullptr) : QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);

        // 默认配置
        m_checked = false;
        m_offset = 0.0;
        m_onText = "ON";
        m_offText = "OFF";
        m_onColor = QColor("#2ecc71");
        m_offColor = QColor("#bdc3c7");
        m_sliderColor = Qt::white;
        m_textColor = Qt::white;

        m_anim = new QPropertyAnimation(this, "offset", this);
        m_anim->setDuration(200);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);
    }

    void setTexts(const QString &onText, const QString &offText) {
        m_onText = onText;
        m_offText = offText;
        updateGeometry(); // 文字改变需重新计算尺寸
        update();
    }

    void setColors(const QColor &on, const QColor &off) {
        m_onColor = on;
        m_offColor = off;
        update();
    }

    bool isChecked() const { return m_checked; }

    void setChecked(bool checked) {
        if (m_checked == checked) return;
        m_checked = checked;
        m_offset = checked ? 1.0 : 0.0; // 直接切换，无动画
        emit toggled(checked);
        update();
    }

    QSize sizeHint() const override {
        QFontMetrics fm(font());
        // 宽度基于较长的文字计算：高度(滑块直径) + 文字宽 + 边距
        int textW = qMax(fm.horizontalAdvance(m_onText), fm.horizontalAdvance(m_offText));
        int h = 30;
        int w = h + textW + 20;
        return QSize(w, h);
    }

    double offset() const { return m_offset; }

    void setOffset(double o) {
        m_offset = o;
        update(); // 动画更新数值时触发重绘
    }

signals:
    void toggled(bool checked);

protected:
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            m_checked = !m_checked;
            emit toggled(m_checked);

            // 启动动画至目标状态
            m_anim->stop();
            m_anim->setStartValue(m_offset);
            m_anim->setEndValue(m_checked ? 1.0 : 0.0);
            m_anim->start();
        }
        QWidget::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        int w = width();
        int h = height();
        int margin = 3;
        int sliderSize = h - 2 * margin;

        // --- 1. 绘制背景 ---
        // 根据当前进度选择颜色
        QColor bgColor = (m_offset > 0.5) ? m_onColor : m_offColor;
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, w, h, h/2, h/2);

        // --- 2. 绘制文字 ---
        p.setPen(m_textColor);
        p.setFont(font());

        // offset > 0.5 时滑块在右，文字显示在左；反之亦然
        if (m_offset > 0.5) {
            QRect textRect(0, 0, w - h, h);
            p.drawText(textRect, Qt::AlignCenter, m_onText);
        } else {
            QRect textRect(h, 0, w - h, h);
            p.drawText(textRect, Qt::AlignCenter, m_offText);
        }

        // --- 3. 绘制滑块 ---
        // 线性插值计算滑块 X 坐标
        double startX = margin;
        double endX = w - sliderSize - margin;
        double currentX = startX + (endX - startX) * m_offset;

        p.setBrush(m_sliderColor);
        p.drawEllipse(QRectF(currentX, margin, sliderSize, sliderSize));
    }

private:
    bool m_checked;
    double m_offset;

    QString m_onText;
    QString m_offText;
    QColor m_onColor;
    QColor m_offColor;
    QColor m_sliderColor;
    QColor m_textColor;

    QPropertyAnimation *m_anim;
};