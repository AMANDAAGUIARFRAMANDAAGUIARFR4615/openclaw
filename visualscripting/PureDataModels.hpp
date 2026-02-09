#pragma once

#include "Definitions.hpp"
#include <QtNodes/NodeDelegateModel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QJsonObject>
#include <QJsonValue>

using namespace QtNodes;

// 1. 数值常量节点：仅提供数字
class NumberSourceModel : public NodeDelegateModel
{
    Q_OBJECT
public:
    NumberSourceModel() {
        _edit = new QLineEdit("0");
        // 当文本改变时通知图形更新，防止未保存状态丢失
        connect(_edit, &QLineEdit::textChanged, this, [this](){
            Q_EMIT dataUpdated(0);
        });
    }

    QString caption() const override { return "数值常量"; }
    QString name() const override { return "NumberSource"; }
    unsigned int nPorts(PortType pt) const override { return pt == PortType::Out ? 1 : 0; }
    NodeDataType dataType(PortType, PortIndex) const override { return DecimalData(0).type(); }

    std::shared_ptr<NodeData> outData(PortIndex) override {
        return std::make_shared<DecimalData>(_edit->text().toDouble());
    }
    void setInData(std::shared_ptr<NodeData>, PortIndex) override {}
    QWidget *embeddedWidget() override { return _edit; }

    QJsonObject save() const override {
        QJsonObject modelJson = NodeDelegateModel::save();
        modelJson["number"] = _edit->text();
        return modelJson;
    }

    void load(QJsonObject const& p) override {
        QJsonValue v = p["number"];
        if (!v.isUndefined()) {
            _edit->setText(v.toString());
        }
    }

private:
    QLineEdit *_edit;
};

// 2. 变量读取节点：输入变量名，输出该变量名供下游使用
class VariableGetModel : public NodeDelegateModel
{
    Q_OBJECT
public:
    VariableGetModel() {
        _edit = new QLineEdit("a");
        connect(_edit, &QLineEdit::textChanged, this, [this](){ Q_EMIT dataUpdated(0); });
    }
    QString caption() const override { return "变量读取"; }
    QString name() const override { return "VariableGet"; }
    unsigned int nPorts(PortType pt) const override { return pt == PortType::Out ? 1 : 0; }
    NodeDataType dataType(PortType, PortIndex) const override { return DecimalData(0).type(); }

    std::shared_ptr<NodeData> outData(PortIndex) override {
        return std::make_shared<DecimalData>(_edit->text().trimmed());
    }
    void setInData(std::shared_ptr<NodeData>, PortIndex) override {}
    QWidget *embeddedWidget() override { return _edit; }

    QJsonObject save() const override {
        QJsonObject modelJson = NodeDelegateModel::save();
        modelJson["var_name"] = _edit->text();
        return modelJson;
    }

    void load(QJsonObject const& p) override {
        QJsonValue v = p["var_name"];
        if (!v.isUndefined()) {
            _edit->setText(v.toString());
        }
    }

private:
    QLineEdit *_edit;
};

// 3. 加法节点 (A + B)
// 加法节点没有内部控件状态，不需要重写 save/load
class AdditionModel : public NodeDelegateModel
{
    Q_OBJECT
public:
    QString caption() const override { return "加法 (A + B)"; }
    QString name() const override { return "Addition"; }
    unsigned int nPorts(PortType pt) const override { return pt == PortType::In ? 2 : 1; }
    NodeDataType dataType(PortType, PortIndex) const override { return DecimalData(0).type(); }

    std::shared_ptr<NodeData> outData(PortIndex) override {
        return std::make_shared<DecimalData>(QString("(%1 + %2)").arg(_a, _b));
    }

    void setInData(std::shared_ptr<NodeData> data, PortIndex pi) override {
        auto d = std::dynamic_pointer_cast<DecimalData>(data);
        QString expr = d ? d->expression() : "0";
        if (pi == 0) _a = expr; else _b = expr;
        Q_EMIT dataUpdated(0);
    }
    QWidget *embeddedWidget() override { return nullptr; }
private:
    QString _a = "0", _b = "0";
};
