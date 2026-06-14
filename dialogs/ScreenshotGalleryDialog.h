#pragma once

#include "BaseDialog.h"
#include "Tools.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QDateEdit>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QImageReader>
#include <QResizeEvent>
#include <QStyleHints>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

/** 统一卡片：序号为小号等宽数字贴在卡内左上（非胶囊、不压图），底部留白防裁字 */
class ScreenshotGalleryItemDelegate : public QStyledItemDelegate {
public:
    static constexpr int kInnerPad = 10;
    static constexpr int kIndexRowMin = 18;
    static constexpr int kGap = 5;
    static constexpr int kImgTextGap = 8;
    static constexpr int kBottomPad = 16;

    static int indexRowHeight(const QFont &bodyFont) {
        QFont f = bodyFont;
        f.setPointSize(9);
        f.setBold(true);
        return qMax(kIndexRowMin, QFontMetrics(f).height() + 6);
    }

    static QSize recommendedCellSize(const QFont &bodyFont, const QSize &iconSize) {
        QFontMetrics fm(bodyFont);
        const int lineH = fm.height() + 5;
        const int textBlock = lineH * 2 + 6;
        const int idxH = indexRowHeight(bodyFont);
        return QSize(iconSize.width() + 2 * kInnerPad + 16,
                     kInnerPad + idxH + kGap + iconSize.height() + kImgTextGap + textBlock + kBottomPad);
    }

    ScreenshotGalleryItemDelegate(QListWidget *list, bool dark, const QString &textMain, const QString &subtle,
                                  const QString &selBorderLight, const QString &hoverCell)
        : QStyledItemDelegate(list), list_(list), dark_(dark), textMain_(textMain), subtle_(subtle),
          selBorderLight_(selBorderLight), hoverCell_(hoverCell) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRect cell = option.rect;
        const QRect card = cell.adjusted(4, 4, -4, -4);
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hover = option.state.testFlag(QStyle::State_MouseOver);

        QPainterPath cardPath;
        cardPath.addRoundedRect(card, 12, 12);

        if (selected) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(74, 134, 247, dark_ ? 48 : 28));
            painter->drawPath(cardPath);
        } else if (hover) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(hoverCell_));
            painter->drawPath(cardPath);
        }

        const Theme::Palette &t = Theme::palette();
        painter->setPen(QPen(QColor(t.border), 1));
        painter->setBrush(QColor(t.surface));
        painter->drawPath(cardPath);

        const QRect inner = card.adjusted(kInnerPad, kInnerPad, -kInnerPad, -kInnerPad);
        const int order = index.data(Qt::UserRole + 1).toInt();
        const QSize isz = list_->iconSize();
        const int idxBlock = indexRowHeight(option.font);

        QFont idxFont = option.font;
        idxFont.setPointSize(9);
        idxFont.setBold(true);
        painter->setFont(idxFont);
        QFontMetrics idxFm(idxFont);
        if (order > 0) {
            const QString idxStr =
                order < 100 ? QStringLiteral("%1").arg(order, 2, 10, QLatin1Char('0')) : QString::number(order);
            painter->setPen(QColor(subtle_));
            painter->drawText(inner.left(), inner.top() + idxFm.ascent(), idxStr);
        }

        const int imgTop = inner.top() + idxBlock + kGap;
        QRect imgRect(inner.left() + (inner.width() - isz.width()) / 2, imgTop, isz.width(), isz.height());

        const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        const QPixmap pix = icon.pixmap(isz);
        if (!pix.isNull()) {
            QPainterPath imgClip;
            imgClip.addRoundedRect(imgRect, 8, 8);
            painter->setClipPath(imgClip);
            painter->fillRect(imgRect, QColor(12, 13, 16));
            painter->drawPixmap(imgRect.x() + (imgRect.width() - pix.width()) / 2,
                                imgRect.y() + (imgRect.height() - pix.height()) / 2, pix);
            painter->setClipping(false);
            painter->setPen(QPen(QColor(Theme::palette().divider), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(imgRect.adjusted(0, 0, -1, -1), 8, 8);
        }

        QFont tf = option.font;
        tf.setBold(false);
        tf.setPointSize(option.font.pointSize() > 0 ? option.font.pointSize() : 9);
        painter->setFont(tf);
        QFontMetrics fm(tf);
        const int lineH = fm.height() + 5;
        int ty = imgRect.bottom() + kImgTextGap;

        const QString rawText = index.data(Qt::DisplayRole).toString();
        const QStringList lines = rawText.split(QLatin1Char('\n'));
        if (!lines.isEmpty()) {
            painter->setPen(QColor(textMain_));
            const QString name = fm.elidedText(lines[0], Qt::ElideMiddle, inner.width());
            painter->drawText(QRect(inner.left(), ty, inner.width(), lineH), Qt::AlignHCenter | Qt::AlignTop, name);
            ty += lineH;
        }
        if (lines.size() >= 2) {
            painter->setPen(QColor(subtle_));
            painter->drawText(QRect(inner.left(), ty, inner.width(), lineH), Qt::AlignHCenter | Qt::AlignTop, lines[1]);
        }

        if (selected) {
            painter->setPen(QPen(QColor(selBorderLight_), 2));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(card.adjusted(0, 0, -1, -1), 12, 12);
        }

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        Q_UNUSED(index);
        return recommendedCellSize(option.font, list_->iconSize());
    }

private:
    QListWidget *list_;
    bool dark_;
    QString textMain_, subtle_, selBorderLight_, hoverCell_;
};

