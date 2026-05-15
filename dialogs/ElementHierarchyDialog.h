#pragma once

#include "BaseDialog.h"
#include "DeviceConnection.h"
#include "DeviceInfo.h"
#include "ToastWidget.h"
#include "global.h"
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

/** 通过设备 HTTP（参见 docs：设备 ip:8080 转发脚本服务）拉取 /element/source 并以树形展示。 */
class ElementHierarchyDialog : public BaseDialog {
public:
    static void open(DeviceConnection *connection, DeviceInfo *deviceInfo, QWidget *parent) {
        Q_UNUSED(connection);
        if (!deviceInfo) {
            return;
        }
        if (deviceInfo->localIp.isEmpty()) {
            new ToastWidget(
                QStringLiteral("当前连接没有可用的局域网 IP，无法访问设备上的 HTTP 接口（参见 docs/README.md：使用 设备IP:8080）。"
                               "请改用同一局域网的 Wi-Fi 连接后再试。"),
                parent);
            return;
        }
        ElementHierarchyDialog dialog(deviceInfo, parent);
        dialog.resize(780, 580);
        dialog.exec();
    }

private:
    explicit ElementHierarchyDialog(DeviceInfo *deviceInfo, QWidget *parent)
        : BaseDialog(QStringLiteral("界面层级树"), parent), deviceInfo_(deviceInfo) {
        auto *root = contentLayout();
        root->setContentsMargins(12, 12, 12, 12);
        root->setSpacing(8);

        auto *top = new QHBoxLayout();
        statusLabel_ = new QLabel(QStringLiteral("就绪"), this);
        statusLabel_->setWordWrap(true);
        top->addWidget(statusLabel_, 1);

        refreshBtn_ = new QPushButton(QStringLiteral("刷新"), this);
        expandBtn_ = new QPushButton(QStringLiteral("全部展开"), this);
        collapseBtn_ = new QPushButton(QStringLiteral("全部折叠"), this);
        top->addWidget(refreshBtn_);
        top->addWidget(expandBtn_);
        top->addWidget(collapseBtn_);
        root->addLayout(top);

        filterEdit_ = new QLineEdit(this);
        filterEdit_->setPlaceholderText(QStringLiteral("筛选：类型 / 文本 / 标识（子树匹配）"));
        root->addWidget(filterEdit_);

        tree_ = new QTreeWidget(this);
        tree_->setHeaderLabels({QStringLiteral("类型"), QStringLiteral("文本"), QStringLiteral("位置"),
                               QStringLiteral("标识")});
        tree_->setColumnCount(4);
        tree_->setAlternatingRowColors(true);
        tree_->setUniformRowHeights(false);
        tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        tree_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        tree_->setContextMenuPolicy(Qt::CustomContextMenu);
        tree_->setMinimumHeight(420);

        root->addWidget(tree_, 1);

        connect(refreshBtn_, &QPushButton::clicked, this, &ElementHierarchyDialog::startFetch);
        connect(expandBtn_, &QPushButton::clicked, tree_, &QTreeWidget::expandAll);
        connect(collapseBtn_, &QPushButton::clicked, tree_, &QTreeWidget::collapseAll);
        connect(filterEdit_, &QLineEdit::textChanged, this, &ElementHierarchyDialog::applyFilter);
        connect(tree_, &QTreeWidget::customContextMenuRequested, this, &ElementHierarchyDialog::showTreeMenu);
        connect(tree_, &QTreeWidget::itemDoubleClicked, this, &ElementHierarchyDialog::onItemDoubleClicked);

        baseUrl_.setScheme(QStringLiteral("http"));
        baseUrl_.setHost(deviceInfo_->localIp);
        baseUrl_.setPort(8080);

        startFetch();
    }

    ~ElementHierarchyDialog() override { deleteSessionIfNeeded(); }

