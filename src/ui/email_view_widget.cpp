#include "email_view_widget.h"
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QFrame>
#include <QFont>
#include <QDesktopServices>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>
#include <functional>

namespace {
QString replace_regex(QString input,
                      const QRegularExpression& re,
                      const std::function<QString(const QRegularExpressionMatch&)>& replacer) {
    QString result;
    int offset = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(input);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        result += input.mid(offset, match.capturedStart() - offset);
        result += replacer(match);
        offset = match.capturedEnd();
    }
    result += input.mid(offset);
    return result;
}

QString normalize_cid(QString cid) {
    cid = QUrl::fromPercentEncoding(cid.trimmed().toUtf8()).trimmed();
    if (cid.startsWith('<') && cid.endsWith('>') && cid.size() > 2) {
        cid = cid.mid(1, cid.size() - 2).trimmed();
    }
    if (cid.startsWith(QStringLiteral("cid:"), Qt::CaseInsensitive)) {
        cid = cid.mid(4).trimmed();
    }
    return cid;
}

QString add_image_constraints(QString html) {
    if (!html.contains(QStringLiteral("<style"), Qt::CaseInsensitive)) {
        html.prepend(QStringLiteral(
            "<style>"
            "body{margin:0;color:#1F2937;font-size:15px;line-height:1.8;}"
            "img{max-width:100%;height:auto;}"
            "table{max-width:100%;}"
            "pre{white-space:pre-wrap;}"
            "</style>"));
    }

    QRegularExpression img_re(QStringLiteral("<img\\b([^>]*)>"),
                              QRegularExpression::CaseInsensitiveOption);
    return replace_regex(html, img_re, [](const QRegularExpressionMatch& match) {
        QString attrs = match.captured(1);
        if (attrs.contains(QStringLiteral("style="), Qt::CaseInsensitive)) {
            QRegularExpression style_re(
                QStringLiteral("style\\s*=\\s*(['\"])(.*?)\\1"),
                QRegularExpression::CaseInsensitiveOption |
                    QRegularExpression::DotMatchesEverythingOption);
            attrs = replace_regex(attrs, style_re, [](const QRegularExpressionMatch& style_match) {
                QString style = style_match.captured(2).trimmed();
                if (!style.endsWith(';') && !style.isEmpty()) style += ';';
                style += QStringLiteral(" max-width:100%; height:auto;");
                return QStringLiteral("style=%1%2%1").arg(style_match.captured(1), style);
            });
        } else {
            attrs += QStringLiteral(" style=\"max-width:100%; height:auto;\"");
        }
        return QStringLiteral("<img%1>").arg(attrs);
    });
}

QString prepare_html_body(const Email& email) {
    QString html = QString::fromStdString(email.body_html);
    QHash<QString, QString> cid_to_file;

    for (const auto& att : email.attachments) {
        if (att.file_path.empty() || att.content_id.empty()) continue;

        QString cid = normalize_cid(QString::fromStdString(att.content_id));
        if (cid.isEmpty()) continue;

        QString path = QString::fromStdString(att.file_path);
        if (!QFile::exists(path)) continue;

        QString file_url = QUrl::fromLocalFile(path).toString();
        cid_to_file.insert(cid.toLower(), file_url);
    }

    QRegularExpression src_re(
        QStringLiteral("(src\\s*=\\s*)(['\"])(cid:([^'\"]+))\\2"),
        QRegularExpression::CaseInsensitiveOption);

    html = replace_regex(html, src_re, [&cid_to_file](const QRegularExpressionMatch& match) {
        QString cid = normalize_cid(match.captured(4)).toLower();
        auto it = cid_to_file.constFind(cid);
        if (it == cid_to_file.constEnd()) {
            return match.captured(0);
        }
        return match.captured(1) + match.captured(2) + it.value() + match.captured(2);
    });

    return add_image_constraints(html);
}
}

