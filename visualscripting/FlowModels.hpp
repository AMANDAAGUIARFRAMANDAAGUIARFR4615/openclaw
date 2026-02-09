#pragma once

#include "Definitions.hpp"
#include "../FileViewer.h"
#include <QtNodes/NodeDelegateModel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

using namespace QtNodes;

// 开始节点
class StartFlowModel : public NodeDelegateModel {
    Q_OBJECT
public:
    StartFlowModel() {
        _btn = new QPushButton("生成并运行");
        connect(_btn, &QPushButton::clicked, this, [this](){
            auto buffer = std::make_shared<QString>("");
            _outData = std::make_shared<ExecData>(buffer, 0);

            // 递归传播
            Q_EMIT dataUpdated(0);

            QDialog dialog(QApplication::activeWindow());
            dialog.setWindowTitle("生成的lua代码");
            dialog.resize(800, 600);
            QVBoxLayout* layout = new QVBoxLayout(&dialog);
            auto editor = new CodeEditor();
            editor->setPlainText(*buffer);
            editor->document()->setModified(false);
            layout->addWidget(editor);
            dialog.exec();
        });
    }
    QString caption() const override { return "开始"; }
    QString name() const override { return "StartFlow"; }
    unsigned int nPorts(PortType pt) const override { return pt == PortType::Out ? 1 : 0; }
    NodeDataType dataType(PortType, PortIndex) const override { return ExecData().type(); }
    std::shared_ptr<NodeData> outData(PortIndex) override { return _outData; }
    void setInData(std::shared_ptr<NodeData>, PortIndex) override {}
    QWidget *embeddedWidget() override { return _btn; }
private:
    QPushButton* _btn;
    std::shared_ptr<ExecData> _outData;
};

// 通用 Lua 代码块节点
class LuaStatementModel : public NodeDelegateModel {
    Q_OBJECT
public:
    LuaStatementModel() {
        // 使用 PlainTextEdit 允许输入多行代码
        _edit = new QPlainTextEdit("-- 输入Lua代码\n-- 例如: a, b = b, a+b");
        _edit->setFixedHeight(100);
        _edit->setFixedWidth(200);
        // 样式微调，看起来像代码编辑器
        QFont font("Consolas", 10);
        _edit->setFont(font);
    }

    QString caption() const override { return "自定义代码块"; }
    QString name() const override { return "LuaStatement"; }

    // 只有执行流的进出
    unsigned int nPorts(PortType pt) const override { return 1; }
    NodeDataType dataType(PortType, PortIndex) const override { return ExecData().type(); }

    std::shared_ptr<NodeData> outData(PortIndex) override { return _outData; }

    void setInData(std::shared_ptr<NodeData> data, PortIndex) override {
        auto e = std::dynamic_pointer_cast<ExecData>(data);
        if (!e) return;

        // 获取用户输入的代码
        QString code = _edit->toPlainText();

        // 按行分割，每一行都加上当前的缩进
        QStringList lines = code.split('\n');
        for (const QString& line : lines) {
            e->append(ExecData::formatLine(line, e->indentLevel()));
        }

        // 传递给下游
        _outData = std::make_shared<ExecData>(e->scriptBuffer(), e->indentLevel());
        Q_EMIT dataUpdated(0);
    }

    QWidget *embeddedWidget() override { return _edit; }

    QJsonObject save() const override {
        QJsonObject modelJson = NodeDelegateModel::save();
        modelJson["code"] = _edit->toPlainText();
        return modelJson;
    }
    void load(QJsonObject const& p) override {
        QJsonValue v = p["code"];
        if (!v.isUndefined()) _edit->setPlainText(v.toString());
    }

private:
    QPlainTextEdit* _edit;
    std::shared_ptr<ExecData> _outData;
};