    void deleteSessionIfNeeded() {
        if (!networkAccessManager)
            return;
        const QString sid = sessionId_;
        sessionId_.clear();
        if (sid.isEmpty())
            return;

        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/session"));

        QNetworkRequest request(url);
        request.setRawHeader("X-YK-Session-Id", sid.toUtf8());
        request.setTransferTimeout(15000);

        QNetworkReply *reply = networkAccessManager->deleteResource(request);
        connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    }

    void setBusy(const QString &text) {
        statusLabel_->setText(text);
        refreshBtn_->setEnabled(false);
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    void setIdle(const QString &text) {
        statusLabel_->setText(text);
        refreshBtn_->setEnabled(true);
    }

    void startFetch() {
        abortPending();
        deleteSessionIfNeeded();

        tree_->clear();
        filterEdit_->clear();

        if (!networkAccessManager) {
            setIdle(QStringLiteral("网络模块未初始化"));
            return;
        }

        setBusy(QStringLiteral("正在创建会话…"));

        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/session/create"));

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setTransferTimeout(30000);

        QNetworkReply *reply = networkAccessManager->post(request, QByteArrayLiteral(R"({"name":"RemotePro层级树"})"));
        pendingReply_ = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (pendingReply_ == reply)
                pendingReply_ = nullptr;
            onSessionCreateFinished(reply);
        });
    }

    void abortPending() {
        if (pendingReply_) {
            pendingReply_->abort();
            pendingReply_->deleteLater();
            pendingReply_ = nullptr;
        }
    }

    void onSessionCreateFinished(QNetworkReply *reply) {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError)
                return;
            setIdle(QStringLiteral("创建会话失败：%1").arg(reply->errorString()));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject rootObj = doc.object();
        const QJsonObject val = rootObj.value(QStringLiteral("value")).toObject();
        sessionId_ = val.value(QStringLiteral("sessionId")).toString();
        if (sessionId_.isEmpty())
            sessionId_ = rootObj.value(QStringLiteral("sessionId")).toString();

        if (sessionId_.isEmpty()) {
            setIdle(QStringLiteral("创建会话失败：响应中无 sessionId"));
            return;
        }

        fetchSource();
    }

    void fetchSource() {
        setBusy(QStringLiteral("正在抓取界面元素树…"));

        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/element/source"));

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("X-YK-Session-Id", sessionId_.toUtf8());
        request.setTransferTimeout(120000);

        const QByteArray body = QByteArrayLiteral(
            R"({"options":{"maxDepth":50,"maxElements":8000,"format":"json","includeMeta":true}})");
        QNetworkReply *reply = networkAccessManager->post(request, body);
        pendingReply_ = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onSourceFinished(reply); });
    }

    void onSourceFinished(QNetworkReply *reply) {
        if (reply == pendingReply_)
            pendingReply_ = nullptr;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (reply->error() == QNetworkReply::OperationCanceledError)
                return;
            setIdle(QStringLiteral("获取元素树失败：%1").arg(reply->errorString()));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonObject rootObj = doc.object();
        const QJsonObject val = rootObj.value(QStringLiteral("value")).toObject();
        QJsonArray nodes = val.value(QStringLiteral("nodes")).toArray();

        if (nodes.isEmpty()) {
            const QJsonObject single = val.value(QStringLiteral("tree")).toObject();
            if (!single.isEmpty())
                nodes = QJsonArray{single};
        }

        tree_->clear();
        populateTree(nodes);

        QString meta;
        if (val.contains(QStringLiteral("duration_ms")))
            meta += QStringLiteral("抓取耗时 %1 ms").arg(val.value(QStringLiteral("duration_ms")).toInt());
        if (val.contains(QStringLiteral("http_bridge_duration_ms"))) {
            if (!meta.isEmpty())
                meta += QStringLiteral("，");
            meta += QStringLiteral("桥接 %1 ms").arg(val.value(QStringLiteral("http_bridge_duration_ms")).toInt());
        }
        if (meta.isEmpty())
            meta = QStringLiteral("完成");
        setIdle(QStringLiteral("共 %1 个顶层节点。%2").arg(tree_->topLevelItemCount()).arg(meta));
        applyFilter(filterEdit_->text());
    }

    static QString primaryText(const QJsonObject &o) {
        static const QStringList keys{QStringLiteral("label"), QStringLiteral("text"), QStringLiteral("name"),
                                      QStringLiteral("value"), QStringLiteral("title")};
        for (const QString &k : keys) {
            const QString s = o.value(k).toString();
            if (!s.isEmpty())
                return s;
        }
        return {};
    }

    static QString boundsText(const QJsonObject &o) {
        const QJsonObject b = o.value(QStringLiteral("bounds")).toObject();
        if (b.isEmpty())
            return {};

        if (b.contains(QStringLiteral("centerX")) && b.contains(QStringLiteral("centerY")))
            return QStringLiteral("(%1,%2)")
                .arg(b.value(QStringLiteral("centerX")).toInt())
                .arg(b.value(QStringLiteral("centerY")).toInt());

        if (b.contains(QStringLiteral("x")) && b.contains(QStringLiteral("y"))) {
            const int x = b.value(QStringLiteral("x")).toInt();
            const int y = b.value(QStringLiteral("y")).toInt();
            const int w = b.value(QStringLiteral("width")).toInt();
            const int h = b.value(QStringLiteral("height")).toInt();
            if (w > 0 && h > 0)
                return QStringLiteral("[%1,%2 %3x%4]").arg(x).arg(y).arg(w).arg(h);
            return QStringLiteral("(%1,%2)").arg(x).arg(y);
        }
        return {};
    }

    static void fillItemColumns(QTreeWidgetItem *item, const QJsonObject &o) {
        const QString type = o.value(QStringLiteral("type")).toString();
        const QString id = o.value(QStringLiteral("id")).toString();
        item->setText(0, type.isEmpty() ? QStringLiteral("—") : type);
        item->setText(1, primaryText(o));
        const QString bt = boundsText(o);
        item->setText(2, bt.isEmpty() ? QStringLiteral("—") : bt);
        item->setText(3, id.isEmpty() ? QStringLiteral("—") : id);
        item->setData(0, Qt::UserRole, QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));
    }

    void appendJsonObjectTree(QTreeWidgetItem *parentItem, const QJsonObject &o) {
        QTreeWidgetItem *row = parentItem ? new QTreeWidgetItem(parentItem) : new QTreeWidgetItem();
        fillItemColumns(row, o);
        if (!parentItem)
            tree_->addTopLevelItem(row);

        const QJsonArray ch = o.value(QStringLiteral("children")).toArray();
        for (const QJsonValue &v : ch) {
            if (v.isObject())
                appendJsonObjectTree(row, v.toObject());
        }
    }

    void populateFromDepthList(const QJsonArray &nodes) {
        QTreeWidgetItem *depthParent[96]{};

        for (const QJsonValue &v : nodes) {
            if (!v.isObject())
                continue;
            const QJsonObject o = v.toObject();
            int depth = o.value(QStringLiteral("depth")).toInt(-1);
            if (depth < 0)
                depth = o.value(QStringLiteral("level")).toInt(0);
            depth = qBound(0, depth, 95);

            auto *item = new QTreeWidgetItem();
            fillItemColumns(item, o);

            QTreeWidgetItem *parent = nullptr;
            if (depth > 0) {
                for (int p = depth - 1; p >= 0; --p) {
                    if (depthParent[p]) {
                        parent = depthParent[p];
                        break;
                    }
                }
            }

            if (parent)
                parent->addChild(item);
            else
                tree_->addTopLevelItem(item);

            depthParent[depth] = item;
            for (int i = depth + 1; i < 96; ++i)
                depthParent[i] = nullptr;
        }
    }

    void populateTree(const QJsonArray &nodes) {
        if (nodes.isEmpty())
            return;

        bool anyChildren = false;
        bool anyDepth = false;
        for (const QJsonValue &v : nodes) {
            if (!v.isObject())
                continue;
            const QJsonObject o = v.toObject();
            if (o.contains(QStringLiteral("children")) && o.value(QStringLiteral("children")).isArray()
                && !o.value(QStringLiteral("children")).toArray().isEmpty())
                anyChildren = true;
            if (o.contains(QStringLiteral("depth")) || o.contains(QStringLiteral("level")))
                anyDepth = true;
        }

        if (anyChildren) {
            for (const QJsonValue &v : nodes) {
                if (v.isObject())
                    appendJsonObjectTree(nullptr, v.toObject());
            }
            return;
        }

        if (anyDepth) {
            populateFromDepthList(nodes);
            return;
        }

        for (const QJsonValue &v : nodes) {
            if (!v.isObject())
                continue;
            auto *item = new QTreeWidgetItem();
            fillItemColumns(item, v.toObject());
            tree_->addTopLevelItem(item);
        }
    }

    static bool itemMatchesFilter(QTreeWidgetItem *item, const QString &needle) {
        if (!item)
            return false;
        if (needle.isEmpty())
            return true;
        for (int c = 0; c < item->columnCount(); ++c) {
            if (item->text(c).contains(needle, Qt::CaseInsensitive))
                return true;
        }
        for (int i = 0; i < item->childCount(); ++i) {
            if (itemMatchesFilter(item->child(i), needle))
                return true;
        }
        return false;
    }

    static void applyFilterToItem(QTreeWidgetItem *item, const QString &needle) {
        bool selfOrDesc = itemMatchesFilter(item, needle);
        item->setHidden(!selfOrDesc);
        if (selfOrDesc && !needle.isEmpty())
            item->setExpanded(true);
        for (int i = 0; i < item->childCount(); ++i)
            applyFilterToItem(item->child(i), needle);
    }

    void applyFilter(const QString &text) {
        const QString needle = text.trimmed();
        for (int i = 0; i < tree_->topLevelItemCount(); ++i)
            applyFilterToItem(tree_->topLevelItem(i), needle);
    }

    void showTreeMenu(const QPoint &pos) {
        QTreeWidgetItem *item = tree_->itemAt(pos);
        QMenu menu(this);
        if (item) {
            menu.addAction(QStringLiteral("复制此节点 JSON"), this, [this, item]() {
                const QString json = item->data(0, Qt::UserRole).toString();
                if (!json.isEmpty())
                    QApplication::clipboard()->setText(json);
            });
            menu.addAction(QStringLiteral("复制中心点坐标 (x,y)"), this, [item]() {
                const QJsonDocument doc = QJsonDocument::fromJson(item->data(0, Qt::UserRole).toString().toUtf8());
                const QJsonObject o = doc.object();
                const QJsonObject b = o.value(QStringLiteral("bounds")).toObject();
                if (b.contains(QStringLiteral("centerX")) && b.contains(QStringLiteral("centerY"))) {
                    QApplication::clipboard()->setText(
                        QStringLiteral("%1,%2")
                            .arg(b.value(QStringLiteral("centerX")).toInt())
                            .arg(b.value(QStringLiteral("centerY")).toInt()));
                }
            });
        }
        menu.addAction(QStringLiteral("复制设备 Base URL"), this, [this]() {
            QApplication::clipboard()->setText(baseUrl_.toString());
        });
        menu.exec(tree_->viewport()->mapToGlobal(pos));
    }

    void onItemDoubleClicked(QTreeWidgetItem *item, int column) {
        Q_UNUSED(column);
        if (!item)
            return;
        const QString json = item->data(0, Qt::UserRole).toString();
        if (!json.isEmpty())
            QApplication::clipboard()->setText(json);
        new ToastWidget(QStringLiteral("已复制节点 JSON 到剪贴板"), this);
    }

    DeviceInfo *deviceInfo_ = nullptr;
    QUrl baseUrl_;
    QString sessionId_;
    QNetworkReply *pendingReply_ = nullptr;

    QLabel *statusLabel_ = nullptr;
    QPushButton *refreshBtn_ = nullptr;
    QPushButton *expandBtn_ = nullptr;
    QPushButton *collapseBtn_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    QTreeWidget *tree_ = nullptr;
};