EmailViewWidget::EmailViewWidget(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { border: none; background: #FFFFFF; }");

    QWidget* inner = new QWidget();
    inner->setStyleSheet("background: #FFFFFF;");
    QVBoxLayout* il = new QVBoxLayout(inner);
    il->setSpacing(0);
    il->setContentsMargins(0, 0, 0, 0);

    // 邮件头部卡片
    header_label_ = new QLabel(this);
    header_label_->setWordWrap(true);
    header_label_->setStyleSheet(
        "QLabel {"
        "  background: #FFFFFF;"
        "  padding: 28px 36px 22px 36px;"
        "  border-bottom: 1px solid #E5E7EB;"
        "  margin: 0;"
        "}");
    il->addWidget(header_label_);

    // 正文
    body_browser_ = new QTextBrowser(this);
    body_browser_->setOpenExternalLinks(true);
    body_browser_->setReadOnly(true);
    body_browser_->setStyleSheet(
        "QTextBrowser {"
        "  border: none;"
        "  padding: 26px 36px;"
        "  font-size: 15px;"
        "  line-height: 1.8;"
        "  color: #1F2937;"
        "  background: #FFFFFF;"
        "}");
    il->addWidget(body_browser_, 1);

    // 附件
    attachments_widget_ = new QWidget(this);
    QVBoxLayout* al = new QVBoxLayout(attachments_widget_);
    al->setContentsMargins(36, 8, 36, 28);
    il->addWidget(attachments_widget_);

    scroll->setWidget(inner);
    layout->addWidget(scroll);
    clear();
}

void EmailViewWidget::show_email(const Email& email) {
    auto esc = [](const std::string& s) { return QString::fromStdString(s).toHtmlEscaped(); };

    QString html;
    html += "<table style='width:100%; border-collapse:collapse;'>";
    html += "<tr><td colspan='2' style='font-size:22px; font-weight:700; color:#111827; padding-bottom:18px; line-height:1.4;'>"
            + esc(email.subject.empty() ? "(无主题)" : email.subject) + "</td></tr>";

    // 发件人
    QString from = email.sender_name.empty()
        ? QString::fromStdString(email.sender_addr)
        : QString::fromStdString(email.sender_name + " <" + email.sender_addr + ">");
    html += QStringLiteral("<tr><td style='width:56px; color:#6B7280; padding:4px 0; vertical-align:top; white-space:nowrap;'>发件人</td>"
                           "<td style='color:#111827; padding:4px 0; vertical-align:top;'>%1</td></tr>").arg(from.toHtmlEscaped());

    html += QStringLiteral("<tr><td style='width:56px; color:#6B7280; padding:4px 0; vertical-align:top; white-space:nowrap;'>收件人</td>"
                           "<td style='color:#111827; padding:4px 0; vertical-align:top;'>%1</td></tr>").arg(esc(Email::join_recipients(email.to)));

    if (!email.cc.empty())
        html += QStringLiteral("<tr><td style='width:56px; color:#6B7280; padding:4px 0; vertical-align:top; white-space:nowrap;'>抄送</td>"
                               "<td style='color:#111827; padding:4px 0; vertical-align:top;'>%1</td></tr>").arg(esc(Email::join_recipients(email.cc)));

    html += QStringLiteral("<tr><td style='width:56px; color:#6B7280; padding:4px 0; vertical-align:top; white-space:nowrap;'>时间</td>"
                           "<td style='color:#6B7280; padding:4px 0; vertical-align:top;'>%1</td></tr>").arg(esc(email.received_date));
    html += "</table>";
    header_label_->setText(html);

    // 正文
    if (!email.body_html.empty()) {
        body_browser_->setHtml(prepare_html_body(email));
    } else {
        QString plain = QString::fromStdString(email.body_plain).toHtmlEscaped();
        plain.replace("\n", "<br>");
        body_browser_->setHtml("<div style='font-size:15px; line-height:1.9; color:#1F2937; max-width: 920px;'>" + plain + "</div>");
    }

    // 附件
    QLayout* al = attachments_widget_->layout();
    if (al) { QLayoutItem* c; while ((c = al->takeAt(0))) { delete c->widget(); delete c; } }
    if (email.has_attachments && !email.attachments.empty()) {
        QFrame* div = new QFrame(attachments_widget_);
        div->setStyleSheet("border-top:1px solid #E5E7EB; margin: 8px 0;");
        al->addWidget(div);

        QLabel* hdr = new QLabel(QStringLiteral("附件（%1 个）").arg(email.attachments.size()), attachments_widget_);
        hdr->setStyleSheet("font-weight:600; color:#374151; padding: 8px 0 4px 0; font-size:14px;");
        al->addWidget(hdr);

        for (const auto& att : email.attachments) {
            QFrame* card = new QFrame(attachments_widget_);
            card->setStyleSheet("QFrame{background:#F8FAFC; border:1px solid #E5E7EB; border-radius:8px; padding:12px 16px; margin:4px 0;}");
            QHBoxLayout* cl = new QHBoxLayout(card); cl->setContentsMargins(0,0,0,0);

            QString ext = QString::fromStdString(att.file_name).section('.', -1).toLower();
            QString icon = QStringLiteral("📎");
            if (ext == "pdf") icon = QStringLiteral("📕");
            else if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "bmp" || ext == "webp") icon = QStringLiteral("🖼️");
            else if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" || ext == "gz") icon = QStringLiteral("📦");
            else if (ext == "doc" || ext == "docx") icon = QStringLiteral("📝");
            else if (ext == "xls" || ext == "xlsx") icon = QStringLiteral("📊");
            else if (ext == "ppt" || ext == "pptx") icon = QStringLiteral("📽️");
            else if (ext == "mp3" || ext == "wav" || ext == "flac") icon = QStringLiteral("🎵");
            else if (ext == "mp4" || ext == "avi" || ext == "mov") icon = QStringLiteral("🎬");

            QLabel* ico = new QLabel(icon, card); ico->setStyleSheet("font-size:20px; border:none; background:transparent;");
            ico->setFixedWidth(36);
            cl->addWidget(ico);
            QLabel* nm = new QLabel(QString::fromStdString(att.file_name), card);
            nm->setStyleSheet("font-weight:500; color:#333; border:none; background:transparent;");
            nm->setWordWrap(false);
            cl->addWidget(nm, 1);

            QString size;
            if (att.file_size > 0) {
                size = att.file_size > 1048576
                    ? QString::number(att.file_size / 1048576.0, 'f', 1) + " MB"
                    : QString::number(att.file_size / 1024.0, 'f', 0) + " KB";
            }
            QLabel* sz = new QLabel(size, card);
            sz->setStyleSheet("color:#999; font-size:12px; border:none; background:transparent;");
            cl->addWidget(sz);

            // 下载按钮
            QPushButton* dl = new QPushButton(QStringLiteral("下载"), card);
            dl->setStyleSheet(
                "QPushButton { background: #E8F0FE; color: #1A73E8; border: none;"
                "border-radius: 4px; padding: 6px 14px; font-size: 12px; font-weight: 500; }"
                "QPushButton:hover { background: #D2E3FC; }");
            dl->setCursor(Qt::PointingHandCursor);
            QString src_path = QString::fromStdString(att.file_path);
            QString file_name = QString::fromStdString(att.file_name);
            connect(dl, &QPushButton::clicked, this, [src_path, file_name]() {
                QString default_path = QDir::homePath() + "/" + file_name;
                QString save_path = QFileDialog::getSaveFileName(
                    nullptr, QStringLiteral("保存附件"), default_path);
                if (save_path.isEmpty()) return;
                if (QFile::exists(save_path)) QFile::remove(save_path);
                if (!QFile::copy(src_path, save_path)) {
                    QMessageBox::warning(nullptr, QStringLiteral("下载失败"),
                        QStringLiteral("无法保存文件到：\n%1").arg(save_path));
                }
            });
            // 收信时未保存附件（旧数据 file_path 为空），按钮禁用
            if (src_path.isEmpty() || !QFile::exists(src_path)) {
                dl->setEnabled(false);
                dl->setStyleSheet(
                    "QPushButton { background: #F3F4F6; color: #9CA3AF; border: none;"
                    "border-radius: 4px; padding: 6px 14px; font-size: 12px; }");
                dl->setText(QStringLiteral("不可用"));
            }
            cl->addWidget(dl);

            al->addWidget(card);
        }
    }
}

void EmailViewWidget::clear() {
    header_label_->setText(
        "<div style='text-align:center; padding:72px 0 60px 0;'>"
        "<div style='font-size:18px; font-weight:600; color:#6B7280; margin-bottom:8px;'>" + QStringLiteral("未选择邮件") + "</div>"
        "<div style='font-size:14px; color:#9CA3AF;'>" + QStringLiteral("从左侧列表选择一封邮件查看详情") + "</div></div>");
    body_browser_->clear();
    QLayout* al = attachments_widget_->layout();
    if (al) { QLayoutItem* c; while ((c = al->takeAt(0))) { delete c->widget(); delete c; } }
}
