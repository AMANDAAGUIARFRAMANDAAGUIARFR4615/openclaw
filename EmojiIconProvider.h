#pragma once

#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QFont>

class EmojiIconProvider {
public:
    static QPixmap createPixmap(const QString &emoji, int size = 64) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        QFont font;
        font.setPixelSize(size * 0.8);
        painter.setFont(font);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, emoji);
        painter.end();

        return pixmap;
    }

    static QIcon createIcon(const QString &emoji, int size = 64) {
        return QIcon(createPixmap(emoji, size));
    }
};
