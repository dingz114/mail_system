#include "email_view_widget.h"
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QDesktopServices>

EmailViewWidget::EmailViewWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // 滚动区域
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { border: none; background: #FFFFFF; border-radius: 10px; }");

    QWidget* inner = new QWidget();
    inner->setStyleSheet("background: #FFFFFF;");
    QVBoxLayout* inner_layout = new QVBoxLayout(inner);
    inner_layout->setSpacing(16);

    // 头部信息
    header_label_ = new QLabel(this);
    header_label_->setWordWrap(true);
    header_label_->setStyleSheet(
        "QLabel {"
        "  background: #F8FAFC;"
        "  padding: 20px 24px;"
        "  border-bottom: 2px solid #E4E7ED;"
        "  margin: 0;"
        "}");
    inner_layout->addWidget(header_label_);

    // 正文
    body_browser_ = new QTextBrowser(this);
    body_browser_->setOpenExternalLinks(true);
    body_browser_->setReadOnly(true);
    body_browser_->setStyleSheet(
        "QTextBrowser {"
        "  border: none;"
        "  padding: 16px 24px;"
        "  font-size: 15px;"
        "  line-height: 1.6;"
        "  color: #333333;"
        "  background: #FFFFFF;"
        "}");
    inner_layout->addWidget(body_browser_, 1);

    // 附件区域
    attachments_widget_ = new QWidget(this);
    QVBoxLayout* att_layout = new QVBoxLayout(attachments_widget_);
    att_layout->setContentsMargins(16, 0, 16, 16);
    inner_layout->addWidget(attachments_widget_);

    scroll->setWidget(inner);
    layout->addWidget(scroll);

    clear();
}

void EmailViewWidget::show_email(const Email& email) {
    auto escape = [](const std::string& s) {
        return QString::fromStdString(s).toHtmlEscaped();
    };

    QString header_html;
    header_html += "<table style='width:100%; border-collapse:collapse;'>";

    // 主题
    header_html += "<tr><td colspan='2' style='font-size:20px; font-weight:700; color:#1A1A1A; padding-bottom:16px;'>"
                   + escape(email.subject.empty() ? "(无主题)" : email.subject)
                   + "</td></tr>";

    // 发件人
    QString from_text = email.sender_name.empty()
        ? QString::fromStdString(email.sender_addr)
        : QString::fromStdString(email.sender_name + " <" + email.sender_addr + ">");
    header_html += "<tr><td style='width:56px; color:#8C8C8C; padding:4px 0;'>"
                   + QStringLiteral("发件人") + "</td>"
                   + "<td style='color:#1A1A1A;'>" + escape(from_text.toStdString()) + "</td></tr>";

    // 收件人
    header_html += "<tr><td style='color:#8C8C8C; padding:4px 0;'>"
                   + QStringLiteral("收件人") + "</td>"
                   + "<td style='color:#1A1A1A;'>" + escape(Email::join_recipients(email.to)) + "</td></tr>";

    // 抄送
    if (!email.cc.empty()) {
        header_html += "<tr><td style='color:#8C8C8C; padding:4px 0;'>"
                       + QStringLiteral("抄　送") + "</td>"
                       + "<td style='color:#1A1A1A;'>" + escape(Email::join_recipients(email.cc)) + "</td></tr>";
    }

    // 日期
    header_html += "<tr><td style='color:#8C8C8C; padding:4px 0;'>"
                   + QStringLiteral("时　间") + "</td>"
                   + "<td style='color:#999;'>" + escape(email.received_date) + "</td></tr>";

    header_html += "</table>";

    header_label_->setText(header_html);

    // 正文
    if (!email.body_html.empty()) {
        body_browser_->setHtml(QString::fromStdString(email.body_html));
    } else {
        QString plain = QString::fromStdString(email.body_plain).toHtmlEscaped();
        plain.replace("\n", "<br>");
        body_browser_->setHtml("<div style='font-size:15px; line-height:1.8;'>" + plain + "</div>");
    }

    // 附件
    QLayout* att_layout = attachments_widget_->layout();
    if (att_layout) {
        QLayoutItem* child;
        while ((child = att_layout->takeAt(0)) != nullptr) {
            delete child->widget();
            delete child;
        }
    }

    if (email.has_attachments && !email.attachments.empty()) {
        QLabel* divider = new QLabel(attachments_widget_);
        divider->setStyleSheet("border-top: 1px solid #F0F0F0; margin: 8px 0;");
        att_layout->addWidget(divider);

        QLabel* att_header = new QLabel(QStringLiteral("📎 附件（%1 个）").arg(email.attachments.size()),
                                       attachments_widget_);
        att_header->setStyleSheet("font-weight: 600; color: #555; padding: 4px 0;");
        att_layout->addWidget(att_header);

        for (const auto& att : email.attachments) {
            QFrame* item = new QFrame(attachments_widget_);
            item->setStyleSheet(
                "QFrame {"
                "  background: #F8FAFC;"
                "  border: 1px solid #EBEEF5;"
                "  border-radius: 8px;"
                "  padding: 12px 16px;"
                "  margin: 4px 0;"
                "}");
            QHBoxLayout* item_layout = new QHBoxLayout(item);
            item_layout->setContentsMargins(0, 0, 0, 0);

            QString icon = QStringLiteral("📄");
            QString ext = QString::fromStdString(att.file_name).section('.', -1).toLower();
            if (ext == "pdf") icon = QStringLiteral("📕");
            else if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif") icon = QStringLiteral("🖼️");
            else if (ext == "zip" || ext == "rar" || ext == "7z") icon = QStringLiteral("📦");
            else if (ext == "doc" || ext == "docx") icon = QStringLiteral("📝");

            QLabel* icon_label = new QLabel(icon, item);
            icon_label->setStyleSheet("font-size: 20px; border: none; background: transparent;");
            item_layout->addWidget(icon_label);

            QLabel* name_label = new QLabel(QString::fromStdString(att.file_name), item);
            name_label->setStyleSheet("font-weight: 500; color: #333; border: none; background: transparent;");
            item_layout->addWidget(name_label);

            item_layout->addStretch();

            QString size_text;
            if (att.file_size > 1024 * 1024)
                size_text = QString::number(att.file_size / (1024.0 * 1024.0), 'f', 1) + " MB";
            else
                size_text = QString::number(att.file_size / 1024.0, 'f', 0) + " KB";

            QLabel* size_label = new QLabel(size_text, item);
            size_label->setStyleSheet("color: #999; font-size: 12px; border: none; background: transparent;");
            item_layout->addWidget(size_label);

            att_layout->addWidget(item);
        }
    }
}

void EmailViewWidget::clear() {
    header_label_->setText(
        "<div style='text-align:center; padding:60px 0; color:#CCC;'>"
        "<div style='font-size:48px; margin-bottom:12px;'>📬</div>"
        "<div style='font-size:14px;'>" + QStringLiteral("选择一封邮件查看详情") + "</div>"
        "</div>");
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
