#pragma once

#include "BaseDialog.h"
#include "Tools.h"
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QImageReader>
#include <QResizeEvent>
#include <QStyleHints>
#include <QUrl>
#include <QVBoxLayout>

class ScreenshotGalleryDialog : public BaseDialog {
public:
    static void open(QWidget *parent = nullptr) {
        ScreenshotGalleryDialog dialog(parent);
        dialog.resize(1020, 660);
        dialog.exec();
    }

private:
    explicit ScreenshotGalleryDialog(QWidget *parent = nullptr)
        : BaseDialog(QStringLiteral("截图相册"), parent) {
        dark_ = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        cardBg_ = dark_ ? QStringLiteral("#1A1B1E") : QStringLiteral("#FFFFFF");
        cardBorder_ = dark_ ? QStringLiteral("#2E3036") : QStringLiteral("#E2E5EB");
        subtle_ = dark_ ? QStringLiteral("#94A3B8") : QStringLiteral("#64748B");
        textMain_ = dark_ ? QStringLiteral("#F1F5F9") : QStringLiteral("#0F172A");
        mutedBg_ = dark_ ? QStringLiteral("#121316") : QStringLiteral("#F8FAFC");
        accent_ = QStringLiteral("#3B82F6");
        selBgLight_ = QStringLiteral("#DBEAFE");
        selBorderLight_ = QStringLiteral("#2563EB");
        selBgDark_ = QStringLiteral("#1E3A5F");
        selTextDark_ = QStringLiteral("#F8FAFC");

        buildUi();
        reloadGallery();
    }

    void buildUi() {
        auto *root = contentLayout();
        root->setSpacing(0);
        root->setContentsMargins(0, 0, 0, 0);

        // —— 单行工具区：统计 + 时间筛选 + 按钮；路径单独一行（省高度） ——
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

        auto *timeLbl = new QLabel(QStringLiteral("时间"), this);
        timeLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(subtle_));
        row1->addWidget(timeLbl);

        timeFilter_ = new QComboBox(this);
        timeFilter_->setMinimumWidth(140);
        timeFilter_->addItem(QStringLiteral("全部"), 0);
        timeFilter_->addItem(QStringLiteral("今天"), 1);
        timeFilter_->addItem(QStringLiteral("近 7 天"), 2);
        timeFilter_->addItem(QStringLiteral("近 30 天"), 3);
        timeFilter_->addItem(QStringLiteral("自选…"), 4);
        styleCombo(timeFilter_);
        row1->addWidget(timeFilter_);