class ScreenshotGalleryDialog : public BaseDialog {
public:
    static void open(QWidget *parent, const QString &deviceId) {
        ScreenshotGalleryDialog dialog(parent, deviceId);
        dialog.resize(1020, 660);
        dialog.exec();
    }

private:
    explicit ScreenshotGalleryDialog(QWidget *parent, const QString &deviceId)
        : BaseDialog(QStringLiteral("截图库"), parent),
          galleryDir_(Tools::screenshotSaveDirectory(deviceId)) {
        const Theme::Palette &t = Theme::palette();
        dark_ = Theme::isDark();
        cardBg_ = t.surface;
        cardBorder_ = t.border;
        subtle_ = t.textMuted;
        textMain_ = t.textPrimary;
        mutedBg_ = t.surfaceAlt;
        accent_ = t.primary;
        selBorderLight_ = t.primaryPressed;

        buildUi();
        reloadGallery();
    }

    void buildUi() {
        auto *root = contentLayout();
        root->setSpacing(0);
        root->setContentsMargins(0, 0, 0, 0);

        // —— 单行工具区：统计 + 日期范围 + 按钮；路径单独一行（省高度） ——
        auto *toolbar = new QFrame(this);
        toolbar->setObjectName(QStringLiteral("sgToolbar"));
        toolbar->setStyleSheet(QStringLiteral(
            "#sgToolbar { background: %1; border: 1px solid %2; border-radius: 12px; }")
            .arg(cardBg_, cardBorder_));
        auto *tbOuter = new QVBoxLayout(toolbar);
        tbOuter->setContentsMargins(12, 10, 12, 10);
        tbOuter->setSpacing(8);

        auto *row1 = new QHBoxLayout();
        row1->setSpacing(10);

        countLabel_ = new QLabel(this);
        countLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(subtle_));
        row1->addWidget(countLabel_);

