#pragma once

#include <QEvent>
#include <functional>

class LambdaEventFilter : public QObject {
public:
    using FilterFunc = std::function<bool(QObject*, QEvent*)>;

    LambdaEventFilter(QObject* parent, FilterFunc func) 
        : QObject(parent), m_filter(func) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        return m_filter(watched, event);
    }

private:
    FilterFunc m_filter;
};
