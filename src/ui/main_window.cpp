#include "main_window.h"
#include "email_list_widget.h"
#include "email_view_widget.h"
#include "compose_dialog.h"
#include "account_dialog.h"
#include "app_style.h"
#include "../core/mime_encoder.h"
#include "../core/mime_decoder.h"

#include <ctime>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QApplication>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QFutureWatcher>
#include <QThread>
#include <QScreen>
#include <QFrame>

MainWindow::MainWindow(DbManager* db_mgr, QWidget* parent)
    : QMainWindow(parent), db_mgr_(db_mgr), current_account_id_(-1), current_folder_("inbox") {
    qApp->setStyleSheet(UiStyle::globalStyle());
    setup_ui();
    setup_toolbar();
    setup_menu();
    load_accounts();

    setWindowTitle(QStringLiteral("邮件系统"));
    resize(1180, 760);
    QRect screen = QApplication::primaryScreen()->availableGeometry();
    move(screen.x() + (screen.width() - width()) / 2,
         screen.y() + (screen.height() - height()) / 2);
    status_label_->setText(QStringLiteral("就绪"));
}

MainWindow::~MainWindow() {}

void MainWindow::setup_ui() {
    QWidget* central = new QWidget(this);
    central->setObjectName("mainSurface");
    setCentralWidget(central);
    QHBoxLayout* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // ========== 左侧导航栏 ==========
    QWidget* left_panel = new QWidget(this);
    left_panel->setFixedWidth(220);
    left_panel->setStyleSheet("background: #FFFFFF; border-right: 1px solid #E5E7EB;");
    QVBoxLayout* left_layout = new QVBoxLayout(left_panel);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(0);

    // 写信按钮
    QWidget* compose_area = new QWidget(left_panel);
    compose_area->setStyleSheet("padding: 18px 16px 14px 16px;");
    QHBoxLayout* cl = new QHBoxLayout(compose_area);
    cl->setContentsMargins(0, 0, 0, 0);
    QPushButton* compose_btn = new QPushButton(QStringLiteral("写信"), left_panel);
    UiStyle::applyPrimaryButton(compose_btn, 160);
    compose_btn->setMinimumHeight(38);
    compose_btn->setStyleSheet(
        "QPushButton { background: #2563EB; color: #FFFFFF; border: 1px solid #2563EB; "
        "border-radius: 6px; font-weight: 600; }"
        "QPushButton:hover { background: #1D4ED8; border-color: #1D4ED8; }");
    connect(compose_btn, &QPushButton::clicked, this, &MainWindow::on_compose);
    cl->addWidget(compose_btn, 1);
    left_layout->addWidget(compose_area);

    // 分隔线
    QFrame* sep = new QFrame(left_panel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #EEF1F5; margin: 0 16px;");
    left_layout->addWidget(sep);

    // 文件夹
    folder_tree_ = new QTreeWidget(left_panel);
    folder_tree_->setHeaderHidden(true);
    folder_tree_->setRootIsDecorated(false);
    folder_tree_->setIndentation(0);
    folder_tree_->setCursor(Qt::PointingHandCursor);

    inbox_item_   = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("收件箱")));
    sent_item_    = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("已发送")));
    drafts_item_  = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("草稿箱")));
    trash_item_   = new QTreeWidgetItem(folder_tree_, QStringList(QStringLiteral("废纸篓")));

    // 图标列宽
    for (auto* item : {inbox_item_, sent_item_, drafts_item_, trash_item_})
        item->setSizeHint(0, QSize(0, 42));

    folder_tree_->expandAll();
    folder_tree_->setCurrentItem(inbox_item_);
    left_layout->addWidget(folder_tree_, 1);

    // 底部账号区
    QWidget* acct_bottom = new QWidget(left_panel);
    acct_bottom->setStyleSheet("border-top: 1px solid #E5E7EB; padding: 14px 16px;");
    QVBoxLayout* ab = new QVBoxLayout(acct_bottom);
    ab->setContentsMargins(0, 0, 0, 0);
    ab->setSpacing(6);
    QLabel* account_label = new QLabel(QStringLiteral("当前账号"), acct_bottom);
    account_label->setStyleSheet("font-size: 12px; color: #6B7280;");
    ab->addWidget(account_label);
    account_combo_ = new QComboBox(left_panel);
    account_combo_->setMinimumWidth(180);
    ab->addWidget(account_combo_);
    left_layout->addWidget(acct_bottom);

    main_layout->addWidget(left_panel);

    // ========== 右侧：邮件列表 + 阅读视图 ==========
    email_list_ = new EmailListWidget(this);
    email_view_ = new EmailViewWidget(this);

    right_splitter_ = new QSplitter(Qt::Vertical, this);
    right_splitter_->setHandleWidth(4);
    right_splitter_->addWidget(email_list_);
    right_splitter_->addWidget(email_view_);
    right_splitter_->setStretchFactor(0, 3);
    right_splitter_->setStretchFactor(1, 4);
    right_splitter_->setStyleSheet("QSplitter::handle { background: #E5E7EB; }");

    main_layout->addWidget(right_splitter_, 1);

    // 信号
    connect(folder_tree_, &QTreeWidget::itemClicked, this, &MainWindow::on_folder_changed);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_account_changed);
    connect(email_list_, &EmailListWidget::email_selected, this, &MainWindow::on_email_selected);

    // 状态栏
    status_label_ = new QLabel(this);
    status_label_->setStyleSheet("color: #6B7280; font-size: 12px; padding: 2px 8px;");
    statusBar()->addWidget(status_label_);
    statusBar()->setStyleSheet("QStatusBar { background: #FFFFFF; border-top: 1px solid #E8EAED; }");
}

