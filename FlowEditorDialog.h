#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QFile>
#include <QMessageBox>
#include <memory>

#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>

#include "PureDataModels.hpp"
#include "FlowModels.hpp"

using namespace QtNodes;

class FlowEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FlowEditorDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("可视化编程");
        resize(1200, 800);
        setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint);

        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        QMenuBar *menuBar = new QMenuBar(this);
        QMenu *fileMenu = menuBar->addMenu("文件");

        QAction *saveAction = fileMenu->addAction("保存");
        connect(saveAction, &QAction::triggered, this, &FlowEditorDialog::saveScene);

        QAction *loadAction = fileMenu->addAction("载入");
        connect(loadAction, &QAction::triggered, this, &FlowEditorDialog::loadScene);

        layout->setMenuBar(menuBar);

        auto registry = registerModels();
        m_model = std::make_shared<DataFlowGraphModel>(registry);
        m_scene = new DataFlowGraphicsScene(*m_model, this);
        m_view = new GraphicsView(m_scene, this);

        layout->addWidget(m_view);
    }

    ~FlowEditorDialog() override = default;

private slots:
    void saveScene() {
        QString fileName = QFileDialog::getSaveFileName(this, "保存场景", "", "Flow JSON (*.json)");
        if (fileName.isEmpty()) return;
        if (!fileName.endsWith(".json", Qt::CaseInsensitive)) fileName += ".json";

        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonObject jsonObj = m_model->save();
            QJsonDocument doc(jsonObj);
            file.write(doc.toJson());
        } else {
            QMessageBox::warning(this, "错误", "无法保存文件");
        }
    }

    void loadScene() {
        QString fileName = QFileDialog::getOpenFileName(this, "载入场景", "", "Flow JSON (*.json)");
        if (fileName.isEmpty()) return;

        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            m_model->load(doc.object());
        } else {
            QMessageBox::warning(this, "错误", "无法打开文件");
        }
    }

private:
    std::shared_ptr<NodeDelegateModelRegistry> registerModels() {
        auto ret = std::make_shared<NodeDelegateModelRegistry>();
        ret->registerModel<NumberSourceModel>("数据");
        ret->registerModel<VariableGetModel>("数据");
        ret->registerModel<AdditionModel>("数据");
        ret->registerModel<StartFlowModel>("控制流");
        ret->registerModel<AssignmentModel>("控制流");
        ret->registerModel<LoopModel>("控制流");
        ret->registerModel<PrintModel>("控制流");
        ret->registerModel<LuaStatementModel>("控制流");
        return ret;
    }

    std::shared_ptr<DataFlowGraphModel> m_model;
    DataFlowGraphicsScene *m_scene;
    GraphicsView *m_view;
};
