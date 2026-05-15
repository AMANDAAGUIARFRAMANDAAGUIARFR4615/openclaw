#pragma once

#include "BaseDialog.h"
#include "DeviceConnection.h"
#include "DeviceInfo.h"
#include "ToastWidget.h"
#include "global.h"
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QWidget>
#include <algorithm>
#include <functional>
#include <limits>
#include <numeric>

/** 通过设备 HTTP（参见 docs：设备 ip:8080 转发脚本服务）拉取 /element/source 并以树形展示。 */
class ElementHierarchyDialog : public BaseDialog {
    struct ElementHit {
        QRect bounds;
        QTreeWidgetItem *item = nullptr;
        int index = 0;
    };

    class HierarchyPreviewWidget : public QWidget {
    public:
        explicit HierarchyPreviewWidget(ElementHierarchyDialog *dlg)
            : QWidget(dlg), dlg_(dlg) {
            setMinimumSize(240, 420);
            setMouseTracking(false);
            setCursor(Qt::CrossCursor);
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            Q_UNUSED(event);
            dlg_->drawHierarchyPreview(this);
        }

        void mousePressEvent(QMouseEvent *event) override {
            dlg_->handleHierarchyPreviewClick(this, event->pos());
            QWidget::mousePressEvent(event);
        }

    private:
        ElementHierarchyDialog *dlg_;
    };

    friend class HierarchyPreviewWidget;

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
        dialog.resize(1100, 620);
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

        split_ = new QSplitter(Qt::Horizontal, this);
        preview_ = new HierarchyPreviewWidget(this);
        auto *previewScroll = new QScrollArea(this);
        previewScroll->setFrameShape(QFrame::NoFrame);
        previewScroll->setWidgetResizable(true);
        previewScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        previewScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        previewScroll->setWidget(preview_);

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

        split_->addWidget(previewScroll);
        split_->addWidget(tree_);
        split_->setStretchFactor(0, 2);
        split_->setStretchFactor(1, 3);
        split_->setCollapsible(0, false);
        split_->setCollapsible(1, false);
        root->addWidget(split_, 1);

