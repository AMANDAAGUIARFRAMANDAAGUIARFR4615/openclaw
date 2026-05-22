#pragma once

#include "global.h"
#include "Tools.h"
#include "SettingsViewer.h"
#include "AccountListDialog.h"
#include "FlowEditorDialog.h"
#include <QMenu>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QApplication>
#include <QClipboard>

class DeveloperTools {
public:
    static void showMenu(QWidget *parent, const QPoint &globalPos) {
        QMenu menu(parent);
        menu.addAction(QStringLiteral("数据查看"), [=]() {
            SettingsViewer(settings, parent).exec();
        });
        menu.addAction(QStringLiteral("可视化编程"), [=]() {
            FlowEditorDialog(parent).exec();
        });
        menu.addAction(QStringLiteral("兑换码生成"), [=]() {
            showGenerateCodesDialog(parent);
        });
        menu.addAction(QStringLiteral("禁用兑换码"), [=]() {
            showDisableCodesDialog(parent);
        });
        menu.addAction(QStringLiteral("在线用户"), [=]() {
            showOnlineAccounts(parent);
        });
        menu.exec(globalPos);
    }

private:
    static void showGenerateCodesDialog(QWidget *parent) {
        QDialog dialog(parent);
        dialog.setWindowTitle(QStringLiteral("生成兑换码"));
        dialog.setMinimumWidth(350);

        auto *formLayout = new QFormLayout(&dialog);

        auto *phoneEdit = new QLineEdit(&dialog);
        phoneEdit->setPlaceholderText(QStringLiteral("请输入目标手机号"));

        auto *typeLayout = new QHBoxLayout();
        auto *rbWeekly = new QRadioButton(QStringLiteral("周卡"), &dialog);
        auto *rbMonthly = new QRadioButton(QStringLiteral("月卡"), &dialog);
        auto *rbQuarterly = new QRadioButton(QStringLiteral("季卡"), &dialog);
        auto *rbAnnual = new QRadioButton(QStringLiteral("年卡"), &dialog);

        typeLayout->addWidget(rbWeekly);
        typeLayout->addWidget(rbMonthly);
        typeLayout->addWidget(rbQuarterly);
        typeLayout->addWidget(rbAnnual);

        auto *typeGroup = new QButtonGroup(&dialog);
        typeGroup->addButton(rbWeekly, 1);
        typeGroup->addButton(rbMonthly, 2);
        typeGroup->addButton(rbQuarterly, 3);
        typeGroup->addButton(rbAnnual, 4);
        rbMonthly->setChecked(true);

        auto *countSpin = new QSpinBox(&dialog);
        countSpin->setRange(1, 1000);
        countSpin->setValue(1);

        formLayout->addRow(QStringLiteral("手机号:"), phoneEdit);
        formLayout->addRow(QStringLiteral("类型:"), typeLayout);
        formLayout->addRow(QStringLiteral("生成数量:"), countSpin);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        formLayout->addRow(buttonBox);

        if (dialog.exec() != QDialog::Accepted)
            return;

        const QString inputPhone = phoneEdit->text().trimmed();
        const int inputCount = countSpin->value();
        const int inputType = typeGroup->checkedId();

        webSocketClient->emitEvent(
            "generate_codes",
            QJsonObject{{"phone", inputPhone}, {"count", inputCount}, {"type", inputType}},
            [=](const QJsonValue &res) {
                if (res.isString()) {
                    Tools::showToast(res.toString(), parent);
                    return;
                }

                QStringList codes;
                if (res.isArray()) {
                    for (const QJsonValue &item : res.toArray())
                        codes << item.toString();
                }

                qApp->clipboard()->setText(codes.join('\n'));
                Tools::showToast(QStringLiteral("成功生成 %1 个兑换码并复制").arg(codes.size()), parent);
            });
    }

    static void showDisableCodesDialog(QWidget *parent) {
        QDialog dialog(parent);
        dialog.setWindowTitle(QStringLiteral("禁用兑换码"));
        dialog.setMinimumWidth(400);

        auto *layout = new QVBoxLayout(&dialog);
        auto *hint = new QLabel(QStringLiteral("每行一个兑换码。已兑换的码将自动回滚：\n"
            "· 用于设备续费 → 扣减对应天数\n"
            "· 用于余额充值 → 扣回面值余额"), &dialog);
        hint->setWordWrap(true);

        auto *codesEdit = new QPlainTextEdit(&dialog);
        codesEdit->setPlaceholderText(QStringLiteral("粘贴要禁用的兑换码..."));
        codesEdit->setMinimumHeight(160);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(hint);
        layout->addWidget(codesEdit);
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() != QDialog::Accepted)
            return;

        const QString text = codesEdit->toPlainText().trimmed();
        if (text.isEmpty()) {
            Tools::showToast(QStringLiteral("请输入兑换码"), parent);
            return;
        }

        QStringList codes = text.split(QRegularExpression(QStringLiteral("[\\r\\n,;\\s]+")), Qt::SkipEmptyParts);
        for (QString &c : codes)
            c = c.trimmed();
        codes.removeDuplicates();

        if (codes.isEmpty()) {
            Tools::showToast(QStringLiteral("请输入兑换码"), parent);
            return;
        }

        webSocketClient->emitEvent("disable_redeem_codes", QJsonArray::fromStringList(codes),
            [=](const QJsonValue &res) {
                if (res.isString()) {
                    Tools::showToast(res.toString(), parent);
                    return;
                }

                const QJsonObject obj = res.toObject();
                const QString summary = obj[QStringLiteral("msg")].toString();
                const QJsonArray results = obj[QStringLiteral("results")].toArray();

                QStringList lines;
                for (const QJsonValue &item : results) {
                    const QJsonObject row = item.toObject();
                    const QString code = row[QStringLiteral("code")].toString();
                    const bool ok = row[QStringLiteral("ok")].toBool();
                    const QString msg = row[QStringLiteral("msg")].toString();
                    lines << QStringLiteral("%1 %2 — %3")
                                 .arg(ok ? QStringLiteral("✓") : QStringLiteral("✗"), code, msg);
                }

                QMessageBox box(parent);
                box.setWindowTitle(QStringLiteral("禁用结果"));
                box.setText(summary);
                box.setDetailedText(lines.join('\n'));
                box.setIcon(results.isEmpty() ? QMessageBox::Warning : QMessageBox::Information);
                box.exec();
            });
    }

    static void showOnlineAccounts(QWidget *parent) {
        webSocketClient->emitEvent("online_accounts", QJsonValue(), [=](const QJsonValue &res) {
            if (res.isString()) {
                Tools::showToast(res.toString(), parent);
                return;
            }

            QStringList phoneNumbers;
            for (const QJsonValue &item : res.toArray())
                phoneNumbers << item.toString();

            if (phoneNumbers.isEmpty()) {
                Tools::showToast(QStringLiteral("没有在线账号"), parent);
                return;
            }

            AccountListDialog(phoneNumbers, parent).exec();
        });
    }
};