void MainWindow::setup_toolbar() {
    toolbar_ = addToolBar(QStringLiteral("工具栏"));
    toolbar_->setMovable(false);
    toolbar_->setFloatable(false);

    QLabel* brand = new QLabel(QStringLiteral("邮件系统"), this);
    brand->setStyleSheet("font-size: 18px; font-weight: 700; color: #111827; padding: 0 10px 0 2px;");
    toolbar_->addWidget(brand);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar_->addWidget(spacer);

    btn_receive_ = new QPushButton(QStringLiteral("收信"), this);
    UiStyle::applySecondaryButton(btn_receive_, 76);
    toolbar_->addWidget(btn_receive_);
    connect(btn_receive_, &QPushButton::clicked, this, &MainWindow::on_receive);

    btn_compose_ = new QPushButton(QStringLiteral("写信"), this);
    UiStyle::applyPrimaryButton(btn_compose_, 76);
    toolbar_->addWidget(btn_compose_);
    connect(btn_compose_, &QPushButton::clicked, this, &MainWindow::on_compose);

    btn_reply_ = new QPushButton(QStringLiteral("回复"), this);
    UiStyle::applySecondaryButton(btn_reply_, 76);
    toolbar_->addWidget(btn_reply_);
    connect(btn_reply_, &QPushButton::clicked, this, &MainWindow::on_reply);

    btn_delete_ = new QPushButton(QStringLiteral("删除"), this);
    UiStyle::applyDangerButton(btn_delete_, 76);
    toolbar_->addWidget(btn_delete_);
    connect(btn_delete_, &QPushButton::clicked, this, &MainWindow::on_delete_mail);

    btn_restore_ = new QPushButton(QStringLiteral("恢复"), this);
    UiStyle::applySecondaryButton(btn_restore_, 76);
    btn_restore_->setVisible(true);
    toolbar_->addWidget(btn_restore_);
    connect(btn_restore_, &QPushButton::clicked, this, &MainWindow::on_restore_mail);

    btn_refresh_ = new QPushButton(QStringLiteral("刷新"), this);
    UiStyle::applyGhostButton(btn_refresh_, 76);
    toolbar_->addWidget(btn_refresh_);
    connect(btn_refresh_, &QPushButton::clicked, this, &MainWindow::on_refresh);
}