        connect(refreshBtn_, &QPushButton::clicked, this, &ElementHierarchyDialog::startFetch);
        connect(expandBtn_, &QPushButton::clicked, tree_, &QTreeWidget::expandAll);
        connect(collapseBtn_, &QPushButton::clicked, tree_, &QTreeWidget::collapseAll);
        connect(filterEdit_, &QLineEdit::textChanged, this, &ElementHierarchyDialog::applyFilter);
        connect(tree_, &QTreeWidget::customContextMenuRequested, this, &ElementHierarchyDialog::showTreeMenu);
        connect(tree_, &QTreeWidget::itemDoubleClicked, this, &ElementHierarchyDialog::onItemDoubleClicked);
        connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *, QTreeWidgetItem *) {
            if (preview_)
                preview_->update();
        });

        baseUrl_.setScheme(QStringLiteral("http"));
        baseUrl_.setHost(deviceInfo_->localIp);
        baseUrl_.setPort(8080);

        startFetch();
    }

    ~ElementHierarchyDialog() override { deleteSessionIfNeeded(); }

    void deleteSessionIfNeeded() {
        abortImageReplies();
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
        hierarchyHits_.clear();
        hierarchyScreenshot_ = QPixmap();
        if (preview_)
            preview_->update();

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
        abortImageReplies();
    }

    void abortImageReplies() {
        if (imageCaptureReply_) {
            imageCaptureReply_->abort();
            imageCaptureReply_->deleteLater();
            imageCaptureReply_ = nullptr;
        }
        if (imageBase64Reply_) {
            imageBase64Reply_->abort();
            imageBase64Reply_->deleteLater();
            imageBase64Reply_ = nullptr;
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

        hierarchyScreenshot_ = QPixmap();
        hierarchyHits_.clear();
        fetchHierarchyScreenshot();

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
        rebuildHierarchyHits();
    }

    static QRect boundsToRect(const QJsonObject &o) {
        const QJsonObject b = o.value(QStringLiteral("bounds")).toObject();
        if (b.isEmpty())
            return {};

        if (b.contains(QStringLiteral("x")) && b.contains(QStringLiteral("y"))) {
            const int x = b.value(QStringLiteral("x")).toInt();
            const int y = b.value(QStringLiteral("y")).toInt();
            int w = b.value(QStringLiteral("width")).toInt();
            int h = b.value(QStringLiteral("height")).toInt();
            if (w <= 0 || h <= 0) {
                if (b.contains(QStringLiteral("centerX")) && b.contains(QStringLiteral("centerY"))) {
                    const int cx = b.value(QStringLiteral("centerX")).toInt();
                    const int cy = b.value(QStringLiteral("centerY")).toInt();
                    return QRect(cx - 1, cy - 1, 3, 3);
                }
                return {};
            }
            return QRect(x, y, w, h);
        }

        if (b.contains(QStringLiteral("centerX")) && b.contains(QStringLiteral("centerY"))) {
            const int cx = b.value(QStringLiteral("centerX")).toInt();
            const int cy = b.value(QStringLiteral("centerY")).toInt();
            int w = b.value(QStringLiteral("width")).toInt();
            int h = b.value(QStringLiteral("height")).toInt();
            if (w > 0 && h > 0)
                return QRect(cx - w / 2, cy - h / 2, w, h);
            return QRect(cx - 1, cy - 1, 3, 3);
        }
        return {};
    }

    static QRect fittedPixmapRect(const QSize &widgetSize, const QSize &pixmapSize) {
        if (pixmapSize.width() <= 0 || pixmapSize.height() <= 0)
            return QRect(QPoint(0, 0), widgetSize);
        const qreal wr = qreal(widgetSize.width()) / qreal(pixmapSize.width());
        const qreal hr = qreal(widgetSize.height()) / qreal(pixmapSize.height());
        const qreal ratio = qMin(wr, hr);
        const int w = qMax(1, int(qRound(qreal(pixmapSize.width()) * ratio)));
        const int h = qMax(1, int(qRound(qreal(pixmapSize.height()) * ratio)));
        const int x = (widgetSize.width() - w) / 2;
        const int y = (widgetSize.height() - h) / 2;
        return QRect(x, y, w, h);
    }

    QRect hierarchyScreenBounds() const {
        if (!hierarchyScreenshot_.isNull())
            return QRect(QPoint(0, 0), hierarchyScreenshot_.size());
        QRect u(0, 0, 1, 1);
        for (const ElementHit &h : hierarchyHits_)
            u = u.united(h.bounds);
        return u;
    }

    void rebuildHierarchyHits() {
        hierarchyHits_.clear();
        int idx = 0;
        const std::function<void(QTreeWidgetItem *)> walk = [&](QTreeWidgetItem *it) {
            if (!it || it->isHidden())
                return;
            const QJsonObject o =
                QJsonDocument::fromJson(it->data(0, Qt::UserRole).toString().toUtf8()).object();
            const QRect r = boundsToRect(o);
            if (r.isValid() && !r.isEmpty())
                hierarchyHits_.push_back({r, it, ++idx});
            for (int i = 0; i < it->childCount(); ++i)
                walk(it->child(i));
        };
        for (int i = 0; i < tree_->topLevelItemCount(); ++i)
            walk(tree_->topLevelItem(i));
        if (preview_)
            preview_->update();
    }

    void fetchHierarchyScreenshot() {
        abortImageReplies();
        if (!networkAccessManager || sessionId_.isEmpty())
            return;

        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/image/capture"));

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("X-YK-Session-Id", sessionId_.toUtf8());
        request.setTransferTimeout(60000);

        QNetworkReply *reply = networkAccessManager->post(request, QByteArrayLiteral("{}"));
        imageCaptureReply_ = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (imageCaptureReply_ == reply)
                imageCaptureReply_ = nullptr;
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                if (reply->error() != QNetworkReply::OperationCanceledError && preview_)
                    preview_->update();
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonObject val = root.value(QStringLiteral("value")).toObject();
            if (val.contains(QStringLiteral("error")))
                return;

            QString imageId = val.value(QStringLiteral("imageId")).toString();
            if (imageId.isEmpty())
                imageId = val.value(QStringLiteral("id")).toString();
            if (imageId.isEmpty())
                imageId = root.value(QStringLiteral("imageId")).toString();
            if (imageId.isEmpty())
                return;

            fetchImageBase64(imageId);
        });
    }

    void fetchImageBase64(const QString &imageId) {
        if (!networkAccessManager || sessionId_.isEmpty() || imageId.isEmpty())
            return;

        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/image/toBase64"));

        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("X-YK-Session-Id", sessionId_.toUtf8());
        request.setTransferTimeout(120000);

        const QJsonObject body{{QStringLiteral("imageId"), imageId},
                               {QStringLiteral("format"), QStringLiteral("jpg")},
                               {QStringLiteral("quality"), 0.85}};
        QNetworkReply *reply =
            networkAccessManager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
        imageBase64Reply_ = reply;
        connect(reply, &QNetworkReply::finished, this, [this, reply, imageId]() {
            if (imageBase64Reply_ == reply)
                imageBase64Reply_ = nullptr;
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError) {
                if (preview_)
                    preview_->update();
                return;
            }

            const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
            const QJsonObject val = root.value(QStringLiteral("value")).toObject();
            QString b64 = val.value(QStringLiteral("data")).toString();
            if (b64.isEmpty())
                b64 = val.value(QStringLiteral("base64")).toString();
            if (b64.isEmpty())
                b64 = val.value(QStringLiteral("content")).toString();
            if (b64.isEmpty() && root.value(QStringLiteral("value")).isString())
                b64 = root.value(QStringLiteral("value")).toString();

            const QByteArray raw = QByteArray::fromBase64(b64.toLatin1());
            if (!raw.isEmpty())
                hierarchyScreenshot_.loadFromData(raw, "JPEG");
            if (hierarchyScreenshot_.isNull())
                hierarchyScreenshot_.loadFromData(raw);

            releaseImageId(imageId);

            if (preview_)
                preview_->update();
        });
    }

    void releaseImageId(const QString &imageId) {
        if (imageId.isEmpty() || !networkAccessManager || sessionId_.isEmpty())
            return;
        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/image/release"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("X-YK-Session-Id", sessionId_.toUtf8());
        req.setTransferTimeout(15000);
        const QByteArray body =
            QJsonDocument(QJsonObject{{QStringLiteral("imageId"), imageId}}).toJson(QJsonDocument::Compact);
        QNetworkReply *rel = networkAccessManager->post(req, body);
        connect(rel, &QNetworkReply::finished, rel, &QNetworkReply::deleteLater);
    }

    static void expandItemAncestors(QTreeWidgetItem *item) {
        for (QTreeWidgetItem *p = item ? item->parent() : nullptr; p; p = p->parent())
            p->setExpanded(true);
    }

    QTreeWidgetItem *pickHitAtScreen(int sx, int sy) const {
        QTreeWidgetItem *best = nullptr;
        int bestArea = (std::numeric_limits<int>::max)();
        for (const ElementHit &h : hierarchyHits_) {
            if (!h.bounds.contains(sx, sy))
                continue;
            const int a = qMax(1, h.bounds.width()) * qMax(1, h.bounds.height());
            if (a > bestArea)
                continue;
            bestArea = a;
            best = h.item;
        }
        return best;
    }

    void handleHierarchyPreviewClick(HierarchyPreviewWidget *w, const QPoint &localPos) {
        if (!tree_ || !w)
            return;
        const QRect u = hierarchyScreenBounds();
        if (u.width() <= 0 || u.height() <= 0)
            return;

        const QRect fitted = fittedPixmapRect(w->size(), u.size());
        if (!fitted.contains(localPos))
            return;

        const double relX = (localPos.x() - fitted.x()) * double(u.width()) / double(fitted.width());
        const double relY = (localPos.y() - fitted.y()) * double(u.height()) / double(fitted.height());
        const int screenX = int(qFloor(u.x() + relX));
        const int screenY = int(qFloor(u.y() + relY));

        QTreeWidgetItem *hit = pickHitAtScreen(screenX, screenY);
        if (!hit)
            return;
        expandItemAncestors(hit);
        tree_->setCurrentItem(hit);
        tree_->scrollToItem(hit, QAbstractItemView::EnsureVisible);
        preview_->update();
    }

    void drawHierarchyPreview(HierarchyPreviewWidget *w) {
        QPainter p(w);
        p.fillRect(w->rect(), QColor(36, 36, 38));

        if (hierarchyHits_.isEmpty()) {
            if (!hierarchyScreenshot_.isNull()) {
                const QRect fitted = fittedPixmapRect(w->size(), hierarchyScreenshot_.size());
                p.drawPixmap(fitted, hierarchyScreenshot_, hierarchyScreenshot_.rect());
            }
            p.setPen(QColor(220, 220, 225));
            p.drawText(w->rect(), Qt::AlignCenter,
                       QStringLiteral("当前可见节点无有效 bounds，无法在图上叠加选区"));
            return;
        }

        const QRect u = hierarchyScreenBounds();
        if (u.width() <= 0 || u.height() <= 0) {
            p.setPen(QColor(200, 200, 200));
            p.drawText(w->rect(), Qt::AlignCenter, QStringLiteral("无法计算画面坐标范围"));
            return;
        }

        const QRect fitted = fittedPixmapRect(w->size(), u.size());
        if (!hierarchyScreenshot_.isNull()) {
            p.drawPixmap(fitted, hierarchyScreenshot_, hierarchyScreenshot_.rect());
        } else {
            p.fillRect(fitted, QColor(28, 28, 30));
            p.setPen(QColor(120, 120, 125));
            p.drawText(fitted, Qt::AlignCenter, QStringLiteral("截图加载失败或未返回\n仍可根据边界框点击"));
        }

        QTreeWidgetItem *sel = tree_ ? tree_->currentItem() : nullptr;

        QVector<int> order(hierarchyHits_.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [this](int a, int b) {
            const QRect &ra = hierarchyHits_[a].bounds;
            const QRect &rb = hierarchyHits_[b].bounds;
            const qint64 aa = qint64(ra.width()) * ra.height();
            const qint64 ba = qint64(rb.width()) * rb.height();
            return aa > ba;
        });

        p.setRenderHint(QPainter::TextAntialiasing, true);
        const QFont smallFont = []() {
            QFont f;
            f.setPixelSize(10);
            return f;
        }();

        for (int k : order) {
            const ElementHit &h = hierarchyHits_[k];
            const QRect rScreen = h.bounds;
            const QRect r(
                fitted.x() + int(qRound((rScreen.x() - u.x()) * double(fitted.width()) / double(u.width()))),
                fitted.y() + int(qRound((rScreen.y() - u.y()) * double(fitted.height()) / double(u.height()))),
                qMax(1, int(qRound(rScreen.width() * double(fitted.width()) / double(u.width())))),
                qMax(1, int(qRound(rScreen.height() * double(fitted.height()) / double(u.height())))));
            const bool isSel = sel && h.item == sel;
            p.setPen(QPen(isSel ? QColor(255, 214, 10) : QColor(60, 220, 120), isSel ? 3 : 1));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);

            const QString cap = QStringLiteral("#%1 %2").arg(h.index).arg(h.item ? h.item->text(0) : QString());
            p.setFont(smallFont);
            const QRect textBg = r.adjusted(0, -14, 0, 0).united(r);
            p.fillRect(textBg.intersected(fitted), QColor(0, 0, 0, 140));
            p.setPen(isSel ? QColor(255, 240, 160) : QColor(200, 255, 210));
            p.drawText(textBg.intersected(fitted), Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine, cap);
        }
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
            menu.addAction(QStringLiteral("点击此元素"), this, [this, item]() { requestTapElement(item); });
        }
        if (menu.isEmpty())
            return;
        menu.exec(tree_->viewport()->mapToGlobal(pos));
    }

    void requestTapElement(QTreeWidgetItem *item) {
        const QJsonObject element =
            (item && !sessionId_.isEmpty() && networkAccessManager)
                ? QJsonDocument::fromJson(item->data(0, Qt::UserRole).toString().toUtf8()).object()
                : QJsonObject{};
        if (element.isEmpty()) {
            new ToastWidget(QStringLiteral("无法点击"), this);
            return;
        }

        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/element/tap"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("X-YK-Session-Id", sessionId_.toUtf8());
        req.setTransferTimeout(30000);

        const QJsonObject body{{QStringLiteral("element"), element},
                               {QStringLiteral("options"),
                                QJsonObject{{QStringLiteral("tapMethod"), QStringLiteral("auto")},
                                            {QStringLiteral("durationMs"), 30}}}};
        QNetworkReply *reply =
            networkAccessManager->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            const QByteArray raw = reply->readAll();
            if (reply->error() != QNetworkReply::NoError) {
                new ToastWidget(QStringLiteral("点击失败：%1").arg(reply->errorString()), this);
                return;
            }
            const bool ok =
                QJsonDocument::fromJson(raw).object().value(QStringLiteral("value")).toObject().value(
                    QStringLiteral("success")).toBool();
            new ToastWidget(ok ? QStringLiteral("点击已发送") : QStringLiteral("点击未成功"), this);
        });
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
    QNetworkReply *imageCaptureReply_ = nullptr;
    QNetworkReply *imageBase64Reply_ = nullptr;

    QPixmap hierarchyScreenshot_;
    QVector<ElementHit> hierarchyHits_;

    QLabel *statusLabel_ = nullptr;
    QPushButton *refreshBtn_ = nullptr;
    QPushButton *expandBtn_ = nullptr;
    QPushButton *collapseBtn_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    QSplitter *split_ = nullptr;
    HierarchyPreviewWidget *preview_ = nullptr;
    QTreeWidget *tree_ = nullptr;
};