// 赋值节点
class AssignmentModel : public NodeDelegateModel {
    Q_OBJECT
public:
    AssignmentModel() { _edit = new QLineEdit("var_name"); }
    QString caption() const override { return "变量赋值"; }
    QString name() const override { return "Assignment"; }
    unsigned int nPorts(PortType pt) const override { return pt == PortType::In ? 2 : 1; }
    NodeDataType dataType(PortType pt, PortIndex pi) const override {
        return (pt == PortType::In && pi == 1) ? DecimalData(0).type() : ExecData().type();
    }
    std::shared_ptr<NodeData> outData(PortIndex) override { return _outData; }
    void setInData(std::shared_ptr<NodeData> data, PortIndex pi) override {
        if (pi == 0 && data) {
            auto e = std::dynamic_pointer_cast<ExecData>(data);
            if (!e) return;
            e->append(ExecData::formatLine(QString("%1 = %2").arg(_edit->text(), _valExpr), e->indentLevel()));
            _outData = std::make_shared<ExecData>(e->scriptBuffer(), e->indentLevel());
            Q_EMIT dataUpdated(0);
        } else if (pi == 1) {
            auto d = std::dynamic_pointer_cast<DecimalData>(data);
            _valExpr = d ? d->expression() : "0";
        }
    }
    QWidget *embeddedWidget() override { return _edit; }
    QJsonObject save() const override {
        QJsonObject o = NodeDelegateModel::save(); o["var"] = _edit->text(); return o;
    }
    void load(QJsonObject const& p) override {
        QJsonValue v = p["var"]; if (!v.isUndefined()) _edit->setText(v.toString());
    }
private:
    QLineEdit* _edit;
    QString _valExpr = "0";
    std::shared_ptr<ExecData> _outData;
};

// For 循环节点
class LoopModel : public NodeDelegateModel {
    Q_OBJECT
public:
    QString caption() const override { return "For循环"; }
    QString name() const override { return "ForLoop"; }
    unsigned int nPorts(PortType pt) const override { return pt == PortType::In ? 2 : 2; }
    NodeDataType dataType(PortType pt, PortIndex pi) const override {
        return (pt == PortType::In && pi == 1) ? DecimalData(0).type() : ExecData().type();
    }
    QString portCaption(PortType pt, PortIndex pi) const override {
        if (pt == PortType::In) return pi == 0 ? "执行" : "次数";
        return pi == 0 ? "循环体" : "完成";
    }
    bool portCaptionVisible(PortType, PortIndex) const override { return true; }

    std::shared_ptr<NodeData> outData(PortIndex pi) override {
        return pi == 0 ? _bodyData : _nextData;
    }

    void setInData(std::shared_ptr<NodeData> data, PortIndex pi) override {
        if (pi == 0 && data) {
            auto e = std::dynamic_pointer_cast<ExecData>(data);
            if (!e) return;
            e->append(ExecData::formatLine(QString("for i = 1, %1 do").arg(_maxExpr), e->indentLevel()));

            // 循环体
            _bodyData = std::make_shared<ExecData>(e->scriptBuffer(), e->indentLevel() + 1);
            Q_EMIT dataUpdated(0);

            e->append(ExecData::formatLine("end", e->indentLevel()));

            // 循环后
            _nextData = std::make_shared<ExecData>(e->scriptBuffer(), e->indentLevel());
            Q_EMIT dataUpdated(1);
        } else if (pi == 1) {
            auto d = std::dynamic_pointer_cast<DecimalData>(data);
            _maxExpr = d ? d->expression() : "10";
        }
    }
    QWidget *embeddedWidget() override { return nullptr; }
private:
    QString _maxExpr = "10";
    std::shared_ptr<ExecData> _bodyData;
    std::shared_ptr<ExecData> _nextData;
};

// 打印节点
class PrintModel : public NodeDelegateModel {
    Q_OBJECT
public:
    QString caption() const override { return "打印"; }
    QString name() const override { return "Print"; }
    unsigned int nPorts(PortType pt) const override { return pt == PortType::In ? 2 : 1; }
    NodeDataType dataType(PortType pt, PortIndex pi) const override {
        return (pt == PortType::In && pi == 1) ? DecimalData(0).type() : ExecData().type();
    }

    std::shared_ptr<NodeData> outData(PortIndex) override { return _outData; }

    void setInData(std::shared_ptr<NodeData> data, PortIndex pi) override {
        if (pi == 0 && data) {
            auto e = std::dynamic_pointer_cast<ExecData>(data);
            if (!e) return;

            e->append(ExecData::formatLine(QString("print(%1)").arg(_valExpr), e->indentLevel()));

            _outData = std::make_shared<ExecData>(e->scriptBuffer(), e->indentLevel());
            Q_EMIT dataUpdated(0);
        } else if (pi == 1) {
            auto d = std::dynamic_pointer_cast<DecimalData>(data);
            _valExpr = d ? d->expression() : "nil";
        }
    }
    QWidget *embeddedWidget() override { return nullptr; }
private:
    QString _valExpr;
    std::shared_ptr<ExecData> _outData;
};
