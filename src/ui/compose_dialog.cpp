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
#include <QSplitter>

ComposeDialog::ComposeDialog(DbManager* db_mgr, int account_id, QWidget* parent)
    : QDialog(parent), db_mgr_(db_mgr), account_id_(account_id), draft_id_(-1) {
    setWindowTitle(QStringLiteral("✏️ 写邮件"));
    resize(750, 580);
    setMinimumSize(600, 400);

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(16, 16, 16, 16);

    // --- 发件人 ---
    QHBoxLayout* from_layout = new QHBoxLayout();
    QLabel* from_label = new QLabel(QStringLiteral("发件人"), this);
    from_label->setStyleSheet("font-weight: 600; min-width: 56px; color: #555;");
    from_label->setFixedWidth(56);
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
    div1->setStyleSheet("color: #F0F0F0;");
    main_layout->addWidget(div1);

    // --- 收件人 ---
    QHBoxLayout* to_layout = new QHBoxLayout();
    QLabel* to_label = new QLabel(QStringLiteral("收件人"), this);
    to_label->setStyleSheet("font-weight: 600; min-width: 56px; color: #555;");
    to_label->setFixedWidth(56);
    to_layout->addWidget(to_label);

    to_edit_ = new QLineEdit(this);
    to_edit_->setPlaceholderText(QStringLiteral("收件人邮箱地址，多人用分号 ; 分隔"));
    to_edit_->setStyleSheet("QLineEdit { border: none; border-radius: 0; padding: 8px 0; font-size: 14px; "
                            "border-bottom: 1px solid #F0F0F0; }"
                            "QLineEdit:focus { border-bottom: 2px solid #1890FF; }");
    to_layout->addWidget(to_edit_, 1);

    QPushButton* cc_btn = new QPushButton(QStringLiteral("抄送"), this);
    cc_btn->setFlat(true);
    cc_btn->setStyleSheet("QPushButton { color: #1890FF; border: none; font-size: 12px; padding: 4px 8px; }"
                          "QPushButton:hover { background: #ECF5FF; border-radius: 4px; }");
    connect(cc_btn, &QPushButton::clicked, [this]() {
        cc_edit_->setVisible(!cc_edit_->isVisible());
    });
    to_layout->addWidget(cc_btn);
    main_layout->addLayout(to_layout);

    // --- 抄送 ---
    QHBoxLayout* cc_layout = new QHBoxLayout();
    QLabel* cc_label = new QLabel(QStringLiteral("抄　送"), this);
    cc_label->setStyleSheet("font-weight: 600; min-width: 56px; color: #555;");
    cc_label->setFixedWidth(56);
    cc_layout->addWidget(cc_label);

    cc_edit_ = new QLineEdit(this);
    cc_edit_->setPlaceholderText(QStringLiteral("抄送邮箱地址"));
    cc_edit_->setStyleSheet(to_edit_->styleSheet());
    cc_edit_->setVisible(false);
    cc_layout->addWidget(cc_edit_, 1);
    main_layout->addLayout(cc_layout);

    // --- 主题 ---
    QHBoxLayout* subj_layout = new QHBoxLayout();
    QLabel* subj_label = new QLabel(QStringLiteral("主　题"), this);
    subj_label->setStyleSheet("font-weight: 600; min-width: 56px; color: #555;");
    subj_label->setFixedWidth(56);
    subj_layout->addWidget(subj_label);

    subject_edit_ = new QLineEdit(this);
    subject_edit_->setPlaceholderText(QStringLiteral("邮件主题"));
    subject_edit_->setStyleSheet(to_edit_->styleSheet());
    subj_layout->addWidget(subject_edit_, 1);
    main_layout->addLayout(subj_layout);

    // --- 分隔 ---
    QFrame* div2 = new QFrame(this);
    div2->setFrameShape(QFrame::HLine);
    div2->setStyleSheet("color: #F0F0F0;");
    main_layout->addWidget(div2);

    // --- 正文 ---
    body_edit_ = new QTextEdit(this);
    body_edit_->setPlaceholderText(QStringLiteral("在这里输入邮件正文..."));
    body_edit_->setAcceptRichText(true);
    body_edit_->setStyleSheet(
        "QTextEdit { border: none; font-size: 15px; line-height: 1.6; padding: 4px 0; }");
    main_layout->addWidget(body_edit_, 1);

    // --- 底部工具栏 ---
    QHBoxLayout* bottom_bar = new QHBoxLayout();
    bottom_bar->setContentsMargins(0, 8, 0, 0);

    attach_btn_ = new QPushButton(QStringLiteral("📎 添加附件"), this);
    attach_btn_->setStyleSheet("QPushButton { border: 1px dashed #D9D9D9; border-radius: 6px; "
                               "padding: 6px 14px; color: #666; background: transparent; }"
                               "QPushButton:hover { border-color: #1890FF; color: #1890FF; }");
    connect(attach_btn_, &QPushButton::clicked, this, &ComposeDialog::on_attach);
    bottom_bar->addWidget(attach_btn_);

    // 附件预览
    attach_list_ = new QListWidget(this);
    attach_list_->setMaximumHeight(80);
    attach_list_->setStyleSheet(
        "QListWidget { border: 1px solid #F0F0F0; border-radius: 6px; padding: 4px; font-size: 12px; }"
        "QListWidget::item { padding: 2px 8px; }");
    attach_list_->setVisible(false);
    bottom_bar->addWidget(attach_list_, 1);

    bottom_bar->addStretch();

    QPushButton* discard_btn = new QPushButton(QStringLiteral("丢弃"), this);
    connect(discard_btn, &QPushButton::clicked, this, &QDialog::reject);
    bottom_bar->addWidget(discard_btn);

    draft_btn_ = new QPushButton(QStringLiteral("保存草稿"), this);
    connect(draft_btn_, &QPushButton::clicked, this, &ComposeDialog::on_save_draft);
    bottom_bar->addWidget(draft_btn_);

    send_btn_ = new QPushButton(QStringLiteral("发　送"), this);
    send_btn_->setObjectName("primaryBtn");
    send_btn_->setStyleSheet(
        "QPushButton { background: #1890FF; color: #FFF; border: none; border-radius: 6px; "
        "padding: 8px 28px; font-size: 14px; font-weight: 600; }"
        "QPushButton:hover { background: #40A9FF; }"
        "QPushButton:pressed { background: #096DD9; }");
    connect(send_btn_, &QPushButton::clicked, this, &ComposeDialog::on_send);
    bottom_bar->addWidget(send_btn_);

    main_layout->addLayout(bottom_bar);

    // 附件列表更新时显示/隐藏
    connect(attach_list_->model(), &QAbstractItemModel::rowsInserted, [this]() {
        attach_list_->setVisible(attach_list_->count() > 0);
    });
    connect(attach_list_->model(), &QAbstractItemModel::rowsRemoved, [this]() {
        attach_list_->setVisible(attach_list_->count() > 0);
    });
}

void ComposeDialog::set_reply_mode(const Email& original_email, bool reply_all) {
    setWindowTitle(QStringLiteral("↩ 回复邮件"));

    if (reply_all) {
        to_edit_->setText(QString::fromStdString(Email::join_recipients(original_email.to)));
        cc_edit_->setText(QString::fromStdString(Email::join_recipients(original_email.cc)));
        cc_edit_->setVisible(true);
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
    setWindowTitle(QStringLiteral("↪ 转发邮件"));
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
    subject_edit_->setText(QString::fromStdString(draft.subject));
    body_edit_->setPlainText(QString::fromStdString(draft.body_plain));
    setWindowTitle(QStringLiteral("📝 编辑草稿"));
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
    att.file_name = fi.fileName().toStdString();
    att.file_path = file_path.toStdString();
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
    QString size_text;
    if (att.file_size > 1024 * 1024)
        size_text = QString::number(att.file_size / (1024.0 * 1024.0), 'f', 1) + " MB";
    else
        size_text = QString::number(att.file_size / 1024.0, 'f', 0) + " KB";

    attach_list_->addItem(QString::fromStdString(att.file_name) + "  (" + size_text + ")");
}

void ComposeDialog::on_remove_attachment() {
    int row = attach_list_->currentRow();
    if (row >= 0 && row < (int)attachments_.size()) {
        attachments_.erase(attachments_.begin() + row);
        delete attach_list_->takeItem(row);
    }
}