void MainWindow::setup_menu() {
    QMenu* file_menu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

    QAction* add_account_action = file_menu->addAction(QStringLiteral("添加账号..."));
    connect(add_account_action, &QAction::triggered, [this]() {
        AccountDialog dlg(db_mgr_, this);
        if (dlg.exec() == QDialog::Accepted) {
            Account acc = dlg.get_account();
            db_mgr_->add_account(acc);
            load_accounts();
        }
    });

    QAction* edit_account_action = file_menu->addAction(QStringLiteral("编辑账号..."));
    connect(edit_account_action, &QAction::triggered, [this]() {
        if (current_account_id_ < 0) return;
        Account acc = db_mgr_->get_account(current_account_id_);
        AccountDialog dlg(db_mgr_, this);
        dlg.set_account(acc);
        if (dlg.exec() == QDialog::Accepted) {
            Account new_acc = dlg.get_account();
            new_acc.id = current_account_id_;
            db_mgr_->update_account(new_acc);
            load_accounts();
        }
    });

    QAction* delete_account_action = file_menu->addAction(QStringLiteral("删除账号"));
    connect(delete_account_action, &QAction::triggered, [this]() {
        if (current_account_id_ < 0) return;
        if (QMessageBox::question(this, QStringLiteral("确认"),
              QStringLiteral("确定要删除当前账号吗？")) == QMessageBox::Yes) {
            db_mgr_->delete_account(current_account_id_);
            current_account_id_ = -1;
            load_accounts();
        }
    });

    file_menu->addSeparator();

    QAction* exit_action = file_menu->addAction(QStringLiteral("退出(&X)"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, qApp, &QApplication::quit);

    QMenu* help_menu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    help_menu->addAction(QStringLiteral("关于"))->connect(help_menu, &QMenu::triggered, [this]() {
        QMessageBox::about(this, QStringLiteral("关于"),
            QStringLiteral("<h3>邮件系统 v1.0</h3>"
                           "<p>计算机网络实践课程项目</p>"
                           "<p style='color:#888;'>C++17 / Qt 6 / MySQL / OpenSSL</p>"));
    });
}

// ---------- 以下方法（load_accounts / on_account_changed / on_folder_changed /
//           load_emails / update_folder_counts / on_email_selected /
//           on_receive / on_compose / on_send_draft / on_reply /
//           on_delete_mail / on_refresh）与之前相同 ----------

void MainWindow::load_accounts() {
    accounts_ = db_mgr_->get_all_accounts();
    account_combo_->blockSignals(true);
    account_combo_->clear();
    for (const auto& acc : accounts_)
        account_combo_->addItem(QString::fromStdString(acc.account_name + " (" + acc.email_address + ")"));
    account_combo_->blockSignals(false);

    if (!accounts_.empty() && current_account_id_ < 0) {
        current_account_id_ = accounts_[0].id;
        account_combo_->setCurrentIndex(0);
        load_emails(current_folder_);
        update_folder_counts();
    }
}

void MainWindow::on_account_changed(int index) {
    if (index >= 0 && index < (int)accounts_.size()) {
        current_account_id_ = accounts_[index].id;
        load_emails(current_folder_);
        update_folder_counts();
    }
}

void MainWindow::on_folder_changed() {
    QTreeWidgetItem* item = folder_tree_->currentItem();
    if (!item) return;
    if (item == inbox_item_)      current_folder_ = "inbox";
    else if (item == sent_item_)  current_folder_ = "sent";
    else if (item == drafts_item_) current_folder_ = "drafts";
    else if (item == trash_item_) current_folder_ = "trash";
    load_emails(current_folder_);
}

void MainWindow::load_emails(const std::string& folder) {
    if (current_account_id_ < 0) { email_list_->clear(); email_view_->clear(); return; }
    // 废纸篓特殊处理：显示所有 is_deleted=1 的邮件
    std::vector<Email> emails;
    if (folder == "trash") {
        emails = db_mgr_->get_deleted_emails(current_account_id_);
    } else {
        emails = db_mgr_->get_emails(current_account_id_, folder);
    }
    email_list_->set_emails(emails);
    email_view_->clear();

    QString name;
    if (folder == "inbox") name = QStringLiteral("收件箱");
    else if (folder == "sent") name = QStringLiteral("已发送");
    else if (folder == "drafts") name = QStringLiteral("草稿箱");
    else name = QStringLiteral("废纸篓");
    status_label_->setText(QStringLiteral("%1 — %2 封邮件").arg(name).arg(emails.size()));
}

void MainWindow::update_folder_counts() {
    if (current_account_id_ < 0) return;
    int unread = db_mgr_->get_unread_count(current_account_id_, "inbox");
    inbox_item_->setText(0, unread > 0
        ? QStringLiteral("收件箱 (%1)").arg(unread) : QStringLiteral("收件箱"));
    int sent = db_mgr_->get_total_count(current_account_id_, "sent");
    sent_item_->setText(0, sent > 0 ? QStringLiteral("已发送 (%1)").arg(sent) : QStringLiteral("已发送"));
    int drafts = db_mgr_->get_total_count(current_account_id_, "drafts");
    drafts_item_->setText(0, drafts > 0 ? QStringLiteral("草稿箱 (%1)").arg(drafts) : QStringLiteral("草稿箱"));
}

void MainWindow::on_email_selected(int email_id) {
    if (email_id < 0) return;
    Email email = db_mgr_->get_email(email_id);

    if (current_folder_ == "drafts") {
        ComposeDialog dlg(db_mgr_, current_account_id_, this);
        dlg.load_draft(email);
        if (dlg.exec() == QDialog::Accepted)
            on_send_draft(email_id, dlg.get_email());
        return;
    }

    email_view_->show_email(email);
    if (!email.is_read) { db_mgr_->mark_read(email_id); update_folder_counts(); }
}

void MainWindow::on_send_draft(int draft_id, const Email& updated) {
    Account acc = db_mgr_->get_account(current_account_id_);
    status_label_->setText(QStringLiteral("发送草稿中..."));

    typedef std::pair<bool, std::string> SR;
    auto* w = new QFutureWatcher<SR>(this);
    connect(w, &QFutureWatcher<SR>::finished, [this, w, draft_id, updated, acc]() {
        SR r = w->result();
        if (r.first) {
            db_mgr_->mark_deleted(draft_id);
            Email sent = updated; sent.account_id = acc.id; sent.folder = "sent";
            auto now = std::time(nullptr); char buf[64];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            sent.received_date = buf;
            int eid = db_mgr_->save_email(sent);
            for (const auto& att : sent.attachments) {
                db_mgr_->add_attachment(eid, att);
            }
            status_label_->setText(QStringLiteral("草稿已发送！"));
        } else {
            QMessageBox::critical(this, QStringLiteral("发送失败"), QString::fromStdString(r.second));
            status_label_->setText(QStringLiteral("发送失败"));
        }
        load_emails(current_folder_); update_folder_counts();
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([updated, acc]() -> SR {
        MimeEncoder enc; SmtpClient smtp;
        Email e = updated; e.body_plain = enc.encode(updated);
        bool ok = smtp.send_email(e, acc.smtp_server, acc.smtp_port, acc.smtp_ssl, acc.username, acc.password);
        return {ok, ok ? "" : smtp.get_last_error()};
    }));
}

void MainWindow::on_receive() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加邮箱账号。")); return;
    }
    Account acc = db_mgr_->get_account(current_account_id_);
    btn_receive_->setEnabled(false);
    status_label_->setText(QStringLiteral("正在接收邮件..."));

    typedef std::pair<std::vector<Email>, std::string> RR;
    auto* w = new QFutureWatcher<RR>(this);
    connect(w, &QFutureWatcher<RR>::finished, [this, w, acc]() {
        RR r = w->result(); auto emails = r.first; std::string err = r.second;
        if (!err.empty()) {
            QMessageBox::warning(this, QStringLiteral("接收失败"), QString::fromStdString(err));
            status_label_->setText(QStringLiteral("接收失败 — %1").arg(QString::fromStdString(err)));
        } else {
            MimeDecoder dec; int new_count = 0;
            int failed_count = 0;
            std::string first_save_error;
            for (auto& email : emails) {
                std::string uid = email.pop3_uid;
                if (!uid.empty() && db_mgr_->is_uid_downloaded(acc.id, uid)) continue;

                Email decoded = dec.decode(email.body_plain, acc.id);
                decoded.pop3_uid = uid;
                decoded.folder = "inbox";
                if (decoded.received_date.empty()) {
                    auto now = std::time(nullptr);
                    char buf[64];
                    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                    decoded.received_date = buf;
                }

                int eid = db_mgr_->save_email(decoded);
                if (eid > 0) {
                    if (!uid.empty()) db_mgr_->mark_uid_downloaded(acc.id, uid, eid);
                    new_count++;
                } else {
                    failed_count++;
                    if (first_save_error.empty()) {
                        first_save_error = db_mgr_->last_error();
                    }
                }
            }
            if (failed_count > 0) {
                QString msg = QStringLiteral("已接收 %1 封，%2 封保存失败")
                                  .arg(new_count).arg(failed_count);
                if (!first_save_error.empty()) {
                    msg += QStringLiteral("\n\n数据库错误：%1").arg(QString::fromStdString(first_save_error));
                }
                QMessageBox::warning(this, QStringLiteral("部分邮件保存失败"), msg);
                status_label_->setText(QStringLiteral("接收完成 — %1 封新邮件，%2 封保存失败")
                                           .arg(new_count).arg(failed_count));
            } else {
                status_label_->setText(QStringLiteral("接收完成 — %1 封新邮件").arg(new_count));
            }
            load_emails(current_folder_); update_folder_counts();
        }
        btn_receive_->setEnabled(true); w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([acc]() -> RR {
        Pop3Client pop3; int nc = 0;
        auto emails = pop3.receive_emails(acc.pop3_server, acc.pop3_port, acc.pop3_ssl, acc.username, acc.password, &nc);
        std::string err;
        if (emails.empty()) err = pop3.get_last_error();
        return {emails, err};
    }));
}

void MainWindow::on_compose() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先添加邮箱账号。")); return;
    }
    ComposeDialog dlg(db_mgr_, current_account_id_, this);
    if (dlg.exec() == QDialog::Accepted) {
        Email email = dlg.get_email();
        Account acc = db_mgr_->get_account(current_account_id_);
        status_label_->setText(QStringLiteral("发送中..."));

        typedef std::pair<bool, std::string> SR;
        auto* w = new QFutureWatcher<SR>(this);
        connect(w, &QFutureWatcher<SR>::finished, [this, w, email, acc]() {
            SR r = w->result();
            if (r.first) {
                Email sent = email; sent.account_id = acc.id; sent.folder = "sent";
                auto now = std::time(nullptr); char buf[64];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                sent.received_date = buf;
                int eid = db_mgr_->save_email(sent);
                // 保存附件
                for (const auto& att : sent.attachments) {
                    db_mgr_->add_attachment(eid, att);
                }
                status_label_->setText(QStringLiteral("发送成功！"));
            } else {
                QMessageBox::critical(this, QStringLiteral("发送失败"), QString::fromStdString(r.second));
                status_label_->setText(QStringLiteral("发送失败"));
            }
            load_emails(current_folder_); update_folder_counts(); w->deleteLater();
        });
        w->setFuture(QtConcurrent::run([email, acc]() -> SR {
            MimeEncoder enc; SmtpClient smtp;
            Email e = email; e.body_plain = enc.encode(email);
            bool ok = smtp.send_email(e, acc.smtp_server, acc.smtp_port, acc.smtp_ssl, acc.username, acc.password);
            return {ok, ok ? "" : smtp.get_last_error()};
        }));
    }
}