        customRangeWrap_ = new QWidget(this);
        auto *rangeLay = new QHBoxLayout(customRangeWrap_);
        rangeLay->setContentsMargins(0, 0, 0, 0);
        rangeLay->setSpacing(6);
        dateFrom_ = new QDateEdit(QDate::currentDate().addDays(-7), this);
        dateTo_ = new QDateEdit(QDate::currentDate(), this);
        for (auto *de : {dateFrom_, dateTo_}) {
            de->setCalendarPopup(true);
            de->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
            de->setMinimumHeight(30);
            styleDateEdit(de);
        }
        rangeLay->addWidget(dateFrom_);
        auto *rangeDash = new QLabel(QStringLiteral("—"), customRangeWrap_);
        rangeDash->setStyleSheet(QStringLiteral("color: %1;").arg(subtle_));
        rangeLay->addWidget(rangeDash);
        rangeLay->addWidget(dateTo_);
        customRangeWrap_->hide();
        row1->addWidget(customRangeWrap_);

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
        {
            const QString itemText = dark_ ? selTextDark_ : textMain_;
            const QString hoverBg = dark_ ? QStringLiteral("#252830") : QStringLiteral("#F1F5F9");
            const QString selBg = dark_ ? selBgDark_ : selBgLight_;
            const QString selText = dark_ ? selTextDark_ : QStringLiteral("#0F172A");
            const QString selBorder = dark_ ? QStringLiteral("#60A5FA") : selBorderLight_;
            list_->setStyleSheet(QStringLiteral(
                "QListWidget { background: transparent; border: none; padding: 6px; }"
                "QListWidget::item { border-radius: 10px; padding: 8px 8px 14px 8px; margin: 3px; color: %1; }"
                "QListWidget::item:hover { background-color: %2; color: %1; }"
                "QListWidget::item:selected { background-color: %3; color: %4; border: 2px solid %5; }"
                "QListWidget::item:selected:active { background-color: %3; color: %4; }")
                .arg(itemText, hoverBg, selBg, selText, selBorder));
        }
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
            const QString dir = Tools::screenshotSaveDirectory();
            QDir().mkpath(dir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });
        connect(timeFilter_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                &ScreenshotGalleryDialog::onTimeFilterChanged);
        connect(dateFrom_, &QDateEdit::dateChanged, this, &ScreenshotGalleryDialog::onCustomRangeChanged);
        connect(dateTo_, &QDateEdit::dateChanged, this, &ScreenshotGalleryDialog::onCustomRangeChanged);
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
                "QPushButton:hover { background: #2563EB; }"
                "QPushButton:pressed { background: #1D4ED8; }")
                .arg(accent_));
        } else {
            b->setStyleSheet(QStringLiteral(
                "QPushButton { padding: 7px 16px; border-radius: 8px; border: 1px solid %1; background: %2; color: %3; }"
                "QPushButton:hover { border-color: %4; background: %5; }")
                .arg(cardBorder_, cardBg_, textMain_, accent_,
                     dark_ ? QStringLiteral("#252830") : QStringLiteral("#F1F5F9")));
        }
    }

    void styleCombo(QComboBox *cb) {
        cb->setMinimumHeight(32);
        cb->setStyleSheet(QStringLiteral(
            "QComboBox { padding: 5px 12px; border-radius: 8px; border: 1px solid %1; background: %2; color: %3; }"
            "QComboBox:hover { border-color: %4; }"
            "QComboBox::drop-down { width: 26px; border: none; }")
            .arg(cardBorder_, dark_ ? QStringLiteral("#22252B") : QStringLiteral("#FFFFFF"), textMain_, accent_));
    }

    void styleDateEdit(QDateEdit *de) {
        de->setStyleSheet(QStringLiteral(
            "QDateEdit { padding: 5px 8px; border-radius: 8px; border: 1px solid %1; background: %2; color: %3; }"
            "QDateEdit:hover { border-color: %4; }")
            .arg(cardBorder_, dark_ ? QStringLiteral("#22252B") : QStringLiteral("#FFFFFF"), textMain_, accent_));
    }

    void onTimeFilterChanged(int index) {
        customRangeWrap_->setVisible(index == 4);
        updateGridTitle();
        repopulateList();
    }

    void onCustomRangeChanged() {
        if (timeFilter_->currentIndex() == 4)
            repopulateList();
    }

    void updateGridTitle() {
        const int i = timeFilter_->currentIndex();
        if (i == 4) {
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
            return;
        }
        static const QStringList titles{QStringLiteral("全部截图"), QStringLiteral("今天的截图"),
                                         QStringLiteral("最近 7 天"), QStringLiteral("最近 30 天")};
        if (i >= 0 && i < titles.size())
            gridTitleLabel_->setText(titles[i]);
    }

    void reloadGallery() {
        const QString dirPath = Tools::screenshotSaveDirectory();
        QDir().mkpath(dirPath);
        pathLabel_->setText(elideMiddle(dirPath, pathLabel_->font(), qMax(200, width() - 48)));

        QDir dir(dirPath);
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
        const QDateTime m = fi.lastModified();
        const QDate d = m.date();
        const int idx = timeFilter_->currentIndex();

        if (idx == 0)
            return true;
        if (idx == 1)
            return d == QDate::currentDate();
        if (idx == 2)
            return m >= QDateTime::currentDateTime().addDays(-7);
        if (idx == 3)
            return m >= QDateTime::currentDateTime().addDays(-30);
        if (idx == 4) {
            QDate from = dateFrom_->date();
            QDate to = dateTo_->date();
            if (from > to) {
                const QDate tmp = from;
                from = to;
                to = tmp;
            }
            return d >= from && d <= to;
        }
        return true;
    }

    void setPreviewPlaceholder(const QString &text) {
        preview_->clear();
        preview_->setText(text);
        preview_->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; border: 1px dashed %2; border-radius: 10px; padding: 12px; color: %3; }")
            .arg(mutedBg_, cardBorder_, subtle_));
    }

    QSize thumbnailCellSizeHint() const {
        const QSize iconSz = list_->iconSize();
        QFontMetrics fm(list_->font());
        const int twoLines = fm.lineSpacing() * 2 + fm.leading() + 6;
        const int w = iconSz.width() + 28;
        const int h = iconSz.height() + twoLines + 28;
        return QSize(w, h);
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

            const QString timeLine = fi.lastModified().toString(QStringLiteral("MM-dd  HH:mm"));
            auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(fi.fileName(), timeLine));
            item->setData(Qt::UserRole, fi.absoluteFilePath());
            item->setToolTip(QStringLiteral("%1\n%2")
                                 .arg(fi.absoluteFilePath(), fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));

            const QPixmap pm =
                QPixmap::fromImage(img.scaled(thumbW, thumbH, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            item->setIcon(QIcon(pm));
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
            item->setSizeHint(cell);
            list_->addItem(item);
            ++shown;
        }

        if (shown == 0) {
            if (allFiles_.isEmpty()) {
                setPreviewPlaceholder(QStringLiteral(
                    "暂无截图\n\n"
                    "在「设置 → 截图保存」中选含「文件」后，在投屏窗口截图即可。"));
            } else {
                setPreviewPlaceholder(QStringLiteral("当前时间筛选下没有结果，请换条件或点「刷新」。"));
            }
            metaLabel_->clear();
            countLabel_->setText(QStringLiteral("共 %1 个文件 · 当前 0 张").arg(allFiles_.size()));
            return;
        }

        countLabel_->setText(QStringLiteral("共 %1 个文件 · 显示 %2 张").arg(allFiles_.size()).arg(shown));
        list_->setCurrentRow(0);
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

        metaLabel_->setTextFormat(Qt::PlainText);
        metaLabel_->setText(QStringLiteral("%1\n\n%2 × %3 · %4\n%5")
                               .arg(fi.fileName())
                               .arg(img.width())
                               .arg(img.height())
                               .arg(sizeStr, fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        BaseDialog::resizeEvent(event);
        if (pathLabel_)
            pathLabel_->setText(elideMiddle(Tools::screenshotSaveDirectory(), pathLabel_->font(),
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
    QString selBgLight_, selBorderLight_, selBgDark_, selTextDark_;

    QListWidget *list_{nullptr};
    QLabel *preview_{nullptr};
    QLabel *metaLabel_{nullptr};
    QLabel *pathLabel_{nullptr};
    QLabel *countLabel_{nullptr};
    QLabel *gridTitleLabel_{nullptr};
    QPushButton *refreshBtn_{nullptr};
    QPushButton *folderBtn_{nullptr};
    QComboBox *timeFilter_{nullptr};
    QDateEdit *dateFrom_{nullptr};
    QDateEdit *dateTo_{nullptr};
    QWidget *customRangeWrap_{nullptr};
    QFileInfoList allFiles_;
};
