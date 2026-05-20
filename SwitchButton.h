#pragma once

#include <QWidget>
#include <QPropertyAnimation>
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>

class SwitchButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(float offset READ offset WRITE setOffset)

public:
    explicit SwitchButton(const QString &onText, const QString &offText = QString(), QWidget *parent = nullptr) : m_onText(onText), m_offText(offText), QWidget(parent)
    {
        setCursor(Qt::PointingHandCursor);

        m_offText = offText.isEmpty() ? onText : offText;

        m_checked = false;
        m_offset = 0.0;
        
        m_anim = new QPropertyAnimation(this, "offset", this);
        m_anim->setDuration(200);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);
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
        m_offset = checked ? 1.0 : 0.0;
        emit toggled(checked);
        update();
    }

    QSize sizeHint() const override {
        QFontMetrics fm(font());
        int textW = qMax(fm.horizontalAdvance(m_onText), fm.horizontalAdvance(m_offText));
        int h = 32;
        int w = h + textW + 24;
        return QSize(w, h);
    }

    float offset() const { return m_offset; }

    void setOffset(float o) {
        m_offset = o;
        update();
    }

signals:
    void toggled(bool checked);

protected:
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) {
            m_checked = !m_checked;
            emit toggled(m_checked);

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

        QPalette palette = this->palette();

        QColor currentOnColor = m_onColor.isValid() ? m_onColor : palette.color(QPalette::Highlight);
        QColor currentOffColor = m_offColor.isValid() ? m_offColor : palette.color(QPalette::Mid);
        
        QColor bgColor = m_offset > 0.5 ? currentOnColor : currentOffColor;
        p.setBrush(bgColor);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(0, 0, w, h, h/2, h/2);

        p.setPen(m_offset > 0.5 ? QColor(Qt::white) : QColor("#64748B"));
        QFont labelFont = font();
        labelFont.setPixelSize(13);
        labelFont.setWeight(m_offset > 0.5 ? QFont::DemiBold : QFont::Normal);
        p.setFont(labelFont);

        // 核心修改逻辑：精确计算“空白区域”的矩形
        // 目标：将文字绘制在 [滑块边缘] 和 [背景边缘] 之间的正中心

        if (m_offset > 0.5) {
            // ON 状态：滑块在右，文字在左
            // 空白区域：0 (背景左缘) -> (w - h + margin) (滑块左缘)
            // 注：w - (h - margin) 是滑块左边缘的精确 X 坐标
            int sliderLeftEdge = w - h + margin;
            QRect textRect(0, 0, sliderLeftEdge, h);
            p.drawText(textRect, Qt::AlignCenter, m_onText);
        } else {
            // OFF 状态：滑块在左，文字在右
            // 空白区域：(h - margin) (滑块右缘) -> w (背景右缘)
            // 注：margin + sliderSize = h - margin 是滑块右边缘的精确 X 坐标
            int sliderRightEdge = h - margin;
            QRect textRect(sliderRightEdge, 0, w - sliderRightEdge, h);
            p.drawText(textRect, Qt::AlignCenter, m_offText);
        }

        float startX = margin;
        float endX = w - sliderSize - margin;
        float currentX = startX + (endX - startX) * m_offset;

        p.setBrush(Qt::white);
        p.drawEllipse(QRectF(currentX, margin, sliderSize, sliderSize));
    }

private:
    bool m_checked;
    float m_offset;

    QString m_onText;
    QString m_offText;
    QColor m_onColor;
    QColor m_offColor;

    QPropertyAnimation *m_anim;
};