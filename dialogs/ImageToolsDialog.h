#pragma once

#include "BaseDialog.h"
#include "DeviceInfo.h"
#include "HttpUtil.h"
#include "ToastWidget.h"
#include "global.h"
#include <QAction>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QNetworkReply>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QStyleHints>
#include <QToolButton>
#include <QVBoxLayout>
#include <optional>

/** 通过设备 HTTP（docs 模块 09：image 接口）可视化调试截图、取色、选区、找色与找图。 */
class ImageToolsDialog : public BaseDialog {
    enum class ToolMode { PickPixel, SelectRegion, FindColor, FindImage };

    class PreviewWidget : public QWidget {
    public:
        explicit PreviewWidget(ImageToolsDialog *dlg) : QWidget(dlg), dlg_(dlg) {
            setMinimumSize(240, 320);
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            setMouseTracking(true);
        }

    protected:
        void paintEvent(QPaintEvent *event) override {
            Q_UNUSED(event);
            dlg_->drawPreview(this);
        }

        void mousePressEvent(QMouseEvent *event) override {
            dlg_->onPreviewPress(this, event->pos(), event->button());
            QWidget::mousePressEvent(event);
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            dlg_->onPreviewMove(this, event->pos());
            QWidget::mouseMoveEvent(event);
        }

        void mouseReleaseEvent(QMouseEvent *event) override {
            dlg_->onPreviewRelease(this, event->pos(), event->button());
            QWidget::mouseReleaseEvent(event);
        }

    private:
        ImageToolsDialog *dlg_;
    };

public:
    static void open(DeviceInfo *deviceInfo, QWidget *parent) {
        if (!deviceInfo)
            return;
        if (deviceInfo->localIp.isEmpty()) {
            new ToastWidget(
                QStringLiteral("当前连接没有可用的局域网 IP，无法访问设备上的 HTTP 接口。"
                               "请改用同一局域网的 Wi-Fi 连接后再试。"),
                parent);
            return;
        }
        ImageToolsDialog dialog(deviceInfo, parent);
        dialog.resize(1080, 680);
        dialog.exec();
    }

private:
    explicit ImageToolsDialog(DeviceInfo *deviceInfo, QWidget *parent)
        : BaseDialog(QStringLiteral("图片工具"), parent, false), deviceInfo_(deviceInfo) {
        auto *root = contentLayout();
        root->setContentsMargins(14, 14, 14, 14);
        root->setSpacing(10);

        auto *toolbar = new QWidget(this);
        auto *toolbarLay = new QHBoxLayout(toolbar);
        toolbarLay->setContentsMargins(0, 0, 0, 0);
        toolbarLay->setSpacing(8);

        captureBtn_ = new QPushButton(QStringLiteral("截图"), toolbar);
        releaseAllBtn_ = new QPushButton(QStringLiteral("释放全部"), toolbar);
        for (auto *btn : {captureBtn_, releaseAllBtn_}) {
            btn->setAutoDefault(false);
            btn->setDefault(false);
        }

        modeBtn_ = new QToolButton(toolbar);
        modeBtn_->setObjectName(QStringLiteral("ImageToolModeBtn"));
        modeBtn_->setPopupMode(QToolButton::InstantPopup);
        modeBtn_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        modeBtn_->setLayoutDirection(Qt::RightToLeft);
        modeBtn_->setIconSize(QSize(12, 12));
        modeBtn_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

        auto *modeMenu = new QMenu(modeBtn_);
        const struct {
            QString label;
            ToolMode mode;
        } modeItems[] = {{QStringLiteral("取色"), ToolMode::PickPixel},
                           {QStringLiteral("选区"), ToolMode::SelectRegion},
                           {QStringLiteral("找色"), ToolMode::FindColor},
                           {QStringLiteral("找图"), ToolMode::FindImage}};
        for (const auto &item : modeItems) {
            auto *action = modeMenu->addAction(item.label);
            action->setData(static_cast<int>(item.mode));
        }
        modeBtn_->setMenu(modeMenu);
        modeBtn_->setText(modeItems[0].label);
        currentMode_ = ToolMode::PickPixel;

        toolbarLay->addWidget(captureBtn_);
        toolbarLay->addWidget(releaseAllBtn_);
        toolbarLay->addSpacing(8);
        toolbarLay->addWidget(new QLabel(QStringLiteral("模式:"), toolbar));
        toolbarLay->addWidget(modeBtn_);
        toolbarLay->addStretch();
        root->addWidget(toolbar);

        split_ = new QSplitter(Qt::Horizontal, this);
        preview_ = new PreviewWidget(this);

        auto *sideScroll = new QScrollArea(split_);
        sideScroll->setWidgetResizable(true);
        sideScroll->setFrameShape(QFrame::NoFrame);
        sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        sidePanel_ = new QWidget(sideScroll);
        auto *sideLay = new QVBoxLayout(sidePanel_);
        sideLay->setContentsMargins(0, 0, 0, 0);
        sideLay->setSpacing(12);

        imageInfoLabel_ = new QLabel(QStringLiteral("尚未加载图片"), sidePanel_);
        imageInfoLabel_->setWordWrap(true);
        activeCountLabel_ = new QLabel(sidePanel_);
        activeCountLabel_->setWordWrap(true);

        buildPixelPanel(sideLay);
        buildRegionPanel(sideLay);
        buildFindColorPanel(sideLay);
        buildFindImagePanel(sideLay);

        sideLay->addStretch();
        sideScroll->setWidget(sidePanel_);
        sidePanel_->setMinimumWidth(280);
        sidePanel_->setMaximumWidth(360);

        split_->addWidget(preview_);
        split_->addWidget(sideScroll);
        split_->setStretchFactor(0, 3);
        split_->setStretchFactor(1, 1);
        split_->setSizes({720, 320});
        root->addWidget(split_, 1);

        statusLabel_ = new QLabel(QStringLiteral("正在创建会话…"), this);
        statusLabel_->setWordWrap(true);
        root->addWidget(statusLabel_);

        applyChrome();

        connect(captureBtn_, &QPushButton::clicked, this, &ImageToolsDialog::captureScreenshot);
        connect(releaseAllBtn_, &QPushButton::clicked, this, &ImageToolsDialog::releaseAllImages);
        connect(modeMenu, &QMenu::triggered, this, [this](QAction *action) {
            modeBtn_->setText(action->text());
            currentMode_ = static_cast<ToolMode>(action->data().toInt());
            syncModePanels();
        });
        connect(copyPixelHexBtn_, &QPushButton::clicked, this, &ImageToolsDialog::copyPixelHex);
        connect(copyPixelCoordBtn_, &QPushButton::clicked, this, &ImageToolsDialog::copyPixelCoord);
        connect(copyRegionBtn_, &QPushButton::clicked, this, &ImageToolsDialog::copyRegionLtrb);
        connect(cropBtn_, &QPushButton::clicked, this, &ImageToolsDialog::cropRegion);
        connect(findColorBtn_, &QPushButton::clicked, this, &ImageToolsDialog::findColor);
        connect(findImageBtn_, &QPushButton::clicked, this, &ImageToolsDialog::findImage);

        baseUrl_.setScheme(QStringLiteral("http"));
        baseUrl_.setHost(deviceInfo_->localIp);
        baseUrl_.setPort(65322);

        syncModePanels();
        autoCaptureAfterSession_ = true;
        startSession();
    }

