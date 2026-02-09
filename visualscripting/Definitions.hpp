#pragma once

#include <QtNodes/NodeData>
#include <QString>
#include <memory> // 需要包含 memory

using QtNodes::NodeData;
using QtNodes::NodeDataType;

/// 执行流数据：携带共享的脚本缓冲区和缩进深度
class ExecData : public NodeData
{
public:
    // 默认构造
    ExecData() : _scriptBuffer(std::make_shared<QString>()), _indentLevel(0) {}

    // 构造函数：接收共享缓冲区
    ExecData(std::shared_ptr<QString> buffer, int level = 0)
        : _scriptBuffer(buffer), _indentLevel(level) {}

    NodeDataType type() const override { return NodeDataType{"exec", "执行"}; }

    // 获取当前完整的脚本内容
    QString script() const { return _scriptBuffer ? *_scriptBuffer : ""; }

    // 获取缓冲区指针（传递给下一个节点）
    std::shared_ptr<QString> scriptBuffer() const { return _scriptBuffer; }

    int indentLevel() const { return _indentLevel; }

    // 向共享缓冲区追加代码
    void append(const QString &text) {
        if (_scriptBuffer) {
            *_scriptBuffer += text;
        }
    }

    // 格式化辅助函数
    static QString formatLine(QString text, int level) {
        return QString("    ").repeated(level) + text + "\n";
    }

private:
    std::shared_ptr<QString> _scriptBuffer; // 核心修改：使用指针共享同一份字符串
    int _indentLevel;
};

/// 数值数据：保持不变
class DecimalData : public NodeData
{
public:
    DecimalData(double number) : _expression(QString::number(number)) {}
    DecimalData(QString expr) : _expression(expr) {}

    NodeDataType type() const override { return NodeDataType{"decimal", "数值"}; }
    QString expression() const { return _expression; }

private:
    QString _expression;
};
