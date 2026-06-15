#include "email_view_widget.h"
#include <QScrollArea>
#include <QFrame>

EmailViewWidget::EmailViewWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    // Header section
    header_label_ = new QLabel(this);
    header_label_->setWordWrap(true);
    header_label_->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    header_label_->setStyleSheet(
        "QLabel { background-color: #f5f5f5; padding: 8px; border: 1px solid #ddd; border-radius: 4px; }");
    layout->addWidget(header_label_);

    // Body browser
    body_browser_ = new QTextBrowser(this);
    body_browser_->setOpenExternalLinks(true);
    body_browser_->setReadOnly(true);
    layout->addWidget(body_browser_, 1);

    // Attachments section
    attachments_widget_ = new QWidget(this);
    QVBoxLayout* att_layout = new QVBoxLayout(attachments_widget_);
    att_layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(attachments_widget_);

    clear();
}

void EmailViewWidget::show_email(const Email& email) {
    // Build header HTML
    QString header_html;
    header_html += "<table style='width:100%; font-size:13px;'>";
    header_html += "<tr><td style='font-weight:bold; width:60px;'>" + QString::fromUtf8("From:") + "</td><td>"
                   + QString::fromStdString(email.sender_name.empty() ? email.sender_addr
                                            : email.sender_name + " <" + email.sender_addr + ">")
                   + "</td></tr>";
    header_html += "<tr><td style='font-weight:bold;'>" + QString::fromUtf8("To:") + "</td><td>"
                   + QString::fromStdString(Email::join_recipients(email.to))
                   + "</td></tr>";
    if (!email.cc.empty()) {
        header_html += "<tr><td style='font-weight:bold;'>" + QString::fromUtf8("CC:") + "</td><td>"
                       + QString::fromStdString(Email::join_recipients(email.cc))
                       + "</td></tr>";
    }
    header_html += "<tr><td style='font-weight:bold;'>" + QString::fromUtf8("Date:") + "</td><td>"
                   + QString::fromStdString(email.received_date) + "</td></tr>";
    header_html += "<tr><td style='font-weight:bold;'>" + QString::fromUtf8("Subject:") + "</td><td><b>"
                   + QString::fromStdString(email.subject) + "</b></td></tr>";
    header_html += "</table>";

    header_label_->setText(header_html);

    // Render body
    if (!email.body_html.empty()) {
        body_browser_->setHtml(QString::fromStdString(email.body_html));
    } else {
        body_browser_->setPlainText(QString::fromStdString(email.body_plain));
    }

    // Show attachments
    // Clear existing attachment widgets
    QLayout* att_layout = attachments_widget_->layout();
    if (att_layout) {
        QLayoutItem* child;
        while ((child = att_layout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
    }

    if (email.has_attachments && !email.attachments.empty()) {
        QLabel* att_header = new QLabel("<b>" + QString::fromUtf8("Attachments:") + "</b>", attachments_widget_);
        att_layout->addWidget(att_header);

        for (const auto& att : email.attachments) {
            QString att_text = QString::fromUtf8("  📎 ")  // Paperclip emoji
                             + QString::fromStdString(att.file_name)
                             + " (" + QString::number(att.file_size / 1024) + " KB)";
            QLabel* att_label = new QLabel(att_text, attachments_widget_);
            att_layout->addWidget(att_label);
        }
    }
}

void EmailViewWidget::clear() {
    header_label_->setText("");
    body_browser_->clear();

    QLayout* att_layout = attachments_widget_->layout();
    if (att_layout) {
        QLayoutItem* child;
        while ((child = att_layout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
    }
}