    ~ImageToolsDialog() override { deleteSessionIfNeeded(); }

    void buildPixelPanel(QVBoxLayout *sideLay) {
        pixelPanel_ = new QFrame(sidePanel_);
        pixelPanel_->setObjectName(QStringLiteral("ImageToolPanel"));
        auto *lay = new QVBoxLayout(pixelPanel_);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
        lay->addWidget(new QLabel(QStringLiteral("点击图片读取像素"), pixelPanel_));
        pixelInfoLabel_ = new QLabel(QStringLiteral("—"), pixelPanel_);
        pixelInfoLabel_->setWordWrap(true);
        lay->addWidget(pixelInfoLabel_);
        auto *btnRow = new QHBoxLayout();
        copyPixelHexBtn_ = new QPushButton(QStringLiteral("复制 0xRRGGBB"), pixelPanel_);
        copyPixelCoordBtn_ = new QPushButton(QStringLiteral("复制坐标"), pixelPanel_);
        btnRow->addWidget(copyPixelHexBtn_);
        btnRow->addWidget(copyPixelCoordBtn_);
        lay->addLayout(btnRow);
        sideLay->addWidget(imageInfoLabel_);
        sideLay->addWidget(activeCountLabel_);
        sideLay->addWidget(pixelPanel_);
    }

    void buildRegionPanel(QVBoxLayout *sideLay) {
        regionPanel_ = new QFrame(sidePanel_);
        regionPanel_->setObjectName(QStringLiteral("ImageToolPanel"));
        auto *lay = new QVBoxLayout(regionPanel_);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
        lay->addWidget(new QLabel(QStringLiteral("拖拽框选区域（LTRB）"), regionPanel_));
        auto *form = new QFormLayout();
        leftSpin_ = makeCoordSpin(regionPanel_);
        topSpin_ = makeCoordSpin(regionPanel_);
        rightSpin_ = makeCoordSpin(regionPanel_);
        bottomSpin_ = makeCoordSpin(regionPanel_);
        form->addRow(QStringLiteral("left"), leftSpin_);
        form->addRow(QStringLiteral("top"), topSpin_);
        form->addRow(QStringLiteral("right"), rightSpin_);
        form->addRow(QStringLiteral("bottom"), bottomSpin_);
        lay->addLayout(form);
        auto *btnRow = new QHBoxLayout();
        copyRegionBtn_ = new QPushButton(QStringLiteral("复制 LTRB"), regionPanel_);
        cropBtn_ = new QPushButton(QStringLiteral("裁剪"), regionPanel_);
        btnRow->addWidget(copyRegionBtn_);
        btnRow->addWidget(cropBtn_);
        lay->addLayout(btnRow);
        sideLay->addWidget(regionPanel_);
    }

