#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QSettings>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonObject>
#include <QJsonArray>

class SettingsViewer : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsViewer(QSettings* settings, QWidget* parent = nullptr)
        : QDialog(parent), m_settings(settings), m_tree(nullptr)
    {
        setWindowTitle("QSettings 数据编辑器");
        setMinimumSize(600, 400);
        
        setupUI();
        populateTree();
        m_tree->expandAll();

        setModal(true);
        exec();
    }

private:
    QSettings* m_settings;
    QTreeWidget* m_tree;

    void setupUI()
    {
        m_tree = new QTreeWidget(this);
        QStringList headers = {"键/组", "值", "类型"};
        m_tree->setHeaderLabels(headers);
        m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

        m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &SettingsViewer::showContextMenu);

        auto layout = new QVBoxLayout(this);
        layout->addWidget(m_tree);
        setLayout(layout);
    }

    void populateTree(QTreeWidgetItem* parent = nullptr, const QString& prefix = "")
    {
        m_settings->beginGroup(prefix);

        for (const QString& group : m_settings->childGroups())
        {
            auto item = new QTreeWidgetItem({group});
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            item->setData(0, Qt::UserRole, "group");
            item->setData(0, Qt::UserRole + 1, prefix.isEmpty() ? group : prefix + "/" + group);

            if (parent) parent->addChild(item);
            else m_tree->addTopLevelItem(item);

            populateTree(item, prefix.isEmpty() ? group : prefix + "/" + group);
        }

        for (const QString& key : m_settings->childKeys())
        {
            auto item = new QTreeWidgetItem({key});
            item->setData(0, Qt::UserRole, "key");
            item->setData(0, Qt::UserRole + 1, prefix.isEmpty() ? key : prefix + "/" + key);

            if (parent) parent->addChild(item);
            else m_tree->addTopLevelItem(item);

            addVariantToTree(item, QString(), m_settings->value(key));
        }

        m_settings->endGroup();
    }

    void addVariantToTree(QTreeWidgetItem* parent, const QString& name, const QVariant& var)
    {
        auto item = parent;
        if (!name.isEmpty()) {
            item = new QTreeWidgetItem({name});
            parent->addChild(item);
        }

        item->setText(1, var.toString());
        item->setText(2, var.typeName());
        if (var.isNull())
            item->setForeground(1, Qt::gray);

        if (var.typeId() == QMetaType::QString || var.typeId() == QMetaType::QByteArray)
            return;

        if (var.typeId() == QMetaType::QJsonObject) {
            QJsonObject jsonObject = var.value<QJsonObject>();

            for (auto it = jsonObject.constBegin(); it != jsonObject.constEnd(); ++it) {
                addVariantToTree(item, it.key(), (QJsonValue)it.value());
            }
            return;
        }

        if (var.typeId() == QMetaType::QJsonArray) {
            QJsonArray jsonArray = var.value<QJsonArray>();

            for (int i = 0; i < jsonArray.size(); ++i) {
                addVariantToTree(item, QString("[%1]").arg(i), jsonArray.at(i));
            }
            return;
        }

        if (var.canConvert(QMetaType::QVariantList))
        {
            const QVariantList list = var.toList();
            for (int i = 0; i < list.size(); ++i) {
                addVariantToTree(item, QString("[%1]").arg(i), list[i]);
            }
            return;
        }

        if (var.canConvert(QMetaType::QVariantMap))
        {
            const QVariantMap map = var.toMap();
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                addVariantToTree(item, it.key(), it.value());
            }
            return;
        }
    }

private slots:
    void showContextMenu(const QPoint& pos)
    {
        QTreeWidgetItem* item = m_tree->itemAt(pos);
        if (!item) return;

        QMenu menu;

        QString type = item->data(0, Qt::UserRole).toString();
        if (type == "key" || type == "group")
            menu.addAction("删除", this, [this, item]() { deleteItem(item); });

        if (!menu.actions().isEmpty())
            menu.exec(m_tree->mapToGlobal(pos));
    }

    void deleteItem(QTreeWidgetItem* item)
    {
        if (QMessageBox::question(this, "确认", "确定删除？") != QMessageBox::Yes)
            return;

        QString fullPath = item->data(0, Qt::UserRole + 1).toString();
        QString type = item->data(0, Qt::UserRole).toString();

        if (type == "group")
        {
            m_settings->remove(fullPath);
        }
        else if (type == "key")
        {
            int slash = fullPath.lastIndexOf('/');
            QString group = slash == -1 ? "" : fullPath.left(slash);
            QString key = item->text(0);

            m_settings->beginGroup(group);
            m_settings->remove(key);
            m_settings->endGroup();
        }

        delete item;
    }
};
