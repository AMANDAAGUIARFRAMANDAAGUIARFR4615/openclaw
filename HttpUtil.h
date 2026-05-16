#pragma once

#include "Logger.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QString>
#include <QStringView>
#include <QUrl>
#include <utility>
#include <vector>

namespace HttpUtil {

enum class Method { Get, Post, Put, HttpDelete };

struct Result {
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    QString errorString;
    int httpStatus = 0;
    QByteArray bytes;
    /** 仅在回调期间可与保存的 QNetworkReply* 比对；回调返回后勿再用 */
    QNetworkReply *reply = nullptr;

    [[nodiscard]] bool ok() const { return networkError == QNetworkReply::NoError; }
    [[nodiscard]] bool canceled() const {
        return networkError == QNetworkReply::OperationCanceledError;
    }
};

inline QString redactUrl(const QUrl &u) {
    if (u.password().isEmpty())
        return u.toString(QUrl::FullyEncoded);
    QUrl cp(u);
    cp.setPassword(QStringLiteral("***"));
    return cp.toString(QUrl::FullyEncoded);
}

inline bool looksBinaryPreview(const QByteArray &sample) {
    if (sample.contains(char(0)))
        return true;

    using U = unsigned char;
    if (sample.size() >= 2 && U(sample[0]) == 0xffU && U(sample[1]) == 0xd8U)
        return true;
    if (sample.startsWith("\x89PNG\r\n\x1a\n"))
        return true;

    const int n = sample.size();
    if (n == 0)
        return false;
    int ctl = 0;
    for (int i = 0; i < n; ++i) {
        const U c = U(sample.at(i));
        if (c == U('\t') || c == U('\n') || c == U('\r'))
            continue;
        if (c < 32 || c == 127U)
            ++ctl;
    }
    return ctl * 4 > n;
}

inline QString summarizeBytePreview(const QByteArray &data, int maxPreview = 600) {
    if (data.isEmpty())
        return QStringLiteral("(no body)");

    const QByteArray snip = data.left(maxPreview);
    if (looksBinaryPreview(snip))
        return QStringLiteral("[%1 bytes]").arg(data.size());

    QString s = QString::fromUtf8(snip);
    if (data.size() > maxPreview)
        s.append(QStringLiteral("…(+ %1 bytes)").arg(data.size() - maxPreview));
    return s;
}

inline void logOutgoing(const char *verb, const QNetworkRequest &req, const QByteArray &body) {
    const QString u = redactUrl(req.url());
    if (body.isEmpty())
        qDebugEx() << QStringLiteral("HTTP → %1 %2").arg(QString::fromLatin1(verb), u);
    else
        qDebugEx() << QStringLiteral("HTTP → %1 %2 body=").arg(QString::fromLatin1(verb), u)
                   << summarizeBytePreview(body, 900);
}

inline const char *verbName(Method m) {
    switch (m) {
    case Method::Get:
        return "GET";
    case Method::Post:
        return "POST";
    case Method::Put:
        return "PUT";
    case Method::HttpDelete:
        return "DELETE";
    }
    return "?";
}

/** 构造一次 HTTP 请求；配合 Sender::submit / Request::submit 发起。 */
struct Request {
    Method method = Method::Get;
    QUrl url;
    QByteArray body;
    int timeoutMs = 30000;
    std::vector<std::pair<QByteArray, QByteArray>> headers;
    bool jsonContentType = false;

    /** 在 base URL 下挂 path（如 "/foo/bar"）。 */
    [[nodiscard]] static Request relative(const QUrl &base, QStringView path) {
        Request r;
        QUrl u(base);
        u.setPath(path.toString());
        r.url = u;
        return r;
    }

    [[nodiscard]] static Request relative(const QUrl &base, QLatin1String path) {
        Request r;
        QUrl u(base);
        u.setPath(QString(path));
        r.url = u;
        return r;
    }

    /** 直接使用完整 URL。 */
    [[nodiscard]] static Request absolute(const QUrl &full) {
        Request r;
        r.url = full;
        return r;
    }

    Request &get() {
        method = Method::Get;
        body.clear();
        return *this;
    }

    Request &post(const QByteArray &b = {}) {
        method = Method::Post;
        body = b;
        return *this;
    }

    Request &postJson(const QByteArray &b) {
        method = Method::Post;
        body = b;
        jsonContentType = true;
        return *this;
    }

    Request &putJson(const QByteArray &b) {
        method = Method::Put;
        body = b;
        jsonContentType = true;
        return *this;
    }

    Request &put(const QByteArray &b = {}) {
        method = Method::Put;
        body = b;
        return *this;
    }

