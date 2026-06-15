#include "email_list_widget.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QFont>

EmailListWidget::EmailListWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({
        QString(),
        QStringLiteral("发件人"),
        QStringLiteral("主题"),
        QStringLiteral("日期")
    });

    table_->setColumnWidth(0, 40);
    table_->setColumnWidth(1, 180);
    table_->setColumnWidth(2, 420);
    table_->setColumnWidth(3, 160);

    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(false);
    table_->setFocusPolicy(Qt::NoFocus);
    table_->setMouseTracking(true);

    // 行高
    table_->verticalHeader()->setDefaultSectionSize(44);

    layout->addWidget(table_);

    connect(table_, &QTableWidget::itemSelectionChanged,
            this, &EmailListWidget::on_selection_changed);
}

void EmailListWidget::set_emails(const std::vector<Email>& emails) {
    emails_ = emails;
    table_->setRowCount(0);

    for (size_t i = 0; i < emails_.size(); ++i) {
        const Email& email = emails_[i];
        int row = table_->rowCount();
        table_->insertRow(row);

        bool unread = !email.is_read;

        // 0 — 状态指示图标
        QTableWidgetItem* indicator = new QTableWidgetItem();
        if (email.is_flagged) {
            indicator->setText(QStringLiteral("⭐"));
        } else if (unread) {
            indicator->setText(QStringLiteral("🔵"));
        }
        indicator->setTextAlignment(Qt::AlignCenter);
        table_->setItem(row, 0, indicator);

        // 1 — 发件人
        QString from = email.sender_name.empty()
            ? QString::fromStdString(email.sender_addr)
            : QString::fromStdString(email.sender_name);
        QTableWidgetItem* from_item = new QTableWidgetItem(from);
        from_item->setData(Qt::UserRole, email.id);
        if (unread) {
            QFont f = from_item->font();
            f.setBold(true);
            f.setPointSize(f.pointSize() + 1);
            from_item->setFont(f);
            from_item->setForeground(QColor("#1A1A1A"));
        } else {
            from_item->setForeground(QColor("#666666"));
        }
        table_->setItem(row, 1, from_item);

        // 2 — 主题
        QTableWidgetItem* subject_item = new QTableWidgetItem(
            QString::fromStdString(email.subject.empty() ? "(无主题)" : email.subject));
        if (unread) {
            QFont f = subject_item->font();
            f.setBold(true);
            subject_item->setFont(f);
            subject_item->setForeground(QColor("#1A1A1A"));
        } else {
            subject_item->setForeground(QColor("#888888"));
        }
        table_->setItem(row, 2, subject_item);

        // 3 — 日期
        QTableWidgetItem* date_item = new QTableWidgetItem(
            QString::fromStdString(email.received_date));
        date_item->setForeground(QColor("#AAAAAA"));
        date_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table_->setItem(row, 3, date_item);

        // 交替行背景色
        if (row % 2 == 1) {
            QColor alt("#FAFBFC");
            for (int c = 0; c < 4; ++c) {
                if (table_->item(row, c))
                    table_->item(row, c)->setBackground(alt);
            }
        }
    }
}

int EmailListWidget::current_email_id() const {
    int row = table_->currentRow();
    if (row >= 0 && row < (int)emails_.size()) {
        return emails_[row].id;
    }
    return -1;
}

void EmailListWidget::clear() {
    emails_.clear();
    table_->setRowCount(0);
}

void EmailListWidget::on_selection_changed() {
    int id = current_email_id();
    if (id >= 0) {
        emit email_selected(id);
    }
}
