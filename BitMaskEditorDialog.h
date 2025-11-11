#pragma once

#include <QDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QVector>
#include <QPushButton>

class BitMaskEditorDialog : public QDialog
{
    Q_OBJECT
public:
    struct Item {
        int bit;
        QString name;
    };

    explicit BitMaskEditorDialog(const QVector<Item>& items,
                                 quint32& maskRef,
                                 QWidget* parent = nullptr)
        : QDialog(parent)
        , m_items(items)
        , m_maskRef(maskRef)
    {
        setWindowTitle(tr("位掩码编辑器"));
        setModal(true);
        setupUi();
    }

private:
    void setupUi()
    {
        auto mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->setSpacing(12);

        auto grid = new QGridLayout;
        grid->setHorizontalSpacing(20);
        grid->setVerticalSpacing(10);
        m_checkBoxes.reserve(m_items.size());

        for (size_t i = 0; i < m_items.size(); ++i) {
            const auto& item = m_items[i];

            auto cb = new QCheckBox(item.name, this);
            cb->setChecked(m_maskRef & (1U << item.bit));
            cb->setStyleSheet("QCheckBox { spacing: 8px; }");
            m_checkBoxes.push_back(cb);

            int col = static_cast<int>(i % 4);
            int row = static_cast<int>(i / 4);
            grid->addWidget(cb, row, col);
        }

        mainLayout->addLayout(grid);
        mainLayout->addStretch();

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        buttonBox->button(QDialogButtonBox::Ok)->setText(tr("确定"));
        buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("取消"));

        mainLayout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &BitMaskEditorDialog::collectMask);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void collectMask()
    {
        m_maskRef = 0;

        for (size_t i = 0; i < m_checkBoxes.size(); ++i) {
            if (m_checkBoxes[i]->isChecked()) {
                int bit = m_items[i].bit;
                m_maskRef |= (1U << bit);
            }
        }
    }

    const QVector<Item> m_items;
    quint32& m_maskRef;
    QVector<QCheckBox*> m_checkBoxes;
};
