#include "compose_dialog.h"
#include "app_style.h"
#include <ctime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QSplitter>
#include <QFrame>
#include <QFontMetrics>
#include <QByteArray>

namespace {
std::string to_utf8(const QString& text) {
    QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

QString from_utf8(const std::string& text) {
    return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
}

QString format_size(long long bytes) {
    if (bytes >= 1024 * 1024) {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    }
    return QString::number(bytes / 1024.0, 'f', 0) + QStringLiteral(" KB");
}
}

ComposeDialog::ComposeDialog(DbManager* db_mgr, int account_id, QWidget* parent)
    : QDialog(parent), db_mgr_(db_mgr), account_id_(account_id), draft_id_(-1) {
    setWindowTitle(QStringLiteral("写邮件"));
    resize(820, 640);
    setMinimumSize(680, 480);

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(12);
    main_layout->setContentsMargins(22, 22, 22, 18);

    auto field_style = QStringLiteral(
        "QLineEdit { border: none; border-bottom: 1px solid #E5E7EB; "
        "border-radius: 0; padding: 9px 0; font-size: 14px; background: transparent; }"
        "QLineEdit:focus { border-bottom: 2px solid #2563EB; }");
    auto label_style = QStringLiteral("font-weight: 600; min-width: 64px; color: #4B5563;");
    auto row_spacing = 14;

    // --- 发件人 ---
    QHBoxLayout* from_layout = new QHBoxLayout();
    QLabel* from_label = new QLabel(QStringLiteral("发件人"), this);
    from_label->setStyleSheet(label_style);
    from_label->setFixedWidth(64);
    from_layout->addWidget(from_label);

    from_combo_ = new QComboBox(this);
    from_combo_->setMinimumWidth(300);
    auto accounts = db_mgr_->get_all_accounts();
    int select_idx = -1;
    for (size_t i = 0; i < accounts.size(); ++i) {
        from_combo_->addItem(QString::fromStdString(accounts[i].email_address));
        if (accounts[i].id == account_id) select_idx = (int)i;
    }
    if (select_idx >= 0) from_combo_->setCurrentIndex(select_idx);
    from_layout->addWidget(from_combo_);
    from_layout->addStretch();
    main_layout->addLayout(from_layout);

    // --- 分隔 ---
    QFrame* div1 = new QFrame(this);
    div1->setFrameShape(QFrame::HLine);
    div1->setStyleSheet("color: #E5E7EB;");
    main_layout->addWidget(div1);

    // --- 收件人 ---
    QHBoxLayout* to_layout = new QHBoxLayout();
    to_layout->setSpacing(row_spacing);
    QLabel* to_label = new QLabel(QStringLiteral("收件人"), this);
    to_label->setStyleSheet(label_style);
    to_label->setFixedWidth(64);
    to_layout->addWidget(to_label);

    to_edit_ = new QLineEdit(this);
    to_edit_->setPlaceholderText(QStringLiteral("收件人邮箱地址，多人用分号 ; 分隔"));
    to_edit_->setStyleSheet(field_style);
    to_layout->addWidget(to_edit_, 1);

    QPushButton* cc_btn = new QPushButton(QStringLiteral("抄送"), this);
    cc_btn->setFlat(true);
    UiStyle::applyGhostButton(cc_btn, 54);
    connect(cc_btn, &QPushButton::clicked, [this]() {
        const bool show = !cc_row_->isVisible();
        cc_row_->setVisible(show);
        if (show) cc_edit_->setFocus();
    });
    to_layout->addWidget(cc_btn);
    main_layout->addLayout(to_layout);

    // --- 抄送 ---
    cc_row_ = new QWidget(this);
    cc_row_->setVisible(false);
    QHBoxLayout* cc_layout = new QHBoxLayout(cc_row_);
    cc_layout->setContentsMargins(0, 0, 0, 0);
    cc_layout->setSpacing(row_spacing);
    QLabel* cc_label = new QLabel(QStringLiteral("抄送"), cc_row_);
    cc_label->setStyleSheet(label_style);
    cc_label->setFixedWidth(64);
    cc_layout->addWidget(cc_label);

    cc_edit_ = new QLineEdit(cc_row_);
    cc_edit_->setPlaceholderText(QStringLiteral("抄送邮箱地址，多人用分号 ; 分隔"));
    cc_edit_->setStyleSheet(to_edit_->styleSheet());
    cc_layout->addWidget(cc_edit_, 1);
    main_layout->addWidget(cc_row_);

    // --- 主题 ---
    QHBoxLayout* subj_layout = new QHBoxLayout();
    QLabel* subj_label = new QLabel(QStringLiteral("主题"), this);
    subj_label->setStyleSheet(label_style);
    subj_label->setFixedWidth(64);
    subj_layout->addWidget(subj_label);

    subject_edit_ = new QLineEdit(this);
    subject_edit_->setPlaceholderText(QStringLiteral("邮件主题"));
    subject_edit_->setStyleSheet(to_edit_->styleSheet());
    subj_layout->addWidget(subject_edit_, 1);
    main_layout->addLayout(subj_layout);

    // --- 分隔 ---
    QFrame* div2 = new QFrame(this);
    div2->setFrameShape(QFrame::HLine);
    div2->setStyleSheet("color: #E5E7EB;");
    main_layout->addWidget(div2);

    // --- 正文 ---
    body_edit_ = new QTextEdit(this);
    body_edit_->setPlaceholderText(QStringLiteral("在这里输入邮件正文..."));
    body_edit_->setAcceptRichText(true);
    body_edit_->setStyleSheet(
        "QTextEdit { border: 1px solid #E5E7EB; border-radius: 8px; font-size: 15px; "
        "line-height: 1.7; padding: 12px; background: #FFFFFF; }"
        "QTextEdit:focus { border-color: #2563EB; }");
    main_layout->addWidget(body_edit_, 1);

    // --- 底部工具栏 ---
    QHBoxLayout* bottom_bar = new QHBoxLayout();
    bottom_bar->setContentsMargins(0, 8, 0, 0);

    attach_btn_ = new QPushButton(QStringLiteral("添加附件"), this);
    UiStyle::applySecondaryButton(attach_btn_, 96);
    connect(attach_btn_, &QPushButton::clicked, this, &ComposeDialog::on_attach);
    bottom_bar->addWidget(attach_btn_);

    // 附件预览
    attach_panel_ = new QWidget(this);
    attach_panel_->setObjectName("composeAttachmentPanel");
    attach_panel_->setVisible(false);
    attach_panel_->setStyleSheet(
        "QWidget#composeAttachmentPanel { background: #F8FAFC; border: 1px solid #E3EAF2; border-radius: 8px; }");
    attach_layout_ = new QVBoxLayout(attach_panel_);
    attach_layout_->setContentsMargins(12, 10, 12, 10);
    attach_layout_->setSpacing(8);
    bottom_bar->addWidget(attach_panel_, 1);

    bottom_bar->addStretch();

    QPushButton* discard_btn = new QPushButton(QStringLiteral("丢弃"), this);
    UiStyle::applyGhostButton(discard_btn, 72);
    connect(discard_btn, &QPushButton::clicked, this, &QDialog::reject);
    bottom_bar->addWidget(discard_btn);

    draft_btn_ = new QPushButton(QStringLiteral("保存草稿"), this);
    UiStyle::applySecondaryButton(draft_btn_, 96);
    connect(draft_btn_, &QPushButton::clicked, this, &ComposeDialog::on_save_draft);
    bottom_bar->addWidget(draft_btn_);

    send_btn_ = new QPushButton(QStringLiteral("发送"), this);
    UiStyle::applyPrimaryButton(send_btn_, 96);
    connect(send_btn_, &QPushButton::clicked, this, &ComposeDialog::on_send);
    bottom_bar->addWidget(send_btn_);

    main_layout->addLayout(bottom_bar);

    // 附件列表更新时显示/隐藏
}

void ComposeDialog::set_reply_mode(const Email& original_email, bool reply_all) {
    setWindowTitle(QStringLiteral("回复邮件"));

    if (reply_all) {
        to_edit_->setText(QString::fromStdString(Email::join_recipients(original_email.to)));
        cc_edit_->setText(QString::fromStdString(Email::join_recipients(original_email.cc)));
        cc_row_->setVisible(!cc_edit_->text().trimmed().isEmpty());
    } else {
        if (!original_email.sender_name.empty()) {
            to_edit_->setText(QString::fromStdString(original_email.sender_name +
                               " <" + original_email.sender_addr + ">"));
        } else {
            to_edit_->setText(QString::fromStdString(original_email.sender_addr));
        }
    }

    std::string re_subject = original_email.subject;
    if (re_subject.find("Re:") != 0) {
        re_subject = "Re: " + re_subject;
    }
    subject_edit_->setText(QString::fromStdString(re_subject));

    QString quote_text = "\n\n--- " + QStringLiteral("原始邮件") + " ---\n";
    quote_text += QStringLiteral("发件人：") + QString::fromStdString(original_email.sender_name +
                   " <" + original_email.sender_addr + ">\n");
    quote_text += QStringLiteral("时间：") + QString::fromStdString(original_email.received_date) + "\n";
    quote_text += QStringLiteral("主题：") + QString::fromStdString(original_email.subject) + "\n\n";
    quote_text += QString::fromStdString(original_email.body_plain);
    body_edit_->setPlainText(quote_text);

    QTextCursor cursor = body_edit_->textCursor();
    cursor.movePosition(QTextCursor::Start);
    body_edit_->setTextCursor(cursor);
}

void ComposeDialog::set_forward_mode(const Email& original_email) {
    setWindowTitle(QStringLiteral("转发邮件"));
    std::string fwd_subject = original_email.subject;
    if (fwd_subject.find("Fwd:") != 0) {
        fwd_subject = "Fwd: " + fwd_subject;
    }
    subject_edit_->setText(QString::fromStdString(fwd_subject));

    QString fwd_text = "\n\n--- " + QStringLiteral("转发邮件") + " ---\n";
    fwd_text += QStringLiteral("发件人：") + QString::fromStdString(original_email.sender_name +
                 " <" + original_email.sender_addr + ">\n");
    fwd_text += QStringLiteral("时间：") + QString::fromStdString(original_email.received_date) + "\n";
    fwd_text += QStringLiteral("主题：") + QString::fromStdString(original_email.subject) + "\n\n";
    fwd_text += QString::fromStdString(original_email.body_plain);
    body_edit_->setPlainText(fwd_text);
}

void ComposeDialog::load_draft(const Email& draft) {
    draft_id_ = draft.id;
    to_edit_->setText(QString::fromStdString(Email::join_recipients(draft.to)));
    cc_edit_->setText(QString::fromStdString(Email::join_recipients(draft.cc)));
    cc_row_->setVisible(!cc_edit_->text().trimmed().isEmpty());
    subject_edit_->setText(QString::fromStdString(draft.subject));
    body_edit_->setPlainText(QString::fromStdString(draft.body_plain));
    attachments_ = draft.attachments;
    rebuild_attachment_panel();
    setWindowTitle(QStringLiteral("编辑草稿"));
}

Email ComposeDialog::get_email() const {
    Email email;
    email.account_id = account_id_;
    email.to = Email::split_recipients(to_edit_->text().toStdString());
    email.cc = Email::split_recipients(cc_edit_->text().toStdString());
    email.subject = subject_edit_->text().toStdString();
    email.body_plain = body_edit_->toPlainText().toStdString();
    if (body_edit_->acceptRichText()) {
        email.body_html = body_edit_->toHtml().toStdString();
    }
    if (from_combo_->currentIndex() >= 0) {
        email.sender_addr = from_combo_->currentText().toStdString();
    }
    email.attachments = attachments_;
    email.has_attachments = !attachments_.empty();
    return email;
}

void ComposeDialog::on_send() {
    if (to_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请填写收件人邮箱地址。"));
        return;
    }
    for (const auto& att : attachments_) {
        QString path = from_utf8(att.file_path);
        if (path.isEmpty() || !QFileInfo::exists(path)) {
            QMessageBox::warning(this, QStringLiteral("附件不可用"),
                                 QStringLiteral("附件文件不存在或无法访问：\n%1").arg(path));
            return;
        }
    }
    accept();
}

void ComposeDialog::on_save_draft() {
    Email draft = get_email();
    draft.folder = "drafts";
    auto now = std::time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    draft.received_date = buf;

    if (draft_id_ > 0) {
        draft.id = draft_id_;
        db_mgr_->update_email(draft);
    } else {
        draft_id_ = db_mgr_->save_email(draft);
    }

    QMessageBox::information(this, QStringLiteral("已保存"),
                             QStringLiteral("草稿保存成功。"));
}

void ComposeDialog::on_attach() {
    QString file_path = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"));
    if (file_path.isEmpty()) return;

    QFileInfo fi(file_path);
    Attachment att;
    att.file_name = to_utf8(fi.fileName());
    att.file_path = to_utf8(file_path);
    att.file_size = fi.size();

    QString suffix = fi.suffix().toLower();
    if (suffix == "pdf") att.mime_type = "application/pdf";
    else if (suffix == "jpg" || suffix == "jpeg") att.mime_type = "image/jpeg";
    else if (suffix == "png") att.mime_type = "image/png";
    else if (suffix == "gif") att.mime_type = "image/gif";
    else if (suffix == "txt") att.mime_type = "text/plain";
    else if (suffix == "html" || suffix == "htm") att.mime_type = "text/html";
    else if (suffix == "doc" || suffix == "docx") att.mime_type = "application/msword";
    else if (suffix == "zip" || suffix == "rar") att.mime_type = "application/zip";
    else att.mime_type = "application/octet-stream";

    attachments_.push_back(att);
    rebuild_attachment_panel();
}

void ComposeDialog::rebuild_attachment_panel() {
    if (!attach_layout_ || !attach_panel_) return;

    QLayoutItem* item = nullptr;
    while ((item = attach_layout_->takeAt(0))) {
        delete item->widget();
        delete item;
    }

    attach_panel_->setVisible(!attachments_.empty());
    if (attachments_.empty()) return;

    QLabel* header = new QLabel(QStringLiteral("附件（%1 个）").arg(attachments_.size()), attach_panel_);
    header->setStyleSheet("color:#1F3A5F; font-weight:600; font-size:13px; background:transparent;");
    attach_layout_->addWidget(header);

    for (int i = 0; i < static_cast<int>(attachments_.size()); ++i) {
        const Attachment& att = attachments_[static_cast<size_t>(i)];
        QFrame* row = new QFrame(attach_panel_);
        row->setStyleSheet(
            "QFrame { background:#FFFFFF; border:1px solid #E5ECF4; border-radius:7px; }"
            "QLabel { border:none; background:transparent; }");

        auto* row_layout = new QVBoxLayout(row);
        row_layout->setContentsMargins(12, 8, 12, 8);
        row_layout->setSpacing(6);

        auto* top = new QHBoxLayout();
        top->setContentsMargins(0, 0, 0, 0);
        top->setSpacing(10);

        QString full_name = from_utf8(att.file_name);
        QFontMetrics metrics(row->font());
        QLabel* name = new QLabel(metrics.elidedText(full_name, Qt::ElideMiddle, 260), row);
        name->setStyleSheet("color:#111827; font-size:13px; font-weight:500;");
        name->setMinimumWidth(120);
        name->setMaximumWidth(300);
        name->setToolTip(full_name + QStringLiteral("\n") + from_utf8(att.file_path));
        top->addWidget(name, 1);

        QLabel* size = new QLabel(format_size(att.file_size), row);
        size->setStyleSheet("color:#7B8794; font-size:12px;");
        top->addWidget(size, 0, Qt::AlignRight);

        QLabel* state = new QLabel(QStringLiteral("已添加"), row);
        state->setFixedWidth(46);
        state->setStyleSheet("color:#16A34A; font-size:12px; font-weight:600;");
        top->addWidget(state, 0, Qt::AlignRight);

        QPushButton* remove = new QPushButton(QStringLiteral("移除"), row);
        remove->setCursor(Qt::PointingHandCursor);
        remove->setStyleSheet(
            "QPushButton { background:#F3F6FA; color:#5F6F82; border:none; border-radius:5px; padding:5px 10px; font-size:12px; }"
            "QPushButton:hover { background:#FFE8E8; color:#C24141; }");
        connect(remove, &QPushButton::clicked, this, [this, i]() {
            if (i < 0 || i >= static_cast<int>(attachments_.size())) return;
            attachments_.erase(attachments_.begin() + i);
            rebuild_attachment_panel();
        });
        top->addWidget(remove);
        row_layout->addLayout(top);

        attach_layout_->addWidget(row);

    }
}