    void buildFindColorPanel(QVBoxLayout *sideLay) {
        findColorPanel_ = new QFrame(sidePanel_);
        findColorPanel_->setObjectName(QStringLiteral("ImageToolPanel"));
        auto *lay = new QVBoxLayout(findColorPanel_);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
        lay->addWidget(new QLabel(QStringLiteral("查找第一个匹配颜色"), findColorPanel_));
        auto *form = new QFormLayout();
        colorEdit_ = new QLineEdit(findColorPanel_);
        colorEdit_->setPlaceholderText(QStringLiteral("#FF0000 或 0xFF0000"));
        colorEdit_->setText(QStringLiteral("#FF0000"));
        toleranceSpin_ = new QSpinBox(findColorPanel_);
        toleranceSpin_->setRange(0, 255);
        toleranceSpin_->setValue(10);
        form->addRow(QStringLiteral("颜色"), colorEdit_);
        form->addRow(QStringLiteral("容差"), toleranceSpin_);
        lay->addLayout(form);
        findColorBtn_ = new QPushButton(QStringLiteral("查找"), findColorPanel_);
        lay->addWidget(findColorBtn_);
        findColorResultLabel_ = new QLabel(QStringLiteral("—"), findColorPanel_);
        findColorResultLabel_->setWordWrap(true);
        lay->addWidget(findColorResultLabel_);
        sideLay->addWidget(findColorPanel_);
    }

    void buildFindImagePanel(QVBoxLayout *sideLay) {
        findImagePanel_ = new QFrame(sidePanel_);
        findImagePanel_->setObjectName(QStringLiteral("ImageToolPanel"));
        auto *lay = new QVBoxLayout(findImagePanel_);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
        lay->addWidget(new QLabel(QStringLiteral("模板找图（手机本地路径）"), findImagePanel_));
        auto *form = new QFormLayout();
        templatePathEdit_ = new QLineEdit(findImagePanel_);
        templatePathEdit_->setPlaceholderText(QStringLiteral("/tmp/button.png"));
        thresholdSpin_ = new QDoubleSpinBox(findImagePanel_);
        thresholdSpin_->setRange(0.0, 1.0);
        thresholdSpin_->setSingleStep(0.05);
        thresholdSpin_->setDecimals(2);
        thresholdSpin_->setValue(0.9);
        form->addRow(QStringLiteral("模板路径"), templatePathEdit_);
        form->addRow(QStringLiteral("阈值"), thresholdSpin_);
        lay->addLayout(form);
        findImageBtn_ = new QPushButton(QStringLiteral("查找"), findImagePanel_);
        lay->addWidget(findImageBtn_);
        findImageResultLabel_ = new QLabel(QStringLiteral("—"), findImagePanel_);
        findImageResultLabel_->setWordWrap(true);
        lay->addWidget(findImageResultLabel_);
        sideLay->addWidget(findImagePanel_);
    }

    [[nodiscard]] static QSpinBox *makeCoordSpin(QWidget *parent) {
        auto *spin = new QSpinBox(parent);
        spin->setRange(0, 100000);
        spin->setReadOnly(true);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        return spin;
    }

    [[nodiscard]] HttpUtil::Sender net() {
        return HttpUtil::Sender{networkAccessManager, this};
    }

    [[nodiscard]] HttpUtil::Request api(QLatin1String route) {
        return HttpUtil::Request::relative(baseUrl_, route);
    }

    [[nodiscard]] HttpUtil::Request apiSes(QLatin1String route) {
        return api(route).ykSession(sessionId_);
    }

    void applyChrome() {
        const bool dark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        const QString border = dark ? QStringLiteral("#3A3F4B") : QStringLiteral("#DCE1EB");
        const QString inputBg = dark ? QStringLiteral("#141820") : QStringLiteral("#FAFCFF");
        const QString text = dark ? QStringLiteral("#E8ECF1") : QStringLiteral("#1F2937");
        setStyleSheet(QStringLiteral(
            "QFrame#ImageToolPanel { border: 1px solid %1; border-radius: 8px; padding: 8px; }"
            "QLineEdit, QSpinBox, QDoubleSpinBox {"
            "  background-color: %2; color: %3; border: 1px solid %1; border-radius: 6px; padding: 4px 8px;"
            "}"
            "QToolButton#ImageToolModeBtn {"
            "  background-color: %2; color: %3; border: 1px solid %1; border-radius: 6px;"
            "  padding: 4px 8px; spacing: 4px;"
            "}"
            "QToolButton#ImageToolModeBtn::menu-indicator { image: none; width: 0px; }")
                          .arg(border, inputBg, text));
        if (modeBtn_)
            modeBtn_->setIcon(modeMenuArrowIcon(dark));
    }

