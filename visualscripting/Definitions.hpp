#pragma once

#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModel>
#include <QString>
#include <memory>

using namespace QtNodes;

// --- 基础数据类型定义 ---

/// 执行流数据：携带共享的脚本缓冲区和缩进深度
class ExecData : public NodeData
{
public:
    ExecData() : _scriptBuffer(std::make_shared<QString>()), _indentLevel(0) {}

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
        if (_scriptBuffer) *_scriptBuffer += text;
    }

    // 格式化辅助函数
    static QString formatLine(QString text, int level) {
        return QString("    ").repeated(level) + text + "\n";
    }

private:
    std::shared_ptr<QString> _scriptBuffer;
    int _indentLevel;
};

/// 数值数据
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


// --- 自定义模型基类 ---

/// 所有流节点的基类，统一管理连接策略
class FlowDelegateModel : public NodeDelegateModel
{
public:
    ConnectionPolicy portConnectionPolicy(PortType portType, PortIndex portIndex) const override {
        // 获取该端口的数据类型
        auto type = dataType(portType, portIndex);

        if (type.id == "exec") {
            // --- 执行流端口策略 ---
            if (portType == PortType::In) {
                // 输入 Exec: 允许多个上游汇入 (Merge)
                return ConnectionPolicy::Many;
            } else {
                // 输出 Exec: 强制单线执行
                return ConnectionPolicy::One;
            }
        } else {
            // --- 数据端口策略 (decimal, point, rect 等) ---
            if (portType == PortType::In) {
                // 输入 Data: 只能有一个数据源
                return ConnectionPolicy::One;
            } else {
                // 输出 Data: 可以被多次引用
                return ConnectionPolicy::Many;
            }
        }
    }
};