#include "compose_dialog.h"
#include <ctime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>

ComposeDialog::ComposeDialog(DbManager* db_mgr, int account_id, QWidget* parent)
    : QDialog(parent), db_mgr_(db_mgr), account_id_(account_id), draft_id_(-1) {
    setWindowTitle(QString::fromUtf8("Compose Email"));
    resize(700, 550);

    QVBoxLayout* main_layout = new QVBoxLayout(this);

    // From field
    QHBoxLayout* from_layout = new QHBoxLayout();
    from_layout->addWidget(new QLabel(QString::fromUtf8("From:"), this));
    from_combo_ = new QComboBox(this);
    from_combo_->setMinimumWidth(300);

    // Load accounts for the from field
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

    // To/CC/BCC
    QFormLayout* form = new QFormLayout();

    to_edit_ = new QLineEdit(this);
    to_edit_->setPlaceholderText("recipient@example.com");
    form->addRow(QString::fromUtf8("To:"), to_edit_);

    cc_edit_ = new QLineEdit(this);
    cc_edit_->setPlaceholderText("cc@example.com (semicolon separated)");
    form->addRow(QString::fromUtf8("CC:"), cc_edit_);

    bcc_edit_ = new QLineEdit(this);
    bcc_edit_->setPlaceholderText("bcc@example.com (semicolon separated)");
    form->addRow(QString::fromUtf8("BCC:"), bcc_edit_);

    subject_edit_ = new QLineEdit(this);
    form->addRow(QString::fromUtf8("Subject:"), subject_edit_);

    main_layout->addLayout(form);

    // Body editor
    body_edit_ = new QTextEdit(this);
    body_edit_->setPlaceholderText(QString::fromUtf8("Write your email here..."));
    body_edit_->setAcceptRichText(true);
    main_layout->addWidget(body_edit_, 1);

    // Attachments section
    QGroupBox* attach_group = new QGroupBox(QString::fromUtf8("Attachments"), this);
    QVBoxLayout* attach_layout = new QVBoxLayout(attach_group);

    attach_list_ = new QListWidget(this);
    attach_layout->addWidget(attach_list_);

    QHBoxLayout* attach_btn_layout = new QHBoxLayout();
    attach_btn_ = new QPushButton(QString::fromUtf8("Attach File..."), this);
    connect(attach_btn_, &QPushButton::clicked, this, &ComposeDialog::on_attach);
    attach_btn_layout->addWidget(attach_btn_);

    QPushButton* remove_attach_btn = new QPushButton(QString::fromUtf8("Remove"), this);
    connect(remove_attach_btn, &QPushButton::clicked, this, &ComposeDialog::on_remove_attachment);
    attach_btn_layout->addWidget(remove_attach_btn);
    attach_btn_layout->addStretch();

    attach_layout->addLayout(attach_btn_layout);
    main_layout->addWidget(attach_group);

    // Bottom buttons
    QHBoxLayout* btn_layout = new QHBoxLayout();
    send_btn_ = new QPushButton(QString::fromUtf8("Send"), this);
    send_btn_->setDefault(true);
    connect(send_btn_, &QPushButton::clicked, this, &ComposeDialog::on_send);
    btn_layout->addWidget(send_btn_);

    draft_btn_ = new QPushButton(QString::fromUtf8("Save Draft"), this);
    connect(draft_btn_, &QPushButton::clicked, this, &ComposeDialog::on_save_draft);
    btn_layout->addWidget(draft_btn_);

    QPushButton* discard_btn = new QPushButton(QString::fromUtf8("Discard"), this);
    connect(discard_btn, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(discard_btn);

    btn_layout->addStretch();
    main_layout->addLayout(btn_layout);
}

void ComposeDialog::set_reply_mode(const Email& original_email, bool reply_all) {
    setWindowTitle(QString::fromUtf8("Reply"));

    if (reply_all) {
        to_edit_->setText(QString::fromStdString(Email::join_recipients(original_email.to)));
        cc_edit_->setText(QString::fromStdString(Email::join_recipients(original_email.cc)));
    } else {
        // Reply to sender only
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

    // Quote original
    QString quote_text = "\n\n--- Original Message ---\n";
    quote_text += "From: " + QString::fromStdString(original_email.sender_name +
                   " <" + original_email.sender_addr + ">\n");
    quote_text += "Date: " + QString::fromStdString(original_email.received_date) + "\n";
    quote_text += "Subject: " + QString::fromStdString(original_email.subject) + "\n\n";
    quote_text += QString::fromStdString(original_email.body_plain);
    body_edit_->setPlainText(quote_text);
}

void ComposeDialog::set_forward_mode(const Email& original_email) {
    setWindowTitle(QString::fromUtf8("Forward"));
    std::string fwd_subject = original_email.subject;
    if (fwd_subject.find("Fwd:") != 0) {
        fwd_subject = "Fwd: " + fwd_subject;
    }
    subject_edit_->setText(QString::fromStdString(fwd_subject));

    QString fwd_text = "\n\n--- Forwarded Message ---\n";
    fwd_text += "From: " + QString::fromStdString(original_email.sender_name +
                 " <" + original_email.sender_addr + ">\n");
    fwd_text += "Date: " + QString::fromStdString(original_email.received_date) + "\n";
    fwd_text += "Subject: " + QString::fromStdString(original_email.subject) + "\n\n";
    fwd_text += QString::fromStdString(original_email.body_plain);
    body_edit_->setPlainText(fwd_text);
}

void ComposeDialog::load_draft(const Email& draft) {
    draft_id_ = draft.id;
    to_edit_->setText(QString::fromStdString(Email::join_recipients(draft.to)));
    cc_edit_->setText(QString::fromStdString(Email::join_recipients(draft.cc)));
    bcc_edit_->setText(QString::fromStdString(Email::join_recipients(draft.bcc)));
    subject_edit_->setText(QString::fromStdString(draft.subject));
    body_edit_->setPlainText(QString::fromStdString(draft.body_plain));

    setWindowTitle(QString::fromUtf8("Edit Draft"));
}

Email ComposeDialog::get_email() const {
    Email email;
    email.account_id = account_id_;

    // Parse recipients
    email.to = Email::split_recipients(to_edit_->text().toStdString());
    email.cc = Email::split_recipients(cc_edit_->text().toStdString());
    email.bcc = Email::split_recipients(bcc_edit_->text().toStdString());

    email.subject = subject_edit_->text().toStdString();
    email.body_plain = body_edit_->toPlainText().toStdString();
    if (body_edit_->acceptRichText()) {
        email.body_html = body_edit_->toHtml().toStdString();
    }

    // Set sender from combo
    if (from_combo_->currentIndex() >= 0) {
        email.sender_addr = from_combo_->currentText().toStdString();
    }

    // Attachments
    email.attachments = attachments_;
    email.has_attachments = !attachments_.empty();

    return email;
}

void ComposeDialog::on_send() {
    // Validate
    if (to_edit_->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("Validation Error"),
                             QString::fromUtf8("Please enter at least one recipient."));
        return;
    }
    accept();
}

void ComposeDialog::on_save_draft() {
    Email draft = get_email();
    draft.folder = "drafts";
    // Set date
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

    QMessageBox::information(this, QString::fromUtf8("Saved"),
                             QString::fromUtf8("Draft saved successfully."));
}

void ComposeDialog::on_attach() {
    QString file_path = QFileDialog::getOpenFileName(this, QString::fromUtf8("Select File"));
    if (file_path.isEmpty()) return;

    QFileInfo fi(file_path);
    Attachment att;
    att.file_name = fi.fileName().toStdString();
    att.file_path = file_path.toStdString();
    att.file_size = fi.size();

    // Guess MIME type
    QString suffix = fi.suffix().toLower();
    if (suffix == "pdf") att.mime_type = "application/pdf";
    else if (suffix == "jpg" || suffix == "jpeg") att.mime_type = "image/jpeg";
    else if (suffix == "png") att.mime_type = "image/png";
    else if (suffix == "gif") att.mime_type = "image/gif";
    else if (suffix == "txt") att.mime_type = "text/plain";
    else if (suffix == "html" || suffix == "htm") att.mime_type = "text/html";
    else if (suffix == "doc" || suffix == "docx") att.mime_type = "application/msword";
    else if (suffix == "zip") att.mime_type = "application/zip";
    else att.mime_type = "application/octet-stream";

    attachments_.push_back(att);
    attach_list_->addItem(QString::fromStdString(att.file_name) + " (" +
                          QString::number(att.file_size / 1024) + " KB)");
}

void ComposeDialog::on_remove_attachment() {
    int row = attach_list_->currentRow();
    if (row >= 0 && row < (int)attachments_.size()) {
        attachments_.erase(attachments_.begin() + row);
        delete attach_list_->takeItem(row);
    }
}
