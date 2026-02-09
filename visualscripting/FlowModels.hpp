#pragma once

#include "Definitions.hpp"
#include "../FileViewer.h"
#include <QtNodes/NodeDelegateModel>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QPixmap>
#include <QPainter>
#include <QUuid>
#include <QDir>
#include <QJsonObject>
#include <QJsonValue>
#include <QCryptographicHash>
#include <QBuffer>

using namespace QtNodes;

/// 点数据：{x, y}
class PointData : public NodeData {
public:
    PointData() {}
    PointData(QString expr) : _expr(expr) {}
    NodeDataType type() const override { return NodeDataType{"point", "点"}; }
    QString expression() const { return _expr; }
private:
    QString _expr;
};

/// 矩形数据：{x, y, w, h}
class RectData : public NodeData {
public:
    RectData() {}
    RectData(QString expr) : _expr(expr) {}
    NodeDataType type() const override { return NodeDataType{"rect", "矩形"}; }
    QString expression() const { return _expr; }
private:
    QString _expr;
};

// 开始节点
class StartFlowModel : public NodeDelegateModel {
    Q_OBJECT
public:
    StartFlowModel() {
        _btn = new QPushButton("生成并运行");
        connect(_btn, &QPushButton::clicked, this, [this](){
            auto buffer = std::make_shared<QString>("");
            _outData = std::make_shared<ExecData>(buffer, 0);

            // 递归传播，这将触发下游节点的 setInData，从而执行保存图片等逻辑
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
        _edit = new QPlainTextEdit("-- 输入Lua代码\n-- 例如: a, b = b, a+b");
        _edit->setFixedHeight(100);
        _edit->setFixedWidth(200);
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

class RectModel : public NodeDelegateModel {
    Q_OBJECT
public:
    RectModel() {
        // 提供默认值输入框
        _widget = new QWidget();
        QGridLayout* l = new QGridLayout(_widget);
        l->setContentsMargins(2,2,2,2);
        
        _editX = new QLineEdit("0"); _editY = new QLineEdit("0");
        _editW = new QLineEdit("100"); _editH = new QLineEdit("100");
        
        l->addWidget(new QLabel("X:"), 0, 0); l->addWidget(_editX, 0, 1);
        l->addWidget(new QLabel("Y:"), 0, 2); l->addWidget(_editY, 0, 3);
        l->addWidget(new QLabel("W:"), 1, 0); l->addWidget(_editW, 1, 1);
        l->addWidget(new QLabel("H:"), 1, 2); l->addWidget(_editH, 1, 3);
        
        _widget->resize(160, 80);
        
        // 数据变更时通知更新
        auto update = [this](){ Q_EMIT dataUpdated(0); };
        connect(_editX, &QLineEdit::textChanged, this, update);
        connect(_editY, &QLineEdit::textChanged, this, update);
        connect(_editW, &QLineEdit::textChanged, this, update);
        connect(_editH, &QLineEdit::textChanged, this, update);
    }

    QString caption() const override { return "创建矩形"; }
    QString name() const override { return "Rect"; }

    // 输入: 0=X, 1=Y, 2=W, 3=H (Decimal)
    // 输出: 0=Rect (RectData)
    unsigned int nPorts(PortType pt) const override { return pt == PortType::In ? 4 : 1; }
    
    NodeDataType dataType(PortType pt, PortIndex pi) const override {
        if (pt == PortType::In) return DecimalData(0).type();
        return RectData().type();
    }

    QString portCaption(PortType pt, PortIndex pi) const override {
        if (pt == PortType::In) {
            static const QString names[] = {"X", "Y", "W", "H"};
            return names[pi];
        }
        return "矩形";
    }
    bool portCaptionVisible(PortType, PortIndex) const override { return true; }

    std::shared_ptr<NodeData> outData(PortIndex) override {
        // 优先使用端口输入，如果没有则使用输入框的值
        auto getVal = [&](int portIdx, QLineEdit* edit) {
            return !_inputs[portIdx].isEmpty() ? _inputs[portIdx] : edit->text();
        };

        QString code = QString("{x=%1, y=%2, w=%3, h=%4}")
            .arg(getVal(0, _editX), getVal(1, _editY), getVal(2, _editW), getVal(3, _editH));
            
        return std::make_shared<RectData>(code);
    }

    void setInData(std::shared_ptr<NodeData> data, PortIndex pi) override {
        auto d = std::dynamic_pointer_cast<DecimalData>(data);
        _inputs[pi] = d ? d->expression() : "";
        Q_EMIT dataUpdated(0);
    }

    QWidget *embeddedWidget() override { return _widget; }

private:
    QWidget* _widget;
    QLineEdit *_editX, *_editY, *_editW, *_editH;
    QString _inputs[4]; // 缓存端口输入
};

class FindImageModel : public NodeDelegateModel {
    Q_OBJECT
public:
    FindImageModel() {
        // 生成一个唯一的内部变量名，用于Lua脚本
        _resultVarName = "pt_" + QUuid::createUuid().toString(QUuid::Id128).left(8);

        _widget = new QWidget();
        QVBoxLayout* l = new QVBoxLayout(_widget);
        l->setContentsMargins(4, 4, 4, 4);
        l->setSpacing(6);

        // 1. 图片显示区
        _lblImage = new QLabel("无图片");
        _lblImage->setAlignment(Qt::AlignCenter);
        _lblImage->setFixedSize(200, 100); 
        _lblImage->setStyleSheet("background-color: #2b2b2b; border: 1px solid #555; color: #888; border-radius: 4px;");
        l->addWidget(_lblImage);

        // 2. 粘贴按钮
        QPushButton* btnPaste = new QPushButton("粘贴剪切板图片");
        btnPaste->setCursor(Qt::PointingHandCursor);
        connect(btnPaste, &QPushButton::clicked, this, &FindImageModel::onPasteImage);
        l->addWidget(btnPaste);

        _widget->resize(220, 150);
    }

    QString caption() const override { return "找子图"; }
    QString name() const override { return "FindImage"; }

    unsigned int nPorts(PortType pt) const override { return 2; }
    
    NodeDataType dataType(PortType pt, PortIndex pi) const override {
        if (pi == 0) return ExecData().type();
        if (pt == PortType::In) return RectData().type();
        return PointData().type();
    }

    QString portCaption(PortType pt, PortIndex pi) const override {
        if (pt == PortType::In) {
            if (pi == 0) return "执行";
            if (pi == 1) return "矩形区域"; 
        } else {
            if (pi == 0) return "完成";
            if (pi == 1) return "坐标点"; 
        }
        return "";
    }
    bool portCaptionVisible(PortType, PortIndex) const override { return true; }

    std::shared_ptr<NodeData> outData(PortIndex pi) override {
        if (pi == 0) return _outData;
        if (pi == 1) return std::make_shared<PointData>(_resultVarName);
        return nullptr;
    }

    void setInData(std::shared_ptr<NodeData> data, PortIndex pi) override {
        if (pi == 0 && data) {
            auto e = std::dynamic_pointer_cast<ExecData>(data);
            if (!e) return;

            // --- 核心修改：在生成代码前检查并保存文件 ---
            if (!_cachedPixmap.isNull() && !_imagePath.isEmpty()) {
                QDir dir(QDir::currentPath());
                if (!dir.exists("images")) dir.mkdir("images");
                
                // 只有当文件不存在时才写入，避免重复IO
                if (!QFile::exists(_imagePath)) {
                    _cachedPixmap.save(_imagePath, "PNG");
                }
            }
            // ------------------------------------------

            QString img = _imagePath.isEmpty() ? "nil" : QString("'%1'").arg(_imagePath);
            QString findArgs;
            if (!_exprRegion.isEmpty()) {
                findArgs = QString("%1, %2.x, %2.y, %2.w, %2.h").arg(img, _exprRegion);
            } else {
                findArgs = QString("%1, nil, nil, nil, nil").arg(img);
            }

            QString code = QString("local tx, ty = findImage(%1)\n"
                                   "local %2 = {x = tx, y = ty}")
                                   .arg(findArgs, _resultVarName);

            e->append(ExecData::formatLine(code, e->indentLevel()));
            _outData = std::make_shared<ExecData>(e->scriptBuffer(), e->indentLevel());
            Q_EMIT dataUpdated(0);
        }
        else if (pi == 1) {
            auto d = std::dynamic_pointer_cast<RectData>(data);
            _exprRegion = d ? d->expression() : ""; 
        }
    }

    QWidget *embeddedWidget() override { return _widget; }

    QJsonObject save() const override {
        QJsonObject o = NodeDelegateModel::save();
        o["path"] = _imagePath;
        return o;
    }

    void load(QJsonObject const& p) override {
        if (p.contains("path")) {
            _imagePath = p["path"].toString();
            // 加载时如果有文件，读取到缓存中
            if (!_imagePath.isEmpty() && QFile::exists(_imagePath)) {
                _cachedPixmap.load(_imagePath);
                updateImagePreview();
            }
        }
    }

private:
    void onPasteImage() {
        QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *mimeData = clipboard->mimeData();

        if (mimeData->hasImage()) {
            QPixmap pixmap = qvariant_cast<QPixmap>(mimeData->imageData());
            if (!pixmap.isNull()) {
                // 1. 缓存到内存
                _cachedPixmap = pixmap;

                // 2. 预计算文件名 (MD5)
                QByteArray bytes;
                QBuffer buffer(&bytes);
                buffer.open(QIODevice::WriteOnly);
                pixmap.save(&buffer, "PNG");
                
                QCryptographicHash hash(QCryptographicHash::Md5);
                hash.addData(bytes);
                QString md5 = hash.result().toHex();
                
                _imagePath = "images/img_" + md5 + ".png";
                
                // 3. 更新预览 (不保存文件)
                updateImagePreview();
                
                // 4. 通知有数据变更
                Q_EMIT dataUpdated(0);
            }
        }
    }

    void updateImagePreview() {
        if (_cachedPixmap.isNull()) {
            _lblImage->setText("无图片");
            return;
        }

        int w = _lblImage->width();
        int h = _lblImage->height();
        _lblImage->setPixmap(_cachedPixmap.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    QWidget* _widget;
    QLabel *_lblImage;
    QString _resultVarName;
    QString _imagePath;
    QString _exprRegion;
    QPixmap _cachedPixmap; // 内存缓存，粘贴时更新，setInData时写入
    std::shared_ptr<ExecData> _outData;
};