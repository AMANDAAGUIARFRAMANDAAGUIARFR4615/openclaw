#pragma once

#include <QListWidgetItem>
#include <QCollator>

class NaturalSortListWidgetItem : public QListWidgetItem {
public:
    using QListWidgetItem::QListWidgetItem;

    bool operator<(const QListWidgetItem &other) const override {
        static QCollator collator;
        
        if (!collator.numericMode())
            collator.setNumericMode(true);

        // collator.setIgnorePunctuation(true);

        return collator.compare(text(), other.text()) < 0;
    }
};