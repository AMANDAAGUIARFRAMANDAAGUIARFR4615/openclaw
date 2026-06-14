#pragma once

#include "global.h"
#include "../Theme.h"
#include "ZipUtils.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QStyleHints>
#include <QApplication>

// 版本卡片控件
class VersionCard : public QFrame {
    Q_OBJECT
signals:
    void startDownload(const QString &url, const QString &tagName);

public:
    explicit VersionCard(const QJsonObject &data, bool isLatest, QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setObjectName("VersionCard");
        setProperty("isLatest", isLatest); // 设置属性供QSS使用

        // 1. 解析数据
        QString tagName = data["tag_name"].toString();
        QString body = data["body"].toString();
        QString rawDate = data["created_at"].toString();
        QDateTime dt = QDateTime::fromString(rawDate, Qt::ISODate);
        QString dateStr = dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm") : rawDate;
        
        QString downloadUrl;
        QJsonArray assets = data["assets"].toArray();
        if (!assets.isEmpty()) {
            downloadUrl = assets.first().toObject()["browser_download_url"].toString();
        }

        // 2. 构建界面
        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->setContentsMargins(24, 20, 24, 20);
        layout->setSpacing(20);

        // 左侧信息
        QVBoxLayout *leftLayout = new QVBoxLayout();
        leftLayout->setSpacing(6);
        
        // 标题行
        QHBoxLayout *headerLine = new QHBoxLayout();
        QLabel *lblTag = new QLabel(tagName, this);
        lblTag->setObjectName("LabelTag");
        headerLine->addWidget(lblTag);

        if (isLatest) {
            QLabel *badge = new QLabel("当前最新", this);
            badge->setObjectName("BadgeLatest");
            headerLine->addWidget(badge);
        }
        headerLine->addStretch();

        QLabel *lblDate = new QLabel("发布于 " + dateStr, this);
        lblDate->setObjectName("LabelDate");
        
        QLabel *lblBody = new QLabel(body, this);
        lblBody->setObjectName("LabelBody");
        lblBody->setWordWrap(true);
        lblBody->setAlignment(Qt::AlignLeft | Qt::AlignTop);

        leftLayout->addLayout(headerLine);
        leftLayout->addWidget(lblDate);
        leftLayout->addWidget(lblBody);

        // 右侧按钮
        QPushButton *btn = new QPushButton(downloadUrl.isEmpty() ? "无效" : "安装", this);
        btn->setFixedSize(90, 34);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setObjectName("BtnPrimary");
        btn->setEnabled(!downloadUrl.isEmpty());

        // 使用Lambda捕获必要数据，无需成员变量
        connect(btn, &QPushButton::clicked, this, [this, downloadUrl, tagName](){
            emit startDownload(downloadUrl, tagName);
        });

        layout->addLayout(leftLayout, 1);
        layout->addWidget(btn);
    }
};

// 版本管理对话框
class VersionManagerDialog : public QDialog {
    Q_OBJECT
public:
    VersionManagerDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("软件更新");
        resize(600, 700);
        setupUI();
        fetchVersions();
    }

