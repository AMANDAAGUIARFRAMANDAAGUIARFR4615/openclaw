#pragma once

#include "BaseDialog.h"
#include "DeviceInfo.h"
#include "HttpUtil.h"
#include "ToastWidget.h"
#include "global.h"
#include <QApplication>
#include <QClipboard>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyleHints>
#include <QTextEdit>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <optional>

/** 通过设备 HTTP（docs 模块 19：prefs 接口）可视化编辑用户配置数据。 */
class PrefsConfigEditorDialog : public BaseDialog {
public:
    static void open(DeviceInfo *deviceInfo, QWidget *parent) {
        if (!deviceInfo) {
            return;
        }
        if (deviceInfo->localIp.isEmpty()) {
            new ToastWidget(
                QStringLiteral("当前连接没有可用的局域网 IP，无法访问设备上的 HTTP 接口。"
                               "请改用同一局域网的 Wi-Fi 连接后再试。"),
                parent);
            return;
        }
        PrefsConfigEditorDialog dialog(deviceInfo, parent);
        dialog.resize(960, 620);
        dialog.exec();
    }

private:
    enum class NodeKind { Key, Field };

    explicit PrefsConfigEditorDialog(DeviceInfo *deviceInfo, QWidget *parent)
        : BaseDialog(QStringLiteral("用户配置编辑器"), parent, false), deviceInfo_(deviceInfo) {
        auto *root = contentLayout();
        root->setContentsMargins(14, 14, 14, 14);
        root->setSpacing(10);

        auto *toolbar = new QFrame(this);
        auto *toolbarLay = new QHBoxLayout(toolbar);
        toolbarLay->setContentsMargins(0, 0, 0, 0);
        toolbarLay->setSpacing(8);

        namespaceEdit_ = new QLineEdit(this);
        namespaceEdit_->setPlaceholderText(QStringLiteral("namespace（可选，留空使用当前任务）"));
        namespaceEdit_->setClearButtonEnabled(true);

        filterEdit_ = new QLineEdit(this);
        filterEdit_->setPlaceholderText(QStringLiteral("筛选 key…"));
        filterEdit_->setClearButtonEnabled(true);

        loadBtn_ = new QPushButton(QStringLiteral("加载"), this);
        addBtn_ = new QPushButton(QStringLiteral("新增"), this);
        clearBtn_ = new QPushButton(QStringLiteral("清空分组"), this);
        for (auto *btn : {loadBtn_, addBtn_, clearBtn_}) {
            btn->setAutoDefault(false);
            btn->setDefault(false);
        }

        toolbarLay->addWidget(new QLabel(QStringLiteral("分组:"), this));
        toolbarLay->addWidget(namespaceEdit_, 1);
        toolbarLay->addWidget(filterEdit_, 1);
        toolbarLay->addWidget(loadBtn_);
        toolbarLay->addWidget(addBtn_);
        toolbarLay->addWidget(clearBtn_);
        root->addWidget(toolbar);

        tree_ = new QTreeWidget(this);
        tree_->setHeaderLabels({QStringLiteral("键"), QStringLiteral("值"), QStringLiteral("类型")});
        tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        tree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        tree_->setAlternatingRowColors(true);
        tree_->setContextMenuPolicy(Qt::CustomContextMenu);
        tree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tree_->setFocusPolicy(Qt::StrongFocus);
        root->addWidget(tree_, 1);

        statusLabel_ = new QLabel(QStringLiteral("点击「加载」查看配置；namespace 留空则使用当前任务默认分组"), this);
        statusLabel_->setWordWrap(true);
        root->addWidget(statusLabel_);

        connect(loadBtn_, &QPushButton::clicked, this, &PrefsConfigEditorDialog::startLoad);
        connect(addBtn_, &QPushButton::clicked, this, &PrefsConfigEditorDialog::addKey);
        connect(clearBtn_, &QPushButton::clicked, this, &PrefsConfigEditorDialog::clearNamespace);
        connect(filterEdit_, &QLineEdit::textChanged, this, &PrefsConfigEditorDialog::applyFilter);
        connect(tree_, &QTreeWidget::customContextMenuRequested, this, &PrefsConfigEditorDialog::showTreeMenu);
        connect(tree_, &QTreeWidget::itemDoubleClicked, this, &PrefsConfigEditorDialog::editSelectedKey);

        baseUrl_.setScheme(QStringLiteral("http"));
        baseUrl_.setHost(deviceInfo_->localIp);
        baseUrl_.setPort(65322);

        startSession();
    }