    Request &del() {
        method = Method::HttpDelete;
        body.clear();
        return *this;
    }

    Request &json(bool on = true) {
        jsonContentType = on;
        return *this;
    }

    Request &timeout(int ms) {
        timeoutMs = ms;
        return *this;
    }

    Request &hdr(const char *name, const QByteArray &value) {
        headers.emplace_back(QByteArray(name), value);
        return *this;
    }

    /** 脚本桥 X-YK-Session-Id */
    Request &ykSession(const QString &sessionId) { return hdr("X-YK-Session-Id", sessionId.toUtf8()); }

    template <typename Fn>
    QNetworkReply *submit(QNetworkAccessManager *nam, QObject *context, Fn &&onDone) const;

    QNetworkReply *submit(QNetworkAccessManager *nam, QObject *context) const {
        return submit(nam, context, [](const Result &) {});
    }
};

struct Sender {
    QNetworkAccessManager *nam = nullptr;
    QObject *ctx = nullptr;

    [[nodiscard]] explicit operator bool() const { return nam && ctx; }

    template <typename Fn>
    QNetworkReply *submit(Request req, Fn &&onDone) const {
        return req.submit(nam, ctx, std::forward<Fn>(onDone));
    }

    /** 不关心响应时可调用 */
    void fire(Request req) const {
        if (*this)
            req.submit(nam, ctx);
    }
};

inline QNetworkRequest buildRequest(const Request &rq) {
    QNetworkRequest req(rq.url);
    if (rq.jsonContentType)
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    for (const auto &h : rq.headers)
        req.setRawHeader(h.first, h.second);
    if (rq.timeoutMs > 0)
        req.setTransferTimeout(rq.timeoutMs);
    return req;
}

inline QNetworkReply *issue(QNetworkAccessManager *nam,
                            Method method,
                            QNetworkRequest req,
                            const QByteArray &body) {
    switch (method) {
    case Method::Get:
        return nam->get(req);
    case Method::Post:
        return nam->post(req, body);
    case Method::Put:
        return nam->put(req, body);
    case Method::HttpDelete:
        return nam->deleteResource(req);
    }
    return nullptr;
}

template <typename Fn>
inline QNetworkReply *Request::submit(QNetworkAccessManager *nam, QObject *context, Fn &&onDone) const {
    Request snapshot = *this;

    if (!nam || !context) {
        qWarningEx() << "HttpUtil::submit: QNetworkAccessManager or QObject context is null";
        return nullptr;
    }

    QNetworkRequest qreq = buildRequest(snapshot);
    const char *verb = verbName(snapshot.method);
    const QByteArray logBody =
        (snapshot.method == Method::Get || snapshot.method == Method::HttpDelete) ? QByteArray()
                                                                                   : snapshot.body;
    logOutgoing(verb, qreq, logBody);

    QNetworkReply *reply =
        issue(nam, snapshot.method, std::move(qreq), snapshot.body);
    if (!reply) {
        qWarningEx() << "HttpUtil::submit: failed to start" << verb << redactUrl(snapshot.url);
        return nullptr;
    }

    const QString urlSeen = redactUrl(snapshot.url);
    const QString verbStr = QString::fromLatin1(verb);

    QObject::connect(reply,
                     &QNetworkReply::finished,
                     context,
                     [reply, urlSeen, verbStr, onDone = std::forward<Fn>(onDone)]() mutable {
                         Result r;
                         r.reply = reply;
                         r.networkError = reply->error();
                         r.errorString =
                             (r.networkError == QNetworkReply::NoError) ? QString()
                                                                        : reply->errorString();
                         r.httpStatus =
                             reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                         r.bytes = reply->readAll();

                         QString rs =
                             reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();

                         QString line = QStringLiteral("HTTP ← %1 %2 status=%3 netErr=%4 bytes=%5")
                                            .arg(verbStr,
                                                 urlSeen,
                                                 QString::number(r.httpStatus),
                                                 QString::number(int(r.networkError)),
                                                 QString::number(r.bytes.size()));
                         if (r.networkError != QNetworkReply::NoError && !r.errorString.isEmpty())
                             line.append(QStringLiteral(" msg=%1").arg(r.errorString));
                         if (!rs.isEmpty())
                             line.append(QStringLiteral(" reason=%1").arg(rs));
                         line.append(QStringLiteral(" body="));
                         line.append(summarizeBytePreview(r.bytes, 600));
                         qDebugEx() << line;

                         reply->deleteLater();
                         onDone(r);
                     });

    return reply;
}

} // namespace HttpUtil
