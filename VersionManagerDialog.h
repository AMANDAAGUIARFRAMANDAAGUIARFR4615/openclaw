#pragma once

#include "global.h"
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
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
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
        bool dark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;

        // 颜色变量: 背景, 内容背, 边框, 主字, 次字, 悬停边框, 悬停背景, 最新背, 最新框, 主色
        QString c[] = {
            dark ? "#232324" : "#F7F8FA", // 0
            dark ? "#2D2D2D" : "#FFFFFF", // 1
            dark ? "#484849" : "#E5E6EB", // 2
            dark ? "#E5E6EB" : "#1D2129", // 3
            dark ? "#A9AEB8" : "#86909C", // 4
            "#165DFF",                    // 5 Hover Border
            dark ? "#353535" : "#FFFFFF", // 6 Hover Bg
            dark ? "#1A2234" : "#F2F8FE", // 7 Latest Bg
            dark ? "#2A3D5E" : "#BEDAFF", // 8 Latest Border
            "#165DFF"                     // 9 Primary
        };

        setStyleSheet(QString(R"(
            QDialog { background: %0; font-family: "Microsoft YaHei", sans-serif; }
            QWidget#Header { background: %1; border-bottom: 1px solid %2; }
            QWidget#ScrollContent { background: transparent; }
            
            QLabel#Title { font-size: 20px; font-weight: bold; color: %3; }
            QLabel#SubTitle, QLabel#StatusLabel { font-size: 13px; color: %4; }
            QLabel#SectionTitle { font-size: 14px; font-weight: 600; color: %4; margin: 10px 0 4px 0; }
            
            /* 卡片基础 */
            VersionCard { background: %1; border-radius: 8px; border: 1px solid %2; }
            VersionCard:hover { border: 1px solid %5; background: %6; }
            VersionCard QLabel#LabelTag { font-size: 18px; font-weight: bold; color: %3; }
            VersionCard QLabel#LabelDate, QLabel#LabelBody { font-size: 13px; color: %4; line-height: 20px; }

            /* 最新版样式 */
            VersionCard[isLatest="true"] { background: %7; border: 1px solid %8; }
            VersionCard[isLatest="true"] QLabel#LabelTag { color: %9; }
            VersionCard[isLatest="true"]:hover { border: 1px solid %9; }
            QLabel#BadgeLatest { background: %9; color: white; border-radius: 4px; padding: 4px 8px; font-weight: 600; }

            /* 按钮 */
            QPushButton#BtnPrimary { background: %9; color: white; border: none; border-radius: 4px; font-weight: bold; }
            QPushButton#BtnPrimary:hover { background: #4080FF; }
            QPushButton#BtnPrimary:pressed { background: #0E42D2; }
            
            QScrollBar:vertical { width: 6px; background: transparent; }
            QScrollBar::handle:vertical { background: %2; border-radius: 3px; }
        )").arg(c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9]));
    }

    void fetchVersions() {
        QString repo = (QSysInfo::productType() == "windows") ? "RemotePro-windows" : "RemotePro-macos";
        QUrl url(QString("https://gitee.com/api/v5/repos/RemotePro/%1/releases?direction=desc&page=1&per_page=10").arg(repo));
        
        auto reply = networkAccessManager->get(QNetworkRequest(url));
        connect(reply, &QNetworkReply::finished, this, [this, reply](){
            reply->deleteLater();
            if (reply->error()) {
                m_statusLabel->setText("获取失败: " + reply->errorString());
                m_statusLabel->setStyleSheet("color: #F53F3F;");
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

                auto *card = new VersionCard(obj, isLatest);
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
        QString filePath = QDir(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)).filePath(fileName);

        QFile *file = new QFile(filePath);
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, "错误", "无法写入文件");
            delete file; return;
        }

        QProgressDialog *pd = new QProgressDialog("正在下载 " + tagName + " ...", "取消", 0, 100, this);
        pd->setWindowTitle("下载更新");
        pd->setWindowModality(Qt::WindowModal);
        pd->setStyleSheet("QProgressBar{border:1px solid #E5E6EB;border-radius:4px;text-align:center} QProgressBar::chunk{background-color:#165DFF}");
        
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
                if (ZipUtils::extractSmart(filePath, ".")) {
                    QFile::remove(filePath);
                    qApp->quit();
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
                    QProcess::startDetached(qApp->applicationFilePath());
#endif
                } else {
                    new ToastWidget("解压失败");
                }
#endif
            }
        });
    }
};