    [[nodiscard]] static QIcon modeMenuArrowIcon(bool dark) {
        const QIcon defaultIcon = qApp->style()->standardIcon(QStyle::SP_ArrowDown);
        QPixmap pixmap = defaultIcon.pixmap(16, 16);
        if (pixmap.isNull())
            return defaultIcon;
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), dark ? QColor(QStringLiteral("#E8ECF1")) : QColor(QStringLiteral("#1F2937")));
        painter.end();
        return QIcon(pixmap);
    }

    void syncModePanels() {
        const ToolMode mode = currentMode();
        pixelPanel_->setVisible(mode == ToolMode::PickPixel);
        regionPanel_->setVisible(mode == ToolMode::SelectRegion);
        findColorPanel_->setVisible(mode == ToolMode::FindColor);
        findImagePanel_->setVisible(mode == ToolMode::FindImage);
        if (preview_)
            preview_->setCursor(mode == ToolMode::SelectRegion ? Qt::CrossCursor : Qt::PointingHandCursor);
        if (preview_)
            preview_->update();
    }

    [[nodiscard]] ToolMode currentMode() const { return currentMode_; }

    void setBusy(const QString &text) {
        busy_ = true;
        statusLabel_->setText(text);
        captureBtn_->setEnabled(false);
        releaseAllBtn_->setEnabled(false);
    }

    void setIdle(const QString &text) {
        busy_ = false;
        statusLabel_->setText(text);
        captureBtn_->setEnabled(true);
        releaseAllBtn_->setEnabled(!sessionId_.isEmpty());
    }

    void deleteSessionIfNeeded() {
        abortReplies();
        if (!networkAccessManager)
            return;
        const QString sid = sessionId_;
        sessionId_.clear();
        currentImageId_.clear();
        if (sid.isEmpty())
            return;
        net().fire(api(QLatin1String("/session")).ykSession(sid).del().timeout(15000));
    }

    void abortReplies() {
        for (QNetworkReply **slot : {&pendingReply_, &captureReply_, &downloadReply_}) {
            if (*slot) {
                (*slot)->abort();
                (*slot)->deleteLater();
                *slot = nullptr;
            }
        }
    }

    void startSession() {
        abortReplies();
        deleteSessionIfNeeded();
        screenshot_ = QPixmap();
        if (preview_)
            preview_->update();
        setBusy(QStringLiteral("正在创建会话…"));
        if (!networkAccessManager) {
            setIdle(QStringLiteral("网络模块未初始化"));
            return;
        }
        pendingReply_ = net().submit(
            api(QLatin1String("/session/create")).postJson(QByteArrayLiteral(R"({})")).timeout(30000),
            [this](const HttpUtil::Result &r) {
                if (pendingReply_ == r.reply)
                    pendingReply_ = nullptr;
                if (r.canceled()) {
                    setIdle(QStringLiteral("创建会话已取消，可点击「截图」重试"));
                    pendingCaptureAfterSession_ = false;
                    return;
                }
                if (!r.ok()) {
                    setIdle(QStringLiteral("创建会话失败：%1").arg(r.errorString));
                    pendingCaptureAfterSession_ = false;
                    return;
                }
                sessionId_ = QJsonDocument::fromJson(r.bytes)
                                   .object()
                                   .value(QStringLiteral("value"))
                                   .toObject()
                                   .value(QStringLiteral("sessionId"))
                                   .toString();
                if (sessionId_.isEmpty()) {
                    setIdle(QStringLiteral("创建会话失败：响应中无 sessionId"));
                    pendingCaptureAfterSession_ = false;
                    return;
                }
                refreshActiveCount();
                if (pendingCaptureAfterSession_ || autoCaptureAfterSession_) {
                    pendingCaptureAfterSession_ = false;
                    autoCaptureAfterSession_ = false;
                    busy_ = false;
                    captureScreenshot();
                    return;
                }
                setIdle(QStringLiteral("会话已就绪，点击「截图」加载屏幕画面"));
            });
    }

    void captureScreenshot() {
        if (busy_)
            return;
        if (sessionId_.isEmpty()) {
            pendingCaptureAfterSession_ = true;
            startSession();
            return;
        }
        abortReplies();
        setBusy(QStringLiteral("正在截图…"));
        captureReply_ = net().submit(
            apiSes(QLatin1String("/image/capture")).postJson(QByteArrayLiteral(R"({})")).timeout(60000),
            [this](const HttpUtil::Result &r) {
                if (captureReply_ == r.reply)
                    captureReply_ = nullptr;
                if (r.canceled()) {
                    setIdle(QStringLiteral("截图已取消，可点击「截图」重试"));
                    return;
                }
                if (!r.ok()) {
                    setIdle(QStringLiteral("截图失败：%1").arg(r.errorString));
                    return;
                }
                const QJsonObject val =
                    QJsonDocument::fromJson(r.bytes).object().value(QStringLiteral("value")).toObject();
                if (val.contains(QStringLiteral("error"))) {
                    setIdle(val.value(QStringLiteral("message")).toString());
                    return;
                }
                const QString imageId = val.value(QStringLiteral("imageId")).toString();
                if (imageId.isEmpty()) {
                    setIdle(QStringLiteral("截图失败：无 imageId"));
                    return;
                }
                if (!currentImageId_.isEmpty() && currentImageId_ != imageId)
                    releaseImageId(currentImageId_);
                currentImageId_ = imageId;
                currentMeta_ = val;
                updateImageInfoLabel();
                downloadCurrentImage();
            });
    }

    void downloadCurrentImage() {
        if (currentImageId_.isEmpty() || sessionId_.isEmpty())
            return;
        QUrl url(baseUrl_);
        url.setPath(QStringLiteral("/image/download"));
        url.setQuery(QStringLiteral("format=jpg&quality=0.9&imageId=")
                     + QString::fromLatin1(QUrl::toPercentEncoding(currentImageId_)));
        downloadReply_ = net().submit(
            HttpUtil::Request::absolute(url).get().ykSession(sessionId_).timeout(120000),
            [this](const HttpUtil::Result &r) {
                if (downloadReply_ == r.reply)
                    downloadReply_ = nullptr;
                if (r.canceled()) {
                    setIdle(QStringLiteral("图片下载已取消，可点击「截图」重试"));
                    return;
                }
                screenshot_ = QPixmap();
                if (r.ok() && !r.bytes.isEmpty()) {
                    if (!screenshot_.loadFromData(r.bytes, "JPEG"))
                        screenshot_.loadFromData(r.bytes);
                }
                selection_ = {};
                findColorPoint_ = {};
                findImageRect_ = {};
                if (preview_)
                    preview_->update();
                refreshActiveCount();
                if (screenshot_.isNull())
                    setIdle(QStringLiteral("截图已返回，但图片解码失败"));
                else
                    setIdle(QStringLiteral("截图就绪，可在预览区操作"));
            });
    }

    void refreshActiveCount() {
        if (sessionId_.isEmpty())
            return;
        net().submit(apiSes(QLatin1String("/image/activeCount")).get().timeout(15000),
                     [this](const HttpUtil::Result &r) {
                         if (!r.ok())
                             return;
                         const int count = QJsonDocument::fromJson(r.bytes)
                                               .object()
                                               .value(QStringLiteral("value"))
                                               .toInt();
                         activeCountLabel_->setText(QStringLiteral("当前缓存图片：%1").arg(count));
                     });
    }

    void releaseImageId(const QString &imageId) {
        if (imageId.isEmpty() || sessionId_.isEmpty())
            return;
        const QJsonObject body{{QStringLiteral("imageId"), imageId}};
        net().fire(apiSes(QLatin1String("/image/release"))
                       .postJson(QJsonDocument(body).toJson(QJsonDocument::Compact))
                       .timeout(15000));
    }

    void releaseAllImages() {
        if (sessionId_.isEmpty() || busy_)
            return;
        setBusy(QStringLiteral("正在释放全部图片…"));
        net().submit(apiSes(QLatin1String("/image/releaseAll")).postJson(QByteArrayLiteral(R"({})")).timeout(30000),
                     [this](const HttpUtil::Result &r) {
                         if (r.canceled())
                             return;
                         currentImageId_.clear();
                         currentMeta_ = {};
                         screenshot_ = QPixmap();
                         selection_ = {};
                         findColorPoint_ = {};
                         findImageRect_ = {};
                         if (preview_)
                             preview_->update();
                         updateImageInfoLabel();
                         refreshActiveCount();
                         setIdle(r.ok() ? QStringLiteral("已释放全部图片缓存")
                                        : QStringLiteral("释放失败：%1").arg(r.errorString));
                     });
    }

    void updateImageInfoLabel() {
        if (currentImageId_.isEmpty()) {
            imageInfoLabel_->setText(QStringLiteral("尚未加载图片"));
            return;
        }
        imageInfoLabel_->setText(
            QStringLiteral("imageId: %1\n尺寸: %2 × %3")
                .arg(currentImageId_)
                .arg(currentMeta_.value(QStringLiteral("width")).toInt())
                .arg(currentMeta_.value(QStringLiteral("height")).toInt()));
    }

    [[nodiscard]] static QRect fittedPixmapRect(const QSize &widgetSize, const QSize &pixmapSize) {
        if (pixmapSize.width() <= 0 || pixmapSize.height() <= 0)
            return QRect(QPoint(0, 0), widgetSize);
        const qreal wr = qreal(widgetSize.width()) / qreal(pixmapSize.width());
        const qreal hr = qreal(widgetSize.height()) / qreal(pixmapSize.height());
        const qreal ratio = qMin(wr, hr);
        const int w = qMax(1, int(qRound(qreal(pixmapSize.width()) * ratio)));
        const int h = qMax(1, int(qRound(qreal(pixmapSize.height()) * ratio)));
        return QRect((widgetSize.width() - w) / 2, (widgetSize.height() - h) / 2, w, h);
    }

    [[nodiscard]] bool previewLocalToScreen(const PreviewWidget *w, const QPoint &local, int &sx, int &sy) const {
        if (!w || screenshot_.isNull())
            return false;
        const QRect fitted = fittedPixmapRect(w->size(), screenshot_.size());
        if (!fitted.contains(local))
            return false;
        sx = int(qFloor((local.x() - fitted.x()) * double(screenshot_.width()) / double(fitted.width())));
        sy = int(qFloor((local.y() - fitted.y()) * double(screenshot_.height()) / double(fitted.height())));
        sx = qBound(0, sx, screenshot_.width() - 1);
        sy = qBound(0, sy, screenshot_.height() - 1);
        return true;
    }

    [[nodiscard]] QRect screenRectToPreview(const PreviewWidget *w, const QRect &screenRect) const {
        if (!w || screenshot_.isNull() || screenRect.isEmpty())
            return {};
        const QRect fitted = fittedPixmapRect(w->size(), screenshot_.size());
        const auto mapX = [&](int x) {
            return fitted.x() + int(qRound(x * double(fitted.width()) / double(screenshot_.width())));
        };
        const auto mapY = [&](int y) {
            return fitted.y() + int(qRound(y * double(fitted.height()) / double(screenshot_.height())));
        };
        return QRect(mapX(screenRect.x()), mapY(screenRect.y()),
            qMax(1, mapX(screenRect.right()) - mapX(screenRect.x())),
            qMax(1, mapY(screenRect.bottom()) - mapY(screenRect.y())));
    }

    void onPreviewPress(PreviewWidget *w, const QPoint &local, Qt::MouseButton button) {
        if (button != Qt::LeftButton || screenshot_.isNull())
            return;
        if (currentMode() == ToolMode::SelectRegion) {
            int sx = 0, sy = 0;
            if (!previewLocalToScreen(w, local, sx, sy))
                return;
            dragStart_ = QPoint(sx, sy);
            selection_ = QRect(sx, sy, 1, 1);
            dragging_ = true;
            updateRegionSpins();
            preview_->update();
            return;
        }
        if (currentMode() == ToolMode::PickPixel)
            requestPixelAt(w, local);
    }

    void onPreviewMove(PreviewWidget *w, const QPoint &local) {
        if (!dragging_ || currentMode() != ToolMode::SelectRegion)
            return;
        int sx = 0, sy = 0;
        if (!previewLocalToScreen(w, local, sx, sy))
            return;
        selection_ = normalizedRect(dragStart_, QPoint(sx, sy));
        updateRegionSpins();
        preview_->update();
    }

    void onPreviewRelease(PreviewWidget *w, const QPoint &local, Qt::MouseButton button) {
        Q_UNUSED(w);
        if (button != Qt::LeftButton || !dragging_)
            return;
        dragging_ = false;
        int sx = 0, sy = 0;
        if (previewLocalToScreen(w, local, sx, sy))
            selection_ = normalizedRect(dragStart_, QPoint(sx, sy));
        updateRegionSpins();
        preview_->update();
    }

    [[nodiscard]] static QRect normalizedRect(const QPoint &a, const QPoint &b) {
        return QRect(qMin(a.x(), b.x()), qMin(a.y(), b.y()), qAbs(a.x() - b.x()) + 1, qAbs(a.y() - b.y()) + 1);
    }

    void updateRegionSpins() {
        if (selection_.isEmpty())
            return;
        leftSpin_->setValue(selection_.left());
        topSpin_->setValue(selection_.top());
        rightSpin_->setValue(selection_.right());
        bottomSpin_->setValue(selection_.bottom());
    }

    void requestPixelAt(PreviewWidget *w, const QPoint &local) {
        if (currentImageId_.isEmpty() || sessionId_.isEmpty())
            return;
        int sx = 0, sy = 0;
        if (!previewLocalToScreen(w, local, sx, sy))
            return;
        const QJsonObject body{
            {QStringLiteral("imageId"), currentImageId_},
            {QStringLiteral("x"), sx},
            {QStringLiteral("y"), sy},
        };
        net().submit(
            apiSes(QLatin1String("/image/getPixel"))
                .postJson(QJsonDocument(body).toJson(QJsonDocument::Compact))
                .timeout(30000),
            [this, sx, sy](const HttpUtil::Result &r) {
                if (!r.ok()) {
                    pixelInfoLabel_->setText(QStringLiteral("读取失败：%1").arg(r.errorString));
                    return;
                }
                const QJsonValue val = QJsonDocument::fromJson(r.bytes).object().value(QStringLiteral("value"));
                if (val.isNull()) {
                    pixelInfoLabel_->setText(QStringLiteral("坐标 (%1, %2) 越界").arg(sx).arg(sy));
                    lastPixelHex_ = 0;
                    return;
                }
                const QJsonObject o = val.toObject();
                const int rgb = o.value(QStringLiteral("rgb")).toInt();
                lastPixelHex_ = rgb;
                pixelInfoLabel_->setText(
                    QStringLiteral("(%1, %2)\nR=%3 G=%4 B=%5\n0x%6")
                        .arg(o.value(QStringLiteral("x")).toInt())
                        .arg(o.value(QStringLiteral("y")).toInt())
                        .arg(o.value(QStringLiteral("r")).toInt())
                        .arg(o.value(QStringLiteral("g")).toInt())
                        .arg(o.value(QStringLiteral("b")).toInt())
                        .arg(rgb, 6, 16, QChar('0')));
            });
    }

    void copyPixelHex() {
        if (lastPixelHex_ <= 0)
            return;
        qApp->clipboard()->setText(QStringLiteral("0x%1").arg(lastPixelHex_, 6, 16, QChar('0')));
        new ToastWidget(QStringLiteral("已复制颜色"), this);
    }

    void copyPixelCoord() {
        const QString text = pixelInfoLabel_->text();
        const QRegularExpression re(QStringLiteral(R"(\((\d+),\s*(\d+)\))"));
        const QRegularExpressionMatch m = re.match(text);
        if (!m.hasMatch())
            return;
        qApp->clipboard()->setText(QStringLiteral("%1,%2").arg(m.captured(1), m.captured(2)));
        new ToastWidget(QStringLiteral("已复制坐标"), this);
    }

    void copyRegionLtrb() {
        if (selection_.isEmpty())
            return;
        qApp->clipboard()->setText(
            QStringLiteral(R"({"left":%1,"top":%2,"right":%3,"bottom":%4})")
                .arg(selection_.left())
                .arg(selection_.top())
                .arg(selection_.right())
                .arg(selection_.bottom()));
        new ToastWidget(QStringLiteral("已复制 LTRB"), this);
    }

    [[nodiscard]] static std::optional<uint> parseColorInput(const QString &text) {
        QString s = text.trimmed();
        if (s.isEmpty())
            return std::nullopt;
        if (s.startsWith('#'))
            s = s.mid(1);
        if (s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
            s = s.mid(2);
        bool ok = false;
        const uint rgb = s.toUInt(&ok, 16);
        if (!ok || s.length() > 6)
            return std::nullopt;
        return rgb & 0xFFFFFFu;
    }

    void findColor() {
        if (currentImageId_.isEmpty() || sessionId_.isEmpty() || busy_)
            return;
        const auto color = parseColorInput(colorEdit_->text());
        if (!color) {
            new ToastWidget(QStringLiteral("颜色格式无效"), this);
            return;
        }
        QJsonObject body{
            {QStringLiteral("imageId"), currentImageId_},
            {QStringLiteral("color"), int(*color)},
            {QStringLiteral("tolerance"), toleranceSpin_->value()},
        };
        if (!selection_.isEmpty()) {
            body.insert(QStringLiteral("options"),
                QJsonObject{{QStringLiteral("region"),
                    QJsonObject{{QStringLiteral("left"), selection_.left()},
                        {QStringLiteral("top"), selection_.top()},
                        {QStringLiteral("right"), selection_.right()},
                        {QStringLiteral("bottom"), selection_.bottom()}}}});
        }
        setBusy(QStringLiteral("正在找色…"));
        net().submit(
            apiSes(QLatin1String("/image/opencvFindColor"))
                .postJson(QJsonDocument(body).toJson(QJsonDocument::Compact))
                .timeout(60000),
            [this](const HttpUtil::Result &r) {
                if (r.canceled())
                    return;
                if (!r.ok()) {
                    setIdle(QStringLiteral("找色失败：%1").arg(r.errorString));
                    return;
                }
                const QJsonValue val = QJsonDocument::fromJson(r.bytes).object().value(QStringLiteral("value"));
                if (val.isNull()) {
                    findColorPoint_ = {};
                    findColorResultLabel_->setText(QStringLiteral("未找到匹配颜色"));
                    setIdle(QStringLiteral("找色完成：未命中"));
                } else {
                    const QJsonObject o = val.toObject();
                    findColorPoint_ = QPoint(o.value(QStringLiteral("x")).toInt(), o.value(QStringLiteral("y")).toInt());
                    findColorResultLabel_->setText(
                        QStringLiteral("命中 (%1, %2)  0x%3")
                            .arg(findColorPoint_.x())
                            .arg(findColorPoint_.y())
                            .arg(o.value(QStringLiteral("rgb")).toInt(), 6, 16, QChar('0')));
                    setIdle(QStringLiteral("找色完成"));
                }
                if (preview_)
                    preview_->update();
            });
    }

    void findImage() {
        if (currentImageId_.isEmpty() || sessionId_.isEmpty() || busy_)
            return;
        const QString path = templatePathEdit_->text().trimmed();
        if (path.isEmpty()) {
            new ToastWidget(QStringLiteral("请填写模板路径"), this);
            return;
        }
        QJsonObject body{
            {QStringLiteral("imageId"), currentImageId_},
            {QStringLiteral("templatePath"), path},
            {QStringLiteral("threshold"), thresholdSpin_->value()},
        };
        if (!selection_.isEmpty()) {
            body.insert(QStringLiteral("options"),
                QJsonObject{{QStringLiteral("region"),
                    QJsonObject{{QStringLiteral("left"), selection_.left()},
                        {QStringLiteral("top"), selection_.top()},
                        {QStringLiteral("right"), selection_.right()},
                        {QStringLiteral("bottom"), selection_.bottom()}}}});
        }
        setBusy(QStringLiteral("正在找图…"));
        net().submit(
            apiSes(QLatin1String("/image/opencvFindImage"))
                .postJson(QJsonDocument(body).toJson(QJsonDocument::Compact))
                .timeout(120000),
            [this](const HttpUtil::Result &r) {
                if (r.canceled())
                    return;
                if (!r.ok()) {
                    setIdle(QStringLiteral("找图失败：%1").arg(r.errorString));
                    return;
                }
                const QJsonValue val = QJsonDocument::fromJson(r.bytes).object().value(QStringLiteral("value"));
                if (val.isNull()) {
                    findImageRect_ = {};
                    findImageResultLabel_->setText(QStringLiteral("未找到模板"));
                    setIdle(QStringLiteral("找图完成：未命中"));
                } else {
                    const QJsonObject o = val.toObject();
                    findImageRect_ = QRect(o.value(QStringLiteral("left")).toInt(), o.value(QStringLiteral("top")).toInt(),
                        o.value(QStringLiteral("width")).toInt(), o.value(QStringLiteral("height")).toInt());
                    findImageResultLabel_->setText(
                        QStringLiteral("命中 (%1,%2) %3×%4  score=%5")
                            .arg(findImageRect_.x())
                            .arg(findImageRect_.y())
                            .arg(findImageRect_.width())
                            .arg(findImageRect_.height())
                            .arg(o.value(QStringLiteral("score")).toDouble(), 0, 'f', 3));
                    setIdle(QStringLiteral("找图完成"));
                }
                if (preview_)
                    preview_->update();
            });
    }

    void cropRegion() {
        if (currentImageId_.isEmpty() || sessionId_.isEmpty() || selection_.isEmpty() || busy_)
            return;
        QJsonObject body{
            {QStringLiteral("imageId"), currentImageId_},
            {QStringLiteral("left"), selection_.left()},
            {QStringLiteral("top"), selection_.top()},
            {QStringLiteral("right"), selection_.right()},
            {QStringLiteral("bottom"), selection_.bottom()},
        };
        setBusy(QStringLiteral("正在裁剪…"));
        net().submit(
            apiSes(QLatin1String("/image/crop")).postJson(QJsonDocument(body).toJson(QJsonDocument::Compact)).timeout(60000),
            [this](const HttpUtil::Result &r) {
                if (r.canceled())
                    return;
                if (!r.ok()) {
                    setIdle(QStringLiteral("裁剪失败：%1").arg(r.errorString));
                    return;
                }
                const QJsonObject val =
                    QJsonDocument::fromJson(r.bytes).object().value(QStringLiteral("value")).toObject();
                if (val.contains(QStringLiteral("error"))) {
                    setIdle(val.value(QStringLiteral("message")).toString());
                    return;
                }
                const QString oldId = currentImageId_;
                currentImageId_ = val.value(QStringLiteral("imageId")).toString();
                currentMeta_ = val;
                if (!oldId.isEmpty() && oldId != currentImageId_)
                    releaseImageId(oldId);
                selection_ = {};
                findColorPoint_ = {};
                findImageRect_ = {};
                updateImageInfoLabel();
                downloadCurrentImage();
            });
    }

    void drawPreview(PreviewWidget *w) {
        QPainter p(w);
        p.fillRect(w->rect(), Qt::black);
        if (screenshot_.isNull()) {
            p.setPen(QColor(180, 180, 185));
            p.drawText(w->rect(), Qt::AlignCenter, QStringLiteral("点击「截图」加载屏幕画面"));
            return;
        }
        const QRect fitted = fittedPixmapRect(w->size(), screenshot_.size());
        p.drawPixmap(fitted, screenshot_, screenshot_.rect());

        if (!selection_.isEmpty()) {
            const QRect r = screenRectToPreview(w, selection_);
            p.setPen(QPen(QColor(255, 214, 10), 2));
            p.setBrush(QColor(255, 214, 10, 40));
            p.drawRect(r);
        }

        if (findImageRect_.isValid() && !findImageRect_.isEmpty()) {
            const QRect r = screenRectToPreview(w, findImageRect_);
            p.setPen(QPen(QColor(60, 220, 120), 2));
            p.setBrush(Qt::NoBrush);
            p.drawRect(r);
        }

        if (!findColorPoint_.isNull()) {
            const QRect fittedRect = fittedPixmapRect(w->size(), screenshot_.size());
            const int px = fittedRect.x()
                + int(qRound(findColorPoint_.x() * double(fittedRect.width()) / double(screenshot_.width())));
            const int py = fittedRect.y()
                + int(qRound(findColorPoint_.y() * double(fittedRect.height()) / double(screenshot_.height())));
            p.setPen(QPen(Qt::red, 2));
            p.drawLine(px - 8, py, px + 8, py);
            p.drawLine(px, py - 8, px, py + 8);
        }
    }

    DeviceInfo *deviceInfo_ = nullptr;
    QUrl baseUrl_;
    QString sessionId_;
    QString currentImageId_;
    QJsonObject currentMeta_;
    QPixmap screenshot_;
    QRect selection_;
    QPoint dragStart_;
    QPoint findColorPoint_;
    QRect findImageRect_;
    int lastPixelHex_ = 0;
    bool dragging_ = false;
    bool busy_ = false;
    bool pendingCaptureAfterSession_ = false;
    bool autoCaptureAfterSession_ = false;

    QNetworkReply *pendingReply_ = nullptr;
    QNetworkReply *captureReply_ = nullptr;
    QNetworkReply *downloadReply_ = nullptr;

    ToolMode currentMode_ = ToolMode::PickPixel;
    QSplitter *split_ = nullptr;
    PreviewWidget *preview_ = nullptr;
    QWidget *sidePanel_ = nullptr;
    QToolButton *modeBtn_ = nullptr;
    QPushButton *captureBtn_ = nullptr;
    QPushButton *releaseAllBtn_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QLabel *imageInfoLabel_ = nullptr;
    QLabel *activeCountLabel_ = nullptr;

    QFrame *pixelPanel_ = nullptr;
    QLabel *pixelInfoLabel_ = nullptr;
    QPushButton *copyPixelHexBtn_ = nullptr;
    QPushButton *copyPixelCoordBtn_ = nullptr;

    QFrame *regionPanel_ = nullptr;
    QSpinBox *leftSpin_ = nullptr;
    QSpinBox *topSpin_ = nullptr;
    QSpinBox *rightSpin_ = nullptr;
    QSpinBox *bottomSpin_ = nullptr;
    QPushButton *copyRegionBtn_ = nullptr;
    QPushButton *cropBtn_ = nullptr;

    QFrame *findColorPanel_ = nullptr;
    QLineEdit *colorEdit_ = nullptr;
    QSpinBox *toleranceSpin_ = nullptr;
    QPushButton *findColorBtn_ = nullptr;
    QLabel *findColorResultLabel_ = nullptr;

    QFrame *findImagePanel_ = nullptr;
    QLineEdit *templatePathEdit_ = nullptr;
    QDoubleSpinBox *thresholdSpin_ = nullptr;
    QPushButton *findImageBtn_ = nullptr;
    QLabel *findImageResultLabel_ = nullptr;
};
