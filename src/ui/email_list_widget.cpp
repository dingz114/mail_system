#include "email_list_widget.h"
#include <QVBoxLayout>
#include <QHeaderView>

EmailListWidget::EmailListWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({
        QString::fromUtf8(""),
        QString::fromUtf8("From"),
        QString::fromUtf8("Subject"),
        QString::fromUtf8("Date")
    });

    // Column widths
    table_->setColumnWidth(0, 30);   // Star/unread indicator
    table_->setColumnWidth(1, 180);  // From
    table_->setColumnWidth(2, 400);  // Subject
    table_->setColumnWidth(3, 150);  // Date

    table_->horizontalHeader()->setStretchLastSection(true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->verticalHeader()->setVisible(false);
    table_->setShowGrid(false);
    table_->setAlternatingRowColors(true);

    layout->addWidget(table_);

    connect(table_, &QTableWidget::itemSelectionChanged, this, &EmailListWidget::on_selection_changed);
}

void EmailListWidget::set_emails(const std::vector<Email>& emails) {
    emails_ = emails;
    table_->setRowCount(0);

    for (size_t i = 0; i < emails_.size(); ++i) {
        const Email& email = emails_[i];
        int row = table_->rowCount();
        table_->insertRow(row);

        // Star/unread indicator
        QTableWidgetItem* indicator = new QTableWidgetItem();
        if (email.is_flagged) {
            indicator->setText(QString::fromUtf8("★"));  // Star
        } else if (!email.is_read) {
            indicator->setText(QString::fromUtf8("●"));  // Dot
        }
        table_->setItem(row, 0, indicator);

        // From
        QString from;
        if (!email.sender_name.empty()) {
            from = QString::fromStdString(email.sender_name);
        } else {
            from = QString::fromStdString(email.sender_addr);
        }
        QTableWidgetItem* from_item = new QTableWidgetItem(from);
        if (!email.is_read) {
            QFont bold_font = from_item->font();
            bold_font.setBold(true);
            from_item->setFont(bold_font);
            indicator->setFont(bold_font);
        }
        from_item->setData(Qt::UserRole, email.id);
        table_->setItem(row, 1, from_item);

        // Subject
        QTableWidgetItem* subject_item = new QTableWidgetItem(QString::fromStdString(email.subject));
        if (!email.is_read) {
            QFont bold_font = subject_item->font();
            bold_font.setBold(true);
            subject_item->setFont(bold_font);
        }
        table_->setItem(row, 2, subject_item);

        // Date
        QTableWidgetItem* date_item = new QTableWidgetItem(QString::fromStdString(email.received_date));
        table_->setItem(row, 3, date_item);
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
