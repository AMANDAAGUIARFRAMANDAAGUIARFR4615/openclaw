#pragma once

#include "BaseDialog.h"
#include "global.h"
#include "Tools.h"
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTabBar>
#include <QTabWidget>
#include <QToolTip>
#include <QVBoxLayout>

class SourceRepoDialog : public BaseDialog {
    Q_OBJECT

public:
    explicit SourceRepoDialog(QWidget *parent = nullptr) : BaseDialog("使用相机APP扫码", parent) {
        const bool isMobileLayout =
#if defined(Q_OS_IOS) || defined(Q_OS_ANDROID)
            true;
#else
            false;
#endif

        if (!isMobileLayout) {
            setMinimumSize(540, 620);
        } else {
            setWindowState(windowState() | Qt::WindowFullScreen);
        }

        auto mainLayout = contentLayout();
        mainLayout->setContentsMargins(isMobileLayout ? 12 : 18, isMobileLayout ? 12 : 18,
                                       isMobileLayout ? 12 : 18, isMobileLayout ? 12 : 18);
        mainLayout->setSpacing(isMobileLayout ? 10 : 14);

        auto tabWidget = new QTabWidget(this);
        tabWidget->tabBar()->setExpanding(isMobileLayout);
        tabWidget->setDocumentMode(true);

        struct SourceInfo {
            QString title;
            QString url;
        };
        const QList<SourceInfo> sources = {
            {"Sileo", "sileo://source/" + Config::SITE_URL},
            {"Cydia", "cydia://url/https://cydia.saurik.com/api/share#?source=" + Config::SITE_URL}
        };

        const int qrSize = isMobileLayout ? 220 : 400;

        for (const auto &source : sources) {
            auto page = new QWidget();
            auto vLayout = new QVBoxLayout(page);
            vLayout->setContentsMargins(isMobileLayout ? 6 : 10, isMobileLayout ? 8 : 12,
                                        isMobileLayout ? 6 : 10, isMobileLayout ? 8 : 12);
            vLayout->setSpacing(isMobileLayout ? 8 : 12);

            const auto img = Tools::generateQrImage(source.url);
            const QPixmap pixmap = QPixmap::fromImage(img).scaled(
                qrSize, qrSize,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            );

            auto imgLabel = new QLabel(page);
            imgLabel->setPixmap(pixmap);
            imgLabel->setAlignment(Qt::AlignCenter);

            const auto displayUrl = source.url.mid(source.url.lastIndexOf("https://"));
            const QString richText = QString("如不方便扫码，请手动输入软件源地址：<br><a href=\"%1\" style=\"color: %2; text-decoration: none;\">%1</a>")
                                         .arg(displayUrl, Theme::primary());

            auto textLabel = new QLabel(richText, page);
            textLabel->setAlignment(Qt::AlignCenter);
            textLabel->setWordWrap(true);
            textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
            textLabel->setOpenExternalLinks(false);

            auto copyButton = new QPushButton("复制软件源地址", page);
            if (isMobileLayout) {
                copyButton->setMinimumHeight(38);
            }

            connect(textLabel, &QLabel::linkActivated, [displayUrl](const QString &link) {
                qApp->clipboard()->setText(link);
                Tools::showToast(QStringLiteral("地址已复制"));
            });
            connect(copyButton, &QPushButton::clicked, [displayUrl]() {
                qApp->clipboard()->setText(displayUrl);
                Tools::showToast(QStringLiteral("地址已复制"));
            });

            vLayout->addWidget(imgLabel);
            vLayout->addWidget(textLabel);
            vLayout->addWidget(copyButton, 0, Qt::AlignCenter);

            tabWidget->addTab(page, source.title);
        }

        mainLayout->addWidget(tabWidget);
    }
};