private:
    QVBoxLayout *m_listLayout;
    QWidget *m_scrollContent;
    QLabel *m_statusLabel;
    QWidget *m_loadingWidget;

    void applyMacPostUpdateFixes(const QString &appBundlePath) {
#if defined(Q_OS_MACOS)
        if (appBundlePath.isEmpty() || !QFileInfo::exists(appBundlePath)) {
            qWarning() << "更新后修复: App 路径无效 ->" << appBundlePath;
            return;
        }

        // 清理隔离属性，避免下载解压后的 bundle 触发 Gatekeeper 限制。
        const int xattrCode = QProcess::execute("/usr/bin/xattr", {"-dr", "com.apple.quarantine", appBundlePath});
        if (xattrCode != 0)
            qWarning() << "更新后修复: xattr 清理失败，退出码" << xattrCode;
#else
        Q_UNUSED(appBundlePath);
#endif
    }

    void setupUI() {
        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);

        // 头部
        QWidget *header = new QWidget(this);
        header->setObjectName("Header");
        QHBoxLayout *headerL = new QHBoxLayout(header);
        headerL->setContentsMargins(30, 20, 30, 20);
        
        QVBoxLayout *titleBox = new QVBoxLayout();
        QLabel *title = new QLabel("版本管理", header);
        title->setObjectName("Title");
        QLabel *sub = new QLabel("查看更新日志并选择版本安装", header);
        sub->setObjectName("SubTitle");
        titleBox->addWidget(title);
        titleBox->addWidget(sub);
        
        headerL->addLayout(titleBox);
        headerL->addStretch();

        // 列表滚动区
        QScrollArea *scroll = new QScrollArea(this);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        
        m_scrollContent = new QWidget();
        m_scrollContent->setObjectName("ScrollContent");
        m_listLayout = new QVBoxLayout(m_scrollContent);
        m_listLayout->setContentsMargins(30, 20, 30, 30);
        m_listLayout->setSpacing(16);
        scroll->setWidget(m_scrollContent);

        // 底部加载状态
        m_loadingWidget = new QWidget(this);
        QHBoxLayout *loadLayout = new QHBoxLayout(m_loadingWidget);
        loadLayout->setContentsMargins(0, 20, 0, 20);
        m_statusLabel = new QLabel("正在检查更新...", m_loadingWidget);
        m_statusLabel->setObjectName("StatusLabel");
        m_statusLabel->setAlignment(Qt::AlignCenter);
        loadLayout->addWidget(m_statusLabel);

        mainLayout->addWidget(header);
        mainLayout->addWidget(scroll);
        mainLayout->addWidget(m_loadingWidget);

        applyStyles();
    }

    void applyStyles() {
        setStyleSheet(Theme::fill(QStringLiteral(R"(
            QDialog { background: @{pageBg}; }
            QWidget#Header { background: @{surface}; border-bottom: 1px solid @{border}; }
            QWidget#ScrollContent { background: transparent; }

            QLabel#Title { font-size: 20px; font-weight: bold; color: @{textPrimary}; }
            QLabel#SubTitle, QLabel#StatusLabel { font-size: 13px; color: @{textMuted}; }
            QLabel#SectionTitle { font-size: 14px; font-weight: 600; color: @{textSecondary}; margin: 10px 0 4px 0; }

            /* 卡片基础 */
            VersionCard { background: @{surface}; border-radius: 12px; border: 1px solid @{border}; }
            VersionCard:hover { border: 1px solid @{primary}; background: @{surfaceHover}; }
            VersionCard QLabel#LabelTag { font-size: 18px; font-weight: bold; color: @{textPrimary}; }
            VersionCard QLabel#LabelDate, QLabel#LabelBody { font-size: 13px; color: @{textSecondary}; line-height: 20px; }

            /* 最新版样式 */
            VersionCard[isLatest="true"] { background: @{primarySoft}; border: 1px solid @{primary}; }
            VersionCard[isLatest="true"] QLabel#LabelTag { color: @{primary}; }
            VersionCard[isLatest="true"]:hover { border: 1px solid @{primary}; }
            QLabel#BadgeLatest { background: @{primary}; color: white; border-radius: 6px; padding: 4px 8px; font-weight: 600; }

            /* 按钮 */
            QPushButton#BtnPrimary { background: @{primary}; color: white; border: none; border-radius: 8px; font-weight: bold; }
            QPushButton#BtnPrimary:hover { background: @{primaryHover}; }
            QPushButton#BtnPrimary:pressed { background: @{primaryPressed}; }

            QScrollBar:vertical { width: 8px; background: transparent; }
            QScrollBar::handle:vertical { background: @{scrollHandle}; border-radius: 4px; }
            QScrollBar::handle:vertical:hover { background: @{scrollHandleHover}; }
        )")));
    }

    void fetchVersions() {
        QString repo = (QSysInfo::productType() == "windows") ? "RemotePro-windows" : "RemotePro-macos";
        QUrl url(QString("https://gitee.com/api/v5/repos/RemotePro/%1/releases?direction=desc&page=1&per_page=10").arg(repo));
        
        auto reply = networkAccessManager->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply](){
            reply->deleteLater();
            if (reply->error()) {
                m_statusLabel->setText("获取失败: " + reply->errorString());
                m_statusLabel->setStyleSheet(QString("color: %1;").arg(Theme::danger()));
                return;
            }

            QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
            if (array.isEmpty()) { m_statusLabel->setText("暂无版本信息"); return; }

            // 渲染列表
            for (int i = 0; i < array.size(); ++i) {
                QJsonObject obj = array[i].toObject();
                if (obj.isEmpty()) continue;

                bool isLatest = (i == 0);
                
                // 添加分节标题
                if (isLatest) 
                    addSectionTitle("最新版本");
                else if (i == 1) 
                    addSectionTitle("历史版本");

                auto card = new VersionCard(obj, isLatest);
                connect(card, &VersionCard::startDownload, this, &VersionManagerDialog::downloadFile);
                m_listLayout->addWidget(card);
            }
            m_listLayout->addStretch();
            m_loadingWidget->hide();
        });
    }

    void addSectionTitle(const QString &text) {
        if (m_listLayout->count() > 1) m_listLayout->addSpacing(10);
        QLabel *lbl = new QLabel(text, m_scrollContent);
        lbl->setObjectName("SectionTitle");
        m_listLayout->addWidget(lbl);
    }

    void downloadFile(const QString &url, const QString &tagName) {
        QUrl qurl(url);
        QString fileName = qurl.fileName();
        if (fileName.isEmpty()) fileName = QString("update_%1.zip").arg(tagName);
        QString filePath = QDir::temp().filePath(fileName);

        QFile *file = new QFile(filePath);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, "错误", "无法写入文件");
            delete file; return;
        }

        QProgressDialog *pd = new QProgressDialog("正在下载 " + tagName + " ...", "取消", 0, 100, this);
        pd->setWindowTitle("下载更新");
        pd->setWindowModality(Qt::WindowModal);
        pd->setStyleSheet(Theme::fill(QStringLiteral("QProgressBar{border:1px solid @{border};border-radius:8px;text-align:center} QProgressBar::chunk{background-color:@{primary};border-radius:7px}")));
        
        QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(qurl));
        connect(pd, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
        
        connect(reply, &QNetworkReply::downloadProgress, pd, [pd](qint64 cur, qint64 total){
            if(total > 0) pd->setValue((int)(cur * 100 / total));
        });
        connect(reply, &QNetworkReply::readyRead, this, [file, reply](){ file->write(reply->readAll()); });
        connect(reply, &QNetworkReply::finished, this, [=](){
            delete file; pd->deleteLater(); reply->deleteLater();
            
            if (reply->error()) {
                if (reply->error() != QNetworkReply::OperationCanceledError)
                    QMessageBox::warning(this, "失败", reply->errorString());
                QFile::remove(filePath);
            } else {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
                QString extractDir = QCoreApplication::applicationDirPath();
                QString appBundlePath;

#if defined(Q_OS_MACOS)
                QDir dir(extractDir);
                dir.cdUp();
                dir.cdUp();
                appBundlePath = dir.absolutePath();
                dir.cdUp();
                extractDir = dir.absolutePath();
#endif

                if (ZipUtils::extractSmart(filePath, extractDir)) {
#if defined(Q_OS_MACOS)
                    applyMacPostUpdateFixes(appBundlePath);
#endif
                    QFile::remove(filePath);
                    Tools::quitApplication(true, appBundlePath);
                } else {
                    new ToastWidget("解压失败");
                }
#endif
            }
        });
    }
};