        auto *rangeLbl = new QLabel(QStringLiteral("日期"), this);
        rangeLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(subtle_));
        row1->addWidget(rangeLbl);

        auto *fromLbl = new QLabel(QStringLiteral("从"), this);
        fromLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(subtle_));
        row1->addWidget(fromLbl);

        dateFrom_ = new QDateEdit(QDate(2000, 1, 1), this);
        dateTo_ = new QDateEdit(QDate::currentDate(), this);
        for (auto *de : {dateFrom_, dateTo_}) {
            de->setCalendarPopup(true);
            de->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
            de->setMinimumHeight(30);
            styleDateEdit(de);
        }
        row1->addWidget(dateFrom_);

        auto *rangeDash = new QLabel(QStringLiteral("至"), this);
        rangeDash->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(subtle_));
        row1->addWidget(rangeDash);

        row1->addWidget(dateTo_);

        row1->addStretch();

        refreshBtn_ = new QPushButton(QStringLiteral("刷新"), this);
        folderBtn_ = new QPushButton(QStringLiteral("打开文件夹"), this);
        for (auto *b : {refreshBtn_, folderBtn_}) {
            b->setCursor(Qt::PointingHandCursor);
            b->setMinimumHeight(32);
            b->setMinimumWidth(88);
        }
        styleToolButton(refreshBtn_, false);
        styleToolButton(folderBtn_, true);
        row1->addWidget(refreshBtn_);
        row1->addWidget(folderBtn_);
        tbOuter->addLayout(row1);

        pathLabel_ = new QLabel(this);
        pathLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QFont mono = QFont(QStringLiteral("SF Mono"), -1);
        if (!mono.exactMatch())
            mono = QFont(QStringLiteral("Menlo"), -1);
        if (!mono.exactMatch())
            mono = QFont(QStringLiteral("Consolas"), 9);
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(9);
        pathLabel_->setFont(mono);
        pathLabel_->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; background: %2; border: 1px solid %3; border-radius: 8px; padding: 6px 8px; }")
            .arg(subtle_, mutedBg_, cardBorder_));
        tbOuter->addWidget(pathLabel_);

        root->addWidget(toolbar);

        // —— 主区：网格 + 预览 ——
        auto *mainRow = new QHBoxLayout();
        mainRow->setSpacing(12);
        mainRow->setContentsMargins(0, 12, 0, 0);

        auto *gridCard = new QFrame(this);
        gridCard->setObjectName(QStringLiteral("sgGrid"));
        gridCard->setStyleSheet(QStringLiteral(
            "#sgGrid { background: %1; border: 1px solid %2; border-radius: 12px; }")
            .arg(mutedBg_, cardBorder_));
        auto *gridOuter = new QVBoxLayout(gridCard);
        gridOuter->setContentsMargins(10, 10, 10, 10);

        gridTitleLabel_ = new QLabel(QStringLiteral("全部截图"), this);
        QFont gf = gridTitleLabel_->font();
        gf.setBold(true);
        gf.setPointSize(10);
        gridTitleLabel_->setFont(gf);
        gridTitleLabel_->setStyleSheet(QStringLiteral("color: %1; padding: 0 4px 6px;").arg(textMain_));
        gridOuter->addWidget(gridTitleLabel_);

        list_ = new QListWidget(this);
        list_->setViewMode(QListWidget::IconMode);
        list_->setMovement(QListWidget::Static);
        list_->setResizeMode(QListWidget::Adjust);
        list_->setSpacing(10);
        list_->setIconSize(QSize(108, 152));
        list_->setUniformItemSizes(true);
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        list_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        list_->setMinimumHeight(440);
        list_->setStyleSheet(QStringLiteral(
            "QListWidget { background: transparent; border: none; padding: 6px; }"
            "QListWidget::item { background: transparent; border: none; padding: 0px; margin: 4px; outline: none; }"));
        list_->setItemDelegate(new ScreenshotGalleryItemDelegate(
            list_, dark_, textMain_, subtle_, selBorderLight_,
            dark_ ? QStringLiteral("#2A2D35") : QStringLiteral("#F1F5F9")));
        gridOuter->addWidget(list_, 1);
        mainRow->addWidget(gridCard, 1);

        auto *previewCard = new QFrame(this);
        previewCard->setObjectName(QStringLiteral("sgPreview"));
        previewCard->setFixedWidth(392);
        previewCard->setStyleSheet(QStringLiteral(
            "#sgPreview { background: %1; border: 1px solid %2; border-radius: 12px; }")
            .arg(cardBg_, cardBorder_));
        auto *previewLay = new QVBoxLayout(previewCard);
        previewLay->setContentsMargins(14, 12, 14, 14);
        previewLay->setSpacing(10);

        auto *pvTitle = new QLabel(QStringLiteral("预览"), this);
        QFont pf = pvTitle->font();
        pf.setBold(true);
        pf.setPointSize(10);
        pvTitle->setFont(pf);
        pvTitle->setStyleSheet(QStringLiteral("color: %1;").arg(textMain_));
        previewLay->addWidget(pvTitle);

        preview_ = new QLabel(this);
        preview_->setAlignment(Qt::AlignCenter);
        preview_->setMinimumHeight(300);
        preview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        preview_->setScaledContents(false);
        setPreviewPlaceholder(QStringLiteral("在左侧点选缩略图"));
        previewLay->addWidget(preview_, 1);

        metaLabel_ = new QLabel(this);
        metaLabel_->setWordWrap(true);
        metaLabel_->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 12px; line-height: 1.5; }")
            .arg(subtle_));
        previewLay->addWidget(metaLabel_, 0);

        mainRow->addWidget(previewCard, 0);
        root->addLayout(mainRow, 1);

        connect(refreshBtn_, &QPushButton::clicked, this, &ScreenshotGalleryDialog::reloadGallery);
        connect(folderBtn_, &QPushButton::clicked, this, [this]() {
            QDir().mkpath(galleryDir_);
            QDesktopServices::openUrl(QUrl::fromLocalFile(galleryDir_));
        });
        connect(dateFrom_, &QDateEdit::dateChanged, this, &ScreenshotGalleryDialog::onDateRangeChanged);
        connect(dateTo_, &QDateEdit::dateChanged, this, &ScreenshotGalleryDialog::onDateRangeChanged);
        connect(list_, &QListWidget::currentItemChanged, this, &ScreenshotGalleryDialog::onSelectionChanged);
        connect(list_, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem *item) {
            if (!item)
                return;
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty())
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
    }

    void styleToolButton(QPushButton *b, bool primary) {
        if (primary) {
            b->setStyleSheet(QStringLiteral(
                "QPushButton { padding: 7px 16px; border-radius: 8px; border: none; background: %1; color: white; font-weight: 600; }"
                "QPushButton:hover { background: %2; }"
                "QPushButton:pressed { background: %3; }")
                .arg(accent_, Theme::primaryHover(), Theme::primaryPressed()));
        } else {
            b->setStyleSheet(QStringLiteral(
                "QPushButton { padding: 7px 16px; border-radius: 8px; border: 1px solid %1; background: %2; color: %3; }"
                "QPushButton:hover { border-color: %4; background: %5; }")
                .arg(cardBorder_, cardBg_, textMain_, accent_,
                     dark_ ? QStringLiteral("#252830") : QStringLiteral("#F1F5F9")));
        }
    }

    void styleDateEdit(QDateEdit *de) {
        de->setStyleSheet(QStringLiteral(
            "QDateEdit { padding: 5px 8px; border-radius: 8px; border: 1px solid %1; background: %2; color: %3; }"
            "QDateEdit:hover { border-color: %4; }")
            .arg(cardBorder_, Theme::palette().inputBg, textMain_, accent_));
    }

    void onDateRangeChanged() {
        updateGridTitle();
        repopulateList();
    }

    void updateGridTitle() {
        QDate df = dateFrom_->date();
        QDate dt = dateTo_->date();
        if (df > dt) {
            const QDate t = df;
            df = dt;
            dt = t;
        }
        gridTitleLabel_->setText(QStringLiteral("%1 — %2")
                                     .arg(df.toString(QStringLiteral("yyyy-MM-dd")),
                                          dt.toString(QStringLiteral("yyyy-MM-dd"))));
    }

    void reloadGallery() {
        QDir().mkpath(galleryDir_);
        pathLabel_->setText(elideMiddle(galleryDir_, pathLabel_->font(), qMax(200, width() - 48)));

        QDir dir(galleryDir_);
        QStringList filters{QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png"),
                            QStringLiteral("*.webp")};
        allFiles_ = dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::NoSort);
        std::sort(allFiles_.begin(), allFiles_.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.lastModified() > b.lastModified();
        });

        countLabel_->setText(QStringLiteral("共 %1 个文件").arg(allFiles_.size()));
        updateGridTitle();
        repopulateList();
    }

    bool passesTimeFilter(const QFileInfo &fi) const {
        const QDate d = fi.lastModified().date();
        QDate from = dateFrom_->date();
        QDate to = dateTo_->date();
        if (from > to) {
            const QDate tmp = from;
            from = to;
            to = tmp;
        }
        return d >= from && d <= to;
    }

    void setPreviewPlaceholder(const QString &text) {
        preview_->clear();
        preview_->setText(text);
        preview_->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; border: 1px dashed %2; border-radius: 10px; padding: 12px; color: %3; }")
            .arg(mutedBg_, cardBorder_, subtle_));
    }

    QSize thumbnailCellSizeHint() const {
        return ScreenshotGalleryItemDelegate::recommendedCellSize(list_->font(), list_->iconSize());
    }

    void repopulateList() {
        list_->clear();
        setPreviewPlaceholder(QStringLiteral("在左侧点选缩略图"));
        metaLabel_->clear();

        const QSize cell = thumbnailCellSizeHint();
        const int thumbW = list_->iconSize().width();
        const int thumbH = list_->iconSize().height();

        int shown = 0;
        for (const QFileInfo &fi : allFiles_) {
            if (!passesTimeFilter(fi))
                continue;

            QImageReader reader(fi.absoluteFilePath());
            reader.setAutoTransform(true);
            QImage img = reader.read();
            if (img.isNull())
                continue;

            ++shown;

            const QString timeLine = fi.lastModified().toString(QStringLiteral("MM-dd  HH:mm"));
            auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(fi.fileName(), timeLine));
            item->setData(Qt::UserRole, fi.absoluteFilePath());
            item->setData(Qt::UserRole + 1, shown);
            item->setToolTip(QStringLiteral("#%1\n%2\n%3")
                                 .arg(shown)
                                 .arg(fi.absoluteFilePath(), fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

            const QPixmap pm =
                QPixmap::fromImage(img.scaled(thumbW, thumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            item->setIcon(QIcon(pm));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
            item->setSizeHint(cell);
            list_->addItem(item);
        }

        if (shown == 0) {
            if (allFiles_.isEmpty()) {
                setPreviewPlaceholder(QStringLiteral(
                    "暂无截图\n\n"
                    "在「设置 → 截图保存」中选含「文件」后，在本窗口截取屏幕即可。"));
            } else {
                setPreviewPlaceholder(QStringLiteral("当前日期范围内没有结果，请调整日期或点「刷新」。"));
            }
            metaLabel_->clear();
            countLabel_->setText(QStringLiteral("共 %1 个文件 · 当前 0 张").arg(allFiles_.size()));
            return;
        }

        countLabel_->setText(QStringLiteral("共 %1 个文件 · 显示 %2 张").arg(allFiles_.size()).arg(shown));
        
        QTimer::singleShot(0, this, [this] {
            list_->setCurrentRow(0);
        });
    }

    void onSelectionChanged(QListWidgetItem *current, QListWidgetItem *) {
        if (!current) {
            setPreviewPlaceholder(QStringLiteral("在左侧点选缩略图"));
            metaLabel_->clear();
            return;
        }

        const QString path = current->data(Qt::UserRole).toString();
        QFileInfo fi(path);
        if (!fi.exists()) {
            setPreviewPlaceholder(QStringLiteral("文件已不存在"));
            metaLabel_->clear();
            return;
        }

        QImage img(path);
        if (img.isNull()) {
            setPreviewPlaceholder(QStringLiteral("无法加载图片"));
            metaLabel_->clear();
            return;
        }

        const int maxW = qMax(260, preview_->width() - 32);
        const int maxH = qMax(220, preview_->height() - 32);
        QPixmap pm = QPixmap::fromImage(img.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        preview_->setPixmap(pm);
        preview_->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; border: 1px solid %2; border-radius: 10px; padding: 10px; }")
            .arg(mutedBg_, cardBorder_));

        const qint64 bytes = fi.size();
        QString sizeStr;
        if (bytes < 1024)
            sizeStr = QStringLiteral("%1 B").arg(bytes);
        else if (bytes < 1024 * 1024)
            sizeStr = QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
        else
            sizeStr = QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);

        const QString meta = QStringLiteral("%1\n\n%2 × %3 · %4\n%5")
                                 .arg(fi.fileName())
                                 .arg(img.width())
                                 .arg(img.height())
                                 .arg(sizeStr, fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        metaLabel_->setTextFormat(Qt::PlainText);
        metaLabel_->setText(meta);
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        BaseDialog::resizeEvent(event);
        if (pathLabel_)
            pathLabel_->setText(elideMiddle(galleryDir_, pathLabel_->font(),
                                            qMax(200, width() - 48)));
        if (list_->currentItem())
            onSelectionChanged(list_->currentItem(), nullptr);
    }

private:
    static QString elideMiddle(const QString &str, const QFont &font, int maxPx) {
        if (maxPx <= 0)
            return str;
        QFontMetrics fm(font);
        return fm.elidedText(str, Qt::ElideMiddle, maxPx);
    }

    bool dark_{false};
    QString cardBg_, cardBorder_, subtle_, textMain_, mutedBg_, accent_;
    QString selBorderLight_;

    QListWidget *list_{nullptr};
    QLabel *preview_{nullptr};
    QLabel *metaLabel_{nullptr};
    QLabel *pathLabel_{nullptr};
    QLabel *countLabel_{nullptr};
    QLabel *gridTitleLabel_{nullptr};
    QPushButton *refreshBtn_{nullptr};
    QPushButton *folderBtn_{nullptr};
    QDateEdit *dateFrom_{nullptr};
    QDateEdit *dateTo_{nullptr};
    QFileInfoList allFiles_;
    QString galleryDir_;
};
