#pragma once

#include <QListWidgetItem>
#include <QCollator>

class NaturalSortListWidgetItem : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;

    bool operator<(const QListWidgetItem &other) const override {
        if (!listWidget()->isEnabled())
            return false;

        if (settings->value("sortSelectedToTop").toBool()) {
            bool mySelected = isSelected();
            bool otherSelected = other.isSelected();

            if (mySelected != otherSelected) {
                // 在 AscendingOrder (升序) 下，返回 true 表示 "我排在它前面"
                // 如果我是选中的 (true)，它是未选中的 (false)，我应该排前面 -> return true
                return mySelected; 
            }
        }

        static QCollator collator;
        
        if (!collator.numericMode())
            collator.setNumericMode(true);

        // collator.setIgnorePunctuation(true);

        return collator.compare(text(), other.text()) < 0;
    }
};