void MainWindow::on_reply() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0 || current_account_id_ < 0) return;
    Email original = db_mgr_->get_email(email_id);
    ComposeDialog dlg(db_mgr_, current_account_id_, this);
    dlg.set_reply_mode(original, false);
    if (dlg.exec() == QDialog::Accepted) {
        Email email = dlg.get_email();
        Account acc = db_mgr_->get_account(current_account_id_);
        QtConcurrent::run([email, acc]() {
            MimeEncoder enc; SmtpClient smtp;
            Email e = email; e.body_plain = enc.encode(email);
            smtp.send_email(e, acc.smtp_server, acc.smtp_port, acc.smtp_ssl, acc.username, acc.password);
        });
        status_label_->setText(QStringLiteral("回复已发送"));
    }
}

void MainWindow::on_delete_mail() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0) {
        status_label_->setText(QStringLiteral("请先在邮件列表中选择一封邮件"));
        return;
    }
    if (current_folder_ == "trash") {
        // 废纸篓中彻底删除
        auto reply = QMessageBox::question(this, QStringLiteral("彻底删除"),
            QStringLiteral("确定要彻底删除这封邮件吗？\n此操作不可恢复！"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            db_mgr_->delete_permanently(email_id);
            load_emails(current_folder_);
            update_folder_counts();
            status_label_->setText(QStringLiteral("已彻底删除"));
        }
    } else {
        // 其他文件夹移入废纸篓
        auto reply = QMessageBox::question(this, QStringLiteral("确认删除"),
            QStringLiteral("确定删除这封邮件？\n（可在废纸篓中找回）"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            db_mgr_->mark_deleted(email_id);  // 只标记，不改 folder
            load_emails(current_folder_);
            update_folder_counts();
            status_label_->setText(QStringLiteral("已移入废纸篓"));
        }
    }
}

void MainWindow::on_restore_mail() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0) {
        status_label_->setText(QStringLiteral("请先在邮件列表中选择一封邮件"));
        return;
    }
    db_mgr_->mark_undeleted(email_id);  // 取消标记即可，folder 保持原值
    load_emails(current_folder_);
    update_folder_counts();
    status_label_->setText(QStringLiteral("已恢复"));
}

void MainWindow::on_refresh() {
    load_emails(current_folder_); update_folder_counts();
    status_label_->setText(QStringLiteral("已刷新"));
}
