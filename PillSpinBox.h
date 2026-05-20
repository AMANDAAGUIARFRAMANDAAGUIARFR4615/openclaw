#pragma once

#include <QFrame>
#include <QSpinBox>
#include <QHBoxLayout>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QMouseEvent>
#include <QFontMetrics>
#include <QPolygon>
#include <functional>

class SpinStepper : public QWidget
{
public:
    explicit SpinStepper(QWidget *parent = nullptr) : QWidget(parent)
    {
        setFixedSize(12, 22);
        setCursor(Qt::PointingHandCursor);
        setMouseTracking(true);
    }

    void setStepHandlers(std::function<void()> onUp, std::function<void()> onDown)
    {
        m_onUp = std::move(onUp);
        m_onDown = std::move(onDown);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QColor normal(0xBC, 0xC5, 0xD1);
        const QColor hover(0x94, 0xA3, 0xB8);
        const QColor active(0x4A, 0x86, 0xF7);
        const bool upHover = m_hoverUp && underMouse();
        const bool downHover = m_hoverDown && underMouse();

        auto drawTriangle = [&](int cy, bool up, bool hovered, bool pressed) {
            QColor c = pressed ? active : (hovered ? hover : normal);
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            const int cx = width() / 2;
            if (up) {
                QPolygon tri{{cx, cy - 3}, {cx - 4, cy + 2}, {cx + 4, cy + 2}};
                p.drawPolygon(tri);
            } else {
                QPolygon tri{{cx, cy + 3}, {cx - 4, cy - 2}, {cx + 4, cy - 2}};
                p.drawPolygon(tri);
            }
        };

        drawTriangle(5, true, upHover, m_pressedUp);
        drawTriangle(height() - 5, false, downHover, m_pressedDown);
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        const bool up = e->position().y() < height() / 2.0;
        if (m_hoverUp != up) {
            m_hoverUp = up;
            m_hoverDown = !up;
            update();
        }
        QWidget::mouseMoveEvent(e);
    }

    void leaveEvent(QEvent *e) override
    {
        m_hoverUp = m_hoverDown = m_pressedUp = m_pressedDown = false;
        update();
        QWidget::leaveEvent(e);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            if (e->position().y() < height() / 2.0) {
                m_pressedUp = true;
                if (m_onUp)
                    m_onUp();
            } else {
                m_pressedDown = true;
                if (m_onDown)
                    m_onDown();
            }
            update();
        }
        QWidget::mousePressEvent(e);
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        m_pressedUp = m_pressedDown = false;
        update();
        QWidget::mouseReleaseEvent(e);
    }

private:
    bool m_hoverUp = false;
    bool m_hoverDown = false;
    bool m_pressedUp = false;
    bool m_pressedDown = false;
    std::function<void()> m_onUp;
    std::function<void()> m_onDown;
};

class PillSpinBox : public QFrame
{
    Q_OBJECT

public:
    explicit PillSpinBox(QWidget *parent = nullptr) : QFrame(parent)
    {
        setObjectName("pillSpinBox");
        setAttribute(Qt::WA_StyledBackground, true);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        setFixedHeight(34);

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 10, 0);
        layout->setSpacing(4);

        m_spin = new QSpinBox(this);
        m_spin->setObjectName("pillSpinBoxInner");
        m_spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        m_spin->setFrame(false);
        m_spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_spin->installEventFilter(this);

        m_stepper = new SpinStepper(this);
        m_stepper->setStepHandlers(
            [this] { m_spin->stepUp(); },
            [this] { m_spin->stepDown(); });
        connect(m_spin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
            updateWidth();
            emit valueChanged(v);
        });

        layout->addWidget(m_spin, 1);
        layout->addWidget(m_stepper, 0, Qt::AlignVCenter);

        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(18);
        shadow->setOffset(0, 2);
        shadow->setColor(QColor(15, 23, 42, 22));
        setGraphicsEffect(shadow);

        updateWidth();
    }

    int value() const { return m_spin->value(); }
    void setValue(int v) { m_spin->setValue(v); updateWidth(); }
    void setRange(int min, int max) { m_spin->setRange(min, max); updateWidth(); }
    void setMinimum(int min) { m_spin->setMinimum(min); }
    void setMaximum(int max) { m_spin->setMaximum(max); }
    void setSuffix(const QString &suffix) { m_spin->setSuffix(suffix); updateWidth(); }

    void setFont(const QFont &font)
    {
        QFrame::setFont(font);
        m_spin->setFont(font);
        updateWidth();
    }

    QSize sizeHint() const override
    {
        return {width(), 34};
    }

signals:
    void valueChanged(int value);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_spin) {
            if (event->type() == QEvent::FocusIn) {
                setProperty("focused", true);
                style()->unpolish(this);
                style()->polish(this);
                update();
            } else if (event->type() == QEvent::FocusOut) {
                setProperty("focused", false);
                style()->unpolish(this);
                style()->polish(this);
                update();
            }
        }
        return QFrame::eventFilter(watched, event);
    }

private:
    void updateWidth()
    {
        QFontMetrics fm(m_spin->font());
        const int textW = fm.horizontalAdvance(QString::number(m_spin->maximum()) + m_spin->suffix());
        setFixedWidth(qMax(84, textW + 16 + 10 + 22));
    }

    QSpinBox *m_spin = nullptr;
    SpinStepper *m_stepper = nullptr;
};