    ~PrefsConfigEditorDialog() override { deleteSessionIfNeeded(); }

    [[nodiscard]] HttpUtil::Sender net() {
        return HttpUtil::Sender{networkAccessManager, this};
    }

    [[nodiscard]] HttpUtil::Request api(QLatin1String route) {
        return HttpUtil::Request::relative(baseUrl_, route);
    }

    [[nodiscard]] HttpUtil::Request apiSes(QLatin1String route) {
        return api(route).ykSession(sessionId_);
    }

    void deleteSessionIfNeeded() {
        if (!networkAccessManager)
            return;
        const QString sid = sessionId_;
        sessionId_.clear();
        if (sid.isEmpty())
            return;
        net().fire(api(QLatin1String("/session")).ykSession(sid).del().timeout(15000));
    }

    void abortPending() {
        if (pendingReply_) {
            pendingReply_->abort();
            pendingReply_->deleteLater();
            pendingReply_ = nullptr;
        }
    }

    void setBusy(const QString &text) {
        loading_ = true;
        statusLabel_->setText(text);
        addBtn_->setEnabled(false);
        clearBtn_->setEnabled(false);
    }

    void setIdle(const QString &text) {
        loading_ = false;
        statusLabel_->setText(text);
        addBtn_->setEnabled(!sessionId_.isEmpty());
        clearBtn_->setEnabled(!sessionId_.isEmpty());
    }

