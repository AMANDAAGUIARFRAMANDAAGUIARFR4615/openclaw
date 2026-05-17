#pragma once

#include "BaseDialog.h"
#include "DeviceConnection.h"
#include "DeviceInfo.h"
#include "ToastWidget.h"
#include "HttpUtil.h"
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
#include <QMouseEvent>
#include <QNetworkReply>
#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QPixmap>
#include <QPushButton>
#include <QSizePolicy>
#include <QSplitter>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QEvent>
#include <QWidget>
#include <QLatin1String>
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
            setMinimumSize(1, 1);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            setMouseTracking(true);
            setCursor(Qt::CrossCursor);
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            Q_UNUSED(event);
            dlg_->drawHierarchyPreview(this);
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            dlg_->onPreviewHoverMove(this, event->pos());
            QWidget::mouseMoveEvent(event);
        }

        void leaveEvent(QEvent *event) override {
            dlg_->onPreviewHoverLeave();
            QWidget::leaveEvent(event);
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
        : BaseDialog(QStringLiteral("界面层级树"), parent, false), deviceInfo_(deviceInfo) {
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
        tree_->setMinimumHeight(160);
        tree_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

        split_->addWidget(preview_);
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

    [[nodiscard]] HttpUtil::Sender net() {
        return HttpUtil::Sender{networkAccessManager, this};
    }

    /** 设备 HTTP 根路径 + path，不含 Session 头 */
    [[nodiscard]] HttpUtil::Request api(QLatin1String route) {
        return HttpUtil::Request::relative(baseUrl_, route);
    }

    [[nodiscard]] HttpUtil::Request api(QLatin1String route, const QString &ykSid) {
        return api(route).ykSession(ykSid);
    }

    /** 带当前 sessionId 的 X-YK-Session-Id */
    [[nodiscard]] HttpUtil::Request apiSes(QLatin1String route) {
        return api(route).ykSession(sessionId_);
    }

    void deleteSessionIfNeeded() {
        abortImageReplies();
        if (!networkAccessManager)
            return;
        const QString sid = sessionId_;
        sessionId_.clear();
        if (sid.isEmpty())
            return;

        net().fire(api(QLatin1String("/session"), sid).del().timeout(15000));
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
        previewHoverItem_ = nullptr;
        if (preview_)
            preview_->update();

        if (!networkAccessManager) {
            setIdle(QStringLiteral("网络模块未初始化"));
            return;
        }

        setBusy(QStringLiteral("正在创建会话…"));

        pendingReply_ =
            net().submit(api(QLatin1String("/session/create"))
                             .postJson(QByteArrayLiteral(R"({})"))
                             .timeout(30000),
                         [this](const HttpUtil::Result &r) {
                             if (pendingReply_ == r.reply)
                                 pendingReply_ = nullptr;
                             onSessionCreateFinished(r);
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
        if (imageDownloadReply_) {
            imageDownloadReply_->abort();
            imageDownloadReply_->deleteLater();
            imageDownloadReply_ = nullptr;
        }
    }

    void onSessionCreateFinished(const HttpUtil::Result &r) {
        if (r.canceled())
            return;
        if (!r.ok()) {
            setIdle(QStringLiteral("创建会话失败：%1").arg(r.errorString));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(r.bytes);
        const QJsonObject rootObj = doc.object();
        sessionId_ = rootObj.value(QStringLiteral("value")).toObject().value(QStringLiteral("sessionId")).toString();
        if (sessionId_.isEmpty()) {
            setIdle(QStringLiteral("创建会话失败：响应中无 sessionId"));
            return;
        }

        fetchSource();
    }

    void fetchSource() {
        setBusy(QStringLiteral("正在抓取界面元素树…"));

        pendingReply_ = net().submit(
            apiSes(QLatin1String("/element/source")).postJson(QByteArrayLiteral(R"({})")).timeout(120000),
            [this](const HttpUtil::Result &r) {
                if (pendingReply_ == r.reply)
                    pendingReply_ = nullptr;
                onSourceFinished(r);
            });
    }

    void onSourceFinished(const HttpUtil::Result &r) {
        if (r.canceled())
            return;
        if (!r.ok()) {
            setIdle(QStringLiteral("获取元素树失败：%1").arg(r.errorString));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(r.bytes);
        const QJsonObject rootObj = doc.object();
        const QJsonObject val = rootObj.value(QStringLiteral("value")).toObject();
        const QJsonArray tree = val.value(QStringLiteral("tree")).toArray();

        tree_->clear();
        populateTree(tree);

        hierarchyScreenshot_ = QPixmap();
        hierarchyHits_.clear();
        previewHoverItem_ = nullptr;
        fetchHierarchyScreenshot();

        setIdle(QStringLiteral("共 %1 个顶层节点").arg(tree_->topLevelItemCount()));
        applyFilter(filterEdit_->text());
    }

    static QString primaryText(const QJsonObject &o) {
        const QString lbl = o.value(QStringLiteral("label")).toString();
        if (!lbl.isEmpty())
            return lbl;
        return o.value(QStringLiteral("value")).toString();
    }

    /** 文档 bounds：center 或 x/y/width/height */
    static bool boundsCenterPx(const QJsonObject &o, int &outX, int &outY) {
        const QJsonObject b = o.value(QStringLiteral("bounds")).toObject();
        if (b.contains(QStringLiteral("centerX")) && b.contains(QStringLiteral("centerY"))) {
            outX = qRound(b.value(QStringLiteral("centerX")).toDouble());
            outY = qRound(b.value(QStringLiteral("centerY")).toDouble());
            return true;
        }
        const int x = b.value(QStringLiteral("x")).toInt();
        const int y = b.value(QStringLiteral("y")).toInt();
        const int w = b.value(QStringLiteral("width")).toInt();
        const int h = b.value(QStringLiteral("height")).toInt();
        if (w > 0 && h > 0) {
            outX = x + w / 2;
            outY = y + h / 2;
            return true;
        }
        return false;
    }

    static QString boundsText(const QJsonObject &o) {
        int cx = 0, cy = 0;
        if (!boundsCenterPx(o, cx, cy))
            return {};
        return QStringLiteral("(%1,%2)").arg(cx).arg(cy);
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

    void populateTree(const QJsonArray &roots) {
        for (const QJsonValue &v : roots) {
            if (!v.isObject())
                continue;
            appendJsonObjectTree(nullptr, v.toObject());
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
        const int x = b.value(QStringLiteral("x")).toInt();
        const int y = b.value(QStringLiteral("y")).toInt();
        const int w = b.value(QStringLiteral("width")).toInt();
        const int h = b.value(QStringLiteral("height")).toInt();
        if (w > 0 && h > 0)
            return QRect(x, y, w, h);
        int cx = 0, cy = 0;
        if (boundsCenterPx(o, cx, cy))
            return QRect(cx - 1, cy - 1, 3, 3);
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

        const QByteArray captureBody = QByteArrayLiteral(R"({})");
        imageCaptureReply_ = net().submit(
            apiSes(QLatin1String("/image/capture")).postJson(captureBody).timeout(60000),
            [this](const HttpUtil::Result &r) {
                if (imageCaptureReply_ == r.reply)
                    imageCaptureReply_ = nullptr;

                if (!r.ok()) {
                    if (!r.canceled() && preview_)
                        preview_->update();
                    return;
                }

                const QJsonObject val =
                    QJsonDocument::fromJson(r.bytes).object().value(QStringLiteral("value")).toObject();
                if (val.contains(QStringLiteral("error"))) {
                    if (preview_)
                        preview_->update();
                    return;
                }

                const QString imageId = val.value(QStringLiteral("imageId")).toString();
                if (imageId.isEmpty()) {
                    if (preview_)
                        preview_->update();
                    return;
                }
                fetchHierarchyImageDownload(imageId);
            });
    }

    void fetchHierarchyImageDownload(const QString &imageId) {
        if (!networkAccessManager || sessionId_.isEmpty() || imageId.isEmpty())
            return;

        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/image/download"));
        url.setQuery(QStringLiteral("format=jpg&imageId=")
                     + QString::fromLatin1(QUrl::toPercentEncoding(imageId)));

        imageDownloadReply_ =
            net().submit(HttpUtil::Request::absolute(url).get().ykSession(sessionId_).timeout(120000),
                         [this, imageId](const HttpUtil::Result &r) {
                             if (imageDownloadReply_ == r.reply)
                                 imageDownloadReply_ = nullptr;

                             hierarchyScreenshot_ = QPixmap();
                             if (r.ok() && !r.bytes.isEmpty()) {
                                 if (!hierarchyScreenshot_.loadFromData(r.bytes, "JPEG"))
                                     hierarchyScreenshot_.loadFromData(r.bytes);
                             }
                             if (preview_)
                                 preview_->update();
                             releaseImageId(imageId);
                         });
    }

    void releaseImageId(const QString &imageId) {
        if (imageId.isEmpty() || !networkAccessManager || sessionId_.isEmpty())
            return;
        const QByteArray body =
            QJsonDocument(QJsonObject{{QStringLiteral("imageId"), imageId}})
                .toJson(QJsonDocument::Compact);
        net().fire(apiSes(QLatin1String("/image/release")).postJson(body).timeout(15000));
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

    /** 预览控件本地坐标 → 层级/截图坐标系 (screenX, screenY)，鼠标在画布外或未就绪时返回 false。 */
    bool previewLocalToScreen(HierarchyPreviewWidget *w, const QPoint &localPos, int &screenX,
                              int &screenY) const {
        if (!w)
            return false;
        const QRect u = hierarchyScreenBounds();
        if (u.width() <= 0 || u.height() <= 0)
            return false;
        const QRect fitted = fittedPixmapRect(w->size(), u.size());
        if (!fitted.contains(localPos))
            return false;
        const double relX = (localPos.x() - fitted.x()) * double(u.width()) / double(fitted.width());
        const double relY = (localPos.y() - fitted.y()) * double(u.height()) / double(fitted.height());
        screenX = int(qFloor(u.x() + relX));
        screenY = int(qFloor(u.y() + relY));
        return true;
    }

    void onPreviewHoverMove(HierarchyPreviewWidget *w, const QPoint &localPos) {
        int sx = 0, sy = 0;
        if (!previewLocalToScreen(w, localPos, sx, sy)) {
            if (previewHoverItem_) {
                previewHoverItem_ = nullptr;
                if (preview_)
                    preview_->update();
            }
            return;
        }
        QTreeWidgetItem *hit = pickHitAtScreen(sx, sy);
        if (hit != previewHoverItem_) {
            previewHoverItem_ = hit;
            if (preview_)
                preview_->update();
        }
    }

    void onPreviewHoverLeave() {
        if (!previewHoverItem_)
            return;
        previewHoverItem_ = nullptr;
        if (preview_)
            preview_->update();
    }

    void handleHierarchyPreviewClick(HierarchyPreviewWidget *w, const QPoint &localPos) {
        if (!tree_ || !w)
            return;

        int screenX = 0, screenY = 0;
        if (!previewLocalToScreen(w, localPos, screenX, screenY))
            return;

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
        // 整块预览先铺黑底，缩放后的截图居中绘制后形成上下或左右黑边（letterbox）。
        p.fillRect(w->rect(), Qt::black);

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
            const bool isHover = previewHoverItem_ && h.item == previewHoverItem_;
            p.setPen(QPen(isSel ? QColor(255, 214, 10) : QColor(60, 220, 120), isSel ? 3 : (isHover ? 2 : 1)));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);

            if (!isSel && !isHover)
                continue;

            const QString cap = QStringLiteral("#%1 %2").arg(h.index).arg(h.item ? h.item->text(0) : QString());
            p.setFont(smallFont);
            const QFontMetrics fm(p.font());
            const int pad = 2;
            const int tw = fm.horizontalAdvance(cap);
            const int th = fm.height();
            QRect textBg(r.left(),
                         qMax(fitted.top(), r.top() - th - pad * 2),
                         tw + pad * 2,
                         th + pad * 2);
            textBg = textBg.intersected(fitted);
            if (!textBg.isEmpty()) {
                // 仅用文字条做小范围底色，避免按整框半透明叠涂导致整页发暗。
                p.fillRect(textBg, QColor(0, 0, 0, 88));
            }
            bool warmText = false;
            if (isHover && !isSel)
                warmText = true;
            p.setPen(isSel               ? QColor(255, 240, 160)
                       : warmText        ? QColor(180, 230, 255)
                                         : QColor(200, 255, 210));
            p.drawText(textBg, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, cap);
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
                int cx = 0, cy = 0;
                if (!boundsCenterPx(QJsonDocument::fromJson(item->data(0, Qt::UserRole).toString().toUtf8()).object(),
                                    cx, cy))
                    return;
                QApplication::clipboard()->setText(QStringLiteral("%1,%2").arg(cx).arg(cy));
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

        int tx = 0, ty = 0;
        if (!boundsCenterPx(element, tx, ty)) {
            new ToastWidget(QStringLiteral("无法解析元素中心坐标"), this);
            return;
        }

        const QJsonObject body{{QStringLiteral("x"), tx}, {QStringLiteral("y"), ty}};
        net().submit(apiSes(QLatin1String("/element/tap"))
                         .postJson(QJsonDocument(body).toJson(QJsonDocument::Compact))
                         .timeout(30000),
                     [this](const HttpUtil::Result &r) {
                         if (!r.ok()) {
                             new ToastWidget(QStringLiteral("点击失败：%1").arg(r.errorString), this);
                             return;
                         }
                         const bool ok = QJsonDocument::fromJson(r.bytes)
                                             .object()
                                             .value(QStringLiteral("value"))
                                             .toObject()
                                             .value(QStringLiteral("success"))
                                             .toBool();
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
    QNetworkReply *imageDownloadReply_ = nullptr;

    QPixmap hierarchyScreenshot_;
    QVector<ElementHit> hierarchyHits_;

    QLabel *statusLabel_ = nullptr;
    QPushButton *refreshBtn_ = nullptr;
    QPushButton *expandBtn_ = nullptr;
    QPushButton *collapseBtn_ = nullptr;
    QTreeWidgetItem *previewHoverItem_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    QSplitter *split_ = nullptr;
    HierarchyPreviewWidget *preview_ = nullptr;
    QTreeWidget *tree_ = nullptr;
};