    void startSession() {
        abortPending();
        deleteSessionIfNeeded();
        tree_->clear();
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
                onSessionCreateFinished(r);
            });
    }

    void onSessionCreateFinished(const HttpUtil::Result &r) {
        if (r.canceled())
            return;
        if (!r.ok()) {
            setIdle(QStringLiteral("创建会话失败：%1").arg(r.errorString));
            pendingLoadAfterSession_ = false;
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(r.bytes);
        sessionId_ = doc.object()
                         .value(QStringLiteral("value"))
                         .toObject()
                         .value(QStringLiteral("sessionId"))
                         .toString();
        if (sessionId_.isEmpty()) {
            setIdle(QStringLiteral("创建会话失败：响应中无 sessionId"));
            pendingLoadAfterSession_ = false;
            return;
        }

        if (pendingLoadAfterSession_) {
            pendingLoadAfterSession_ = false;
            startLoad();
            return;
        }

        setIdle(QStringLiteral("会话已就绪，点击「加载」查看配置"));
    }

    [[nodiscard]] QString currentNamespace() const {
        return namespaceEdit_->text().trimmed();
    }

    [[nodiscard]] bool isNamespaceValid(const QString &ns) const {
        if (ns.isEmpty())
            return true;
        static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_.-]+$"));
        return re.match(ns).hasMatch();
    }

    [[nodiscard]] QString namespaceDisplayName() const {
        const QString ns = currentNamespace();
        return ns.isEmpty() ? QStringLiteral("当前任务默认分组") : ns;
    }

    [[nodiscard]] QJsonObject requestBodyWithNamespace() const {
        QJsonObject body;
        const QString ns = currentNamespace();
        if (!ns.isEmpty())
            body.insert(QStringLiteral("namespace"), ns);
        return body;
    }

    void startLoad() {
        if (loading_)
            return;

        const QString ns = currentNamespace();
        if (!isNamespaceValid(ns)) {
            QMessageBox::warning(this, QStringLiteral("namespace 无效"),
                QStringLiteral("namespace 只能包含字母、数字、下划线、中划线和点。"));
            return;
        }
        if (sessionId_.isEmpty()) {
            pendingLoadAfterSession_ = true;
            startSession();
            return;
        }

        abortPending();
        tree_->clear();
        setBusy(QStringLiteral("正在加载配置…"));

        QJsonDocument doc(requestBodyWithNamespace());
        pendingReply_ = net().submit(
            apiSes(QLatin1String("/prefs/all")).postJson(doc.toJson(QJsonDocument::Compact)).timeout(30000),
            [this](const HttpUtil::Result &r) {
                if (pendingReply_ == r.reply)
                    pendingReply_ = nullptr;
                onLoadFinished(r);
            });
    }

    void onLoadFinished(const HttpUtil::Result &r) {
        if (r.canceled())
            return;
        if (!r.ok()) {
            setIdle(QStringLiteral("加载失败：%1").arg(r.errorString));
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(r.bytes);
        const QJsonValue value = extractResponseValue(doc.object());
        if (value.isObject() && value.toObject().contains(QStringLiteral("error"))) {
            setIdle(value.toObject().value(QStringLiteral("message")).toString());
            return;
        }
        if (!value.isObject()) {
            setIdle(QStringLiteral("加载失败：响应格式不正确"));
            return;
        }

        populateTree(value.toObject());
        setIdle(QStringLiteral("共 %1 个配置项").arg(tree_->topLevelItemCount()));
        applyFilter(filterEdit_->text());
    }

    [[nodiscard]] static QJsonValue extractResponseValue(const QJsonObject &root) {
        return root.value(QStringLiteral("value"));
    }

    void populateTree(const QJsonObject &prefs) {
        tree_->clear();
        for (auto it = prefs.constBegin(); it != prefs.constEnd(); ++it) {
            auto *item = makeKeyItem(it.key(), it.value());
            tree_->addTopLevelItem(item);
        }
        tree_->sortItems(0, Qt::AscendingOrder);
    }

    [[nodiscard]] QTreeWidgetItem *makeKeyItem(const QString &key, const QJsonValue &value) {
        auto *item = new QTreeWidgetItem({key, jsonValueText(value), jsonTypeName(value)});
        item->setData(0, Qt::UserRole, static_cast<int>(NodeKind::Key));
        item->setData(0, Qt::UserRole + 1, key);
        item->setData(0, Qt::UserRole + 2, value);
        appendFieldChildren(item, value);
        return item;
    }

    void appendFieldChildren(QTreeWidgetItem *parent, const QJsonValue &value) {
        if (value.isObject()) {
            const QJsonObject obj = value.toObject();
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                auto *child = new QTreeWidgetItem(
                    {it.key(), jsonValueText(it.value()), jsonTypeName(it.value())});
                child->setData(0, Qt::UserRole, static_cast<int>(NodeKind::Field));
                parent->addChild(child);
                appendFieldChildren(child, it.value());
            }
            return;
        }
        if (value.isArray()) {
            const QJsonArray arr = value.toArray();
            for (int i = 0; i < arr.size(); ++i) {
                auto *child = new QTreeWidgetItem(
                    {QStringLiteral("[%1]").arg(i), jsonValueText(arr.at(i)), jsonTypeName(arr.at(i))});
                child->setData(0, Qt::UserRole, static_cast<int>(NodeKind::Field));
                parent->addChild(child);
                appendFieldChildren(child, arr.at(i));
            }
        }
    }

    [[nodiscard]] static QString jsonTypeName(const QJsonValue &v) {
        if (v.isNull())
            return QStringLiteral("null");
        if (v.isBool())
            return QStringLiteral("boolean");
        if (v.isDouble())
            return QStringLiteral("number");
        if (v.isString())
            return QStringLiteral("string");
        if (v.isArray())
            return QStringLiteral("array");
        if (v.isObject())
            return QStringLiteral("object");
        return QStringLiteral("unknown");
    }

    [[nodiscard]] static QString jsonValueText(const QJsonValue &v) {
        if (v.isObject())
            return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
        if (v.isArray())
            return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
        if (v.isBool())
            return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        if (v.isDouble())
            return QString::number(v.toDouble());
        if (v.isNull())
            return QStringLiteral("null");
        return v.toString();
    }

    [[nodiscard]] QTreeWidgetItem *selectedKeyItem() const {
        QTreeWidgetItem *item = tree_->currentItem();
        while (item) {
            if (static_cast<NodeKind>(item->data(0, Qt::UserRole).toInt()) == NodeKind::Key)
                return item;
            item = item->parent();
        }
        return nullptr;
    }

    void applyFilter(const QString &text) {
        const QString needle = text.trimmed();
        const int topCount = tree_->topLevelItemCount();
        for (int i = 0; i < topCount; ++i) {
            auto *item = tree_->topLevelItem(i);
            const bool match = needle.isEmpty() || item->text(0).contains(needle, Qt::CaseInsensitive);
            item->setHidden(!match);
        }
    }

    void showTreeMenu(const QPoint &pos) {
        QTreeWidgetItem *item = tree_->itemAt(pos);
        if (!item)
            return;

        QMenu menu(this);
        if (static_cast<NodeKind>(item->data(0, Qt::UserRole).toInt()) == NodeKind::Key) {
            menu.addAction(QStringLiteral("编辑"), this, &PrefsConfigEditorDialog::editSelectedKey);
            menu.addAction(QStringLiteral("删除"), this, &PrefsConfigEditorDialog::deleteSelectedKey);
            menu.addSeparator();
        }
        menu.addAction(QStringLiteral("复制键"), [item]() { qApp->clipboard()->setText(item->text(0)); });
        menu.addAction(QStringLiteral("复制值"), [item]() { qApp->clipboard()->setText(item->text(1)); });
        menu.exec(tree_->viewport()->mapToGlobal(pos));
    }

    void addKey() {
        if (!isNamespaceValid(currentNamespace())) {
            QMessageBox::warning(this, QStringLiteral("namespace 无效"),
                QStringLiteral("namespace 只能包含字母、数字、下划线、中划线和点。"));
            return;
        }

        bool ok = false;
        const QString key = QInputDialog::getText(this, QStringLiteral("新增配置"), QStringLiteral("键名:"),
            QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || key.isEmpty())
            return;

        const std::optional<QJsonValue> value = promptEditValue(QJsonValue(), QStringLiteral("string"));
        if (!value)
            return;

        saveKeyValue(key, *value);
    }

    void editSelectedKey() {
        QTreeWidgetItem *item = selectedKeyItem();
        if (!item)
            return;

        const QString key = item->data(0, Qt::UserRole + 1).toString();
        const QJsonValue current = item->data(0, Qt::UserRole + 2).toJsonValue();
        const std::optional<QJsonValue> value = promptEditValue(current, jsonTypeName(current));
        if (!value)
            return;

        saveKeyValue(key, *value);
    }

    void deleteSelectedKey() {
        QTreeWidgetItem *item = selectedKeyItem();
        if (!item)
            return;

        const QString key = item->data(0, Qt::UserRole + 1).toString();
        if (QMessageBox::question(this, QStringLiteral("删除配置"),
                QStringLiteral("确定删除键「%1」？").arg(key)) != QMessageBox::Yes)
            return;

        QJsonObject body = requestBodyWithNamespace();
        body.insert(QStringLiteral("key"), key);
        postSimple("/prefs/delete", body, QStringLiteral("删除成功"), true);
    }

    void clearNamespace() {
        if (!isNamespaceValid(currentNamespace()))
            return;
        if (QMessageBox::warning(this, QStringLiteral("清空分组"),
                QStringLiteral("将清空「%1」下的全部配置，此操作不可恢复。是否继续？")
                    .arg(namespaceDisplayName()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
            != QMessageBox::Yes)
            return;

        postSimple("/prefs/clear", requestBodyWithNamespace(), QStringLiteral("已清空分组"), true);
    }

    void saveKeyValue(const QString &key, const QJsonValue &value) {
        QJsonObject body = requestBodyWithNamespace();
        body.insert(QStringLiteral("key"), key);
        body.insert(QStringLiteral("value"), value);
        postSimple("/prefs/set", body, QStringLiteral("保存成功"), true);
    }

    void postSimple(const char *path, const QJsonObject &body, const QString &successHint, bool reload) {
        if (sessionId_.isEmpty()) {
            setIdle(QStringLiteral("会话未就绪"));
            return;
        }
        if (loading_)
            return;

        abortPending();
        setBusy(QStringLiteral("正在提交…"));

        const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
        pendingReply_ = net().submit(
            apiSes(QLatin1String(path)).postJson(payload).timeout(30000),
            [=](const HttpUtil::Result &r) {
                if (pendingReply_ == r.reply)
                    pendingReply_ = nullptr;
                if (r.canceled())
                    return;
                if (!r.ok()) {
                    setIdle(QStringLiteral("操作失败：%1").arg(r.errorString));
                    return;
                }

                const QJsonDocument doc = QJsonDocument::fromJson(r.bytes);
                const QJsonValue value = extractResponseValue(doc.object());
                if (value.isObject() && value.toObject().contains(QStringLiteral("error"))) {
                    setIdle(value.toObject().value(QStringLiteral("message")).toString());
                    return;
                }

                if (reload)
                    startLoad();
                else
                    setIdle(successHint);
            });
    }

    enum class ValueEditorPage { String, Number, Boolean, Null, Json };

    [[nodiscard]] static ValueEditorPage editorPageForType(const QString &type) {
        if (type == QStringLiteral("number"))
            return ValueEditorPage::Number;
        if (type == QStringLiteral("boolean"))
            return ValueEditorPage::Boolean;
        if (type == QStringLiteral("null"))
            return ValueEditorPage::Null;
        if (type == QStringLiteral("object") || type == QStringLiteral("array"))
            return ValueEditorPage::Json;
        return ValueEditorPage::String;
    }

    void populateValueEditor(QLineEdit *stringEdit, QLineEdit *numberEdit, QCheckBox *boolCheck,
        QTextEdit *jsonEdit, const QJsonValue &current, const QString &type) {
        switch (editorPageForType(type)) {
        case ValueEditorPage::String:
            stringEdit->setText(current.isString() ? current.toString() : QString());
            break;
        case ValueEditorPage::Number:
            numberEdit->setText(current.isDouble() ? QString::number(current.toDouble()) : QStringLiteral("0"));
            break;
        case ValueEditorPage::Boolean:
            boolCheck->setChecked(current.isBool() ? current.toBool() : true);
            break;
        case ValueEditorPage::Null:
            break;
        case ValueEditorPage::Json:
            if (type == QStringLiteral("object") && current.isObject()) {
                jsonEdit->setPlainText(QString::fromUtf8(
                    QJsonDocument(current.toObject()).toJson(QJsonDocument::Indented)));
            } else if (type == QStringLiteral("array") && current.isArray()) {
                jsonEdit->setPlainText(QString::fromUtf8(
                    QJsonDocument(current.toArray()).toJson(QJsonDocument::Indented)));
            } else {
                jsonEdit->setPlainText(type == QStringLiteral("array") ? QStringLiteral("[]")
                                                                       : QStringLiteral("{}"));
            }
            break;
        }
    }

    [[nodiscard]] std::optional<QJsonValue> collectEditedValue(const QString &type, QLineEdit *stringEdit,
        QLineEdit *numberEdit, QCheckBox *boolCheck, QTextEdit *jsonEdit, QWidget *dialogParent) const {
        if (type == QStringLiteral("null"))
            return QJsonValue(QJsonValue::Null);
        if (type == QStringLiteral("boolean"))
            return boolCheck->isChecked();
        if (type == QStringLiteral("number")) {
            const QString trimmed = numberEdit->text().trimmed();
            bool ok = false;
            const double num = trimmed.toDouble(&ok);
            if (!ok || trimmed.isEmpty()) {
                QMessageBox::warning(dialogParent, QStringLiteral("格式错误"),
                    QStringLiteral("请输入有效数字"));
                return std::nullopt;
            }
            return num;
        }
        if (type == QStringLiteral("object") || type == QStringLiteral("array")) {
            const QString trimmed = jsonEdit->toPlainText().trimmed();
            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError) {
                QMessageBox::warning(dialogParent, QStringLiteral("格式错误"),
                    QStringLiteral("JSON 格式不正确：%1").arg(err.errorString()));
                return std::nullopt;
            }
            if (type == QStringLiteral("object")) {
                if (!doc.isObject()) {
                    QMessageBox::warning(dialogParent, QStringLiteral("格式错误"),
                        QStringLiteral("object 必须是 JSON 对象"));
                    return std::nullopt;
                }
                return doc.object();
            }
            if (!doc.isArray()) {
                QMessageBox::warning(dialogParent, QStringLiteral("格式错误"),
                    QStringLiteral("array 必须是 JSON 数组"));
                return std::nullopt;
            }
            return doc.array();
        }
        return stringEdit->text();
    }

    [[nodiscard]] static QString editValueDialogStyleSheet() {
        const bool isDark = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        const QString bg = isDark ? QStringLiteral("#121212") : QStringLiteral("#F4F5F7");
        const QString card = isDark ? QStringLiteral("#1E1E1E") : QStringLiteral("#FFFFFF");
        const QString border = isDark ? QStringLiteral("#343841") : QStringLiteral("#D8DEE8");
        const QString text = isDark ? QStringLiteral("#E8ECF1") : QStringLiteral("#1F2937");
        const QString muted = isDark ? QStringLiteral("#9AA3B2") : QStringLiteral("#6B7280");
        const QString field = isDark ? QStringLiteral("#151515") : QStringLiteral("#FAFBFC");
        const QString accent = isDark ? QStringLiteral("#4C8DFF") : QStringLiteral("#2563EB");

        return QStringLiteral(
                   "QDialog#PrefsValueEditDialog { background-color: %1; }"
                   "QFrame#PrefsValueCard { background-color: %2; border: 1px solid %3; border-radius: 10px; }"
                   "QLabel#PrefsValueTitle { color: %4; font-size: 15px; font-weight: 600; }"
                   "QLabel#PrefsValueHint { color: %5; font-size: 12px; }"
                   "QLabel#PrefsFieldLabel { color: %5; font-size: 12px; min-width: 36px; }"
                   "QComboBox, QLineEdit, QTextEdit {"
                   "  background-color: %6; color: %4; border: 1px solid %3; border-radius: 6px; padding: 7px 10px;"
                   "}"
                   "QComboBox:focus, QLineEdit:focus, QTextEdit:focus { border-color: %7; }"
                   "QCheckBox { color: %4; spacing: 8px; padding: 4px 0; }"
                   "QDialogButtonBox QPushButton { min-width: 72px; padding: 6px 14px; border-radius: 6px; }"
                   "QDialogButtonBox QPushButton#PrefsOkButton {"
                   "  background-color: %7; color: white; border: 1px solid %7;"
                   "}")
            .arg(bg, card, border, text, muted, field, accent);
    }

    [[nodiscard]] std::optional<QJsonValue> promptEditValue(const QJsonValue &current, const QString &typeHint) {
        QDialog dialog(this);
        dialog.setObjectName(QStringLiteral("PrefsValueEditDialog"));
        dialog.setWindowTitle(QStringLiteral("编辑值"));
        dialog.setMinimumWidth(460);
        dialog.setStyleSheet(editValueDialogStyleSheet());

        auto *layout = new QVBoxLayout(&dialog);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        auto *card = new QFrame(&dialog);
        card->setObjectName(QStringLiteral("PrefsValueCard"));
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 16, 16, 16);
        cardLayout->setSpacing(14);

        auto *title = new QLabel(QStringLiteral("编辑配置值"), card);
        title->setObjectName(QStringLiteral("PrefsValueTitle"));
        cardLayout->addWidget(title);

        auto *typeRow = new QHBoxLayout();
        auto *typeLabel = new QLabel(QStringLiteral("类型"), card);
        typeLabel->setObjectName(QStringLiteral("PrefsFieldLabel"));
        auto *typeBox = new QComboBox(card);
        typeBox->addItems({QStringLiteral("string"), QStringLiteral("number"), QStringLiteral("boolean"),
            QStringLiteral("null"), QStringLiteral("object"), QStringLiteral("array")});
        const int typeIndex = typeBox->findText(typeHint);
        typeBox->setCurrentIndex(typeIndex >= 0 ? typeIndex : 0);
        typeRow->addWidget(typeLabel);
        typeRow->addWidget(typeBox, 1);
        cardLayout->addLayout(typeRow);

        auto *valueRow = new QHBoxLayout();
        valueRow->setAlignment(Qt::AlignTop);
        auto *valueLabel = new QLabel(QStringLiteral("值"), card);
        valueLabel->setObjectName(QStringLiteral("PrefsFieldLabel"));
        auto *valueHost = new QWidget(card);
        auto *valueHostLayout = new QVBoxLayout(valueHost);
        valueHostLayout->setContentsMargins(0, 0, 0, 0);
        valueHostLayout->setSpacing(0);

        auto *stringEdit = new QLineEdit(valueHost);
        stringEdit->setClearButtonEnabled(true);
        stringEdit->setPlaceholderText(QStringLiteral("输入字符串"));

        auto *numberEdit = new QLineEdit(valueHost);
        numberEdit->setPlaceholderText(QStringLiteral("输入数字"));
        numberEdit->setValidator(new QDoubleValidator(-1e12, 1e12, 12, numberEdit));

        auto *boolCheck = new QCheckBox(valueHost);
        boolCheck->setChecked(true);
        boolCheck->setToolTip(QStringLiteral("勾选为 true，不勾选为 false"));

        auto *nullPanel = new QWidget(valueHost);
        auto *nullLayout = new QVBoxLayout(nullPanel);
        nullLayout->setContentsMargins(0, 2, 0, 0);
        auto *nullHint = new QLabel(QStringLiteral("null 类型无需填写值。"), nullPanel);
        nullHint->setObjectName(QStringLiteral("PrefsValueHint"));
        nullHint->setWordWrap(true);
        nullLayout->addWidget(nullHint);

        auto *jsonEdit = new QTextEdit(valueHost);
        jsonEdit->setMinimumHeight(160);
        jsonEdit->setPlaceholderText(QStringLiteral("输入 JSON"));
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPointSize(mono.pointSize() > 0 ? mono.pointSize() : 12);
        jsonEdit->setFont(mono);

        valueHostLayout->addWidget(stringEdit);
        valueHostLayout->addWidget(numberEdit);
        valueHostLayout->addWidget(boolCheck);
        valueHostLayout->addWidget(nullPanel);
        valueHostLayout->addWidget(jsonEdit);

        valueRow->addWidget(valueLabel);
        valueRow->addWidget(valueHost, 1);
        cardLayout->addLayout(valueRow);

        layout->addWidget(card);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
        auto *okBtn = buttons->button(QDialogButtonBox::Ok);
        okBtn->setText(QStringLiteral("确定"));
        okBtn->setObjectName(QStringLiteral("PrefsOkButton"));
        okBtn->setAutoDefault(false);
        okBtn->setDefault(false);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

        const auto syncEditor = [&](const QJsonValue &source, const QString &type) {
            const ValueEditorPage page = editorPageForType(type);
            stringEdit->setVisible(page == ValueEditorPage::String);
            numberEdit->setVisible(page == ValueEditorPage::Number);
            boolCheck->setVisible(page == ValueEditorPage::Boolean);
            nullPanel->setVisible(page == ValueEditorPage::Null);
            jsonEdit->setVisible(page == ValueEditorPage::Json);
            valueLabel->setVisible(page != ValueEditorPage::Null);
            populateValueEditor(stringEdit, numberEdit, boolCheck, jsonEdit, source, type);
        };

        connect(typeBox, &QComboBox::currentTextChanged, &dialog,
            [&](const QString &type) { syncEditor(QJsonValue(), type); });

        syncEditor(current, typeBox->currentText());

        if (dialog.exec() != QDialog::Accepted)
            return std::nullopt;

        return collectEditedValue(typeBox->currentText(), stringEdit, numberEdit, boolCheck, jsonEdit, &dialog);
    }

    DeviceInfo *deviceInfo_ = nullptr;
    QUrl baseUrl_;
    QString sessionId_;
    QNetworkReply *pendingReply_ = nullptr;
    bool pendingLoadAfterSession_ = false;
    bool loading_ = false;

    QLineEdit *namespaceEdit_ = nullptr;
    QLineEdit *filterEdit_ = nullptr;
    QPushButton *loadBtn_ = nullptr;
    QPushButton *addBtn_ = nullptr;
    QPushButton *clearBtn_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QLabel *statusLabel_ = nullptr;
};
