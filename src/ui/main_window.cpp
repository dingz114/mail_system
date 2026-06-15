#include "main_window.h"
#include "email_list_widget.h"
#include "email_view_widget.h"
#include "compose_dialog.h"
#include "account_dialog.h"
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

MainWindow::MainWindow(DbManager* db_mgr, QWidget* parent)
    : QMainWindow(parent), db_mgr_(db_mgr), current_account_id_(-1), current_folder_("inbox") {
    setup_ui();
    setup_toolbar();
    setup_menu();
    load_accounts();

    setWindowTitle(QString::fromUtf8("Mail System v1.0"));
    resize(1100, 700);
    status_label_->setText(QString::fromUtf8("Ready"));
}

MainWindow::~MainWindow() {}

void MainWindow::setup_ui() {
    // Central widget
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QHBoxLayout* main_layout = new QHBoxLayout(central);
    main_layout->setContentsMargins(4, 4, 4, 4);

    // Left panel: folder tree
    folder_tree_ = new QTreeWidget(this);
    folder_tree_->setHeaderLabel(QString::fromUtf8("Folders"));
    folder_tree_->setFixedWidth(180);

    inbox_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QString::fromUtf8("Inbox")));
    sent_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QString::fromUtf8("Sent")));
    drafts_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QString::fromUtf8("Drafts")));
    trash_item_ = new QTreeWidgetItem(folder_tree_, QStringList(QString::fromUtf8("Trash")));

    folder_tree_->expandAll();
    connect(folder_tree_, &QTreeWidget::itemClicked, this, &MainWindow::on_folder_changed);

    // Right side: email list + email view
    email_list_ = new EmailListWidget(this);
    email_view_ = new EmailViewWidget(this);

    right_splitter_ = new QSplitter(Qt::Vertical, this);
    right_splitter_->addWidget(email_list_);
    right_splitter_->addWidget(email_view_);
    right_splitter_->setStretchFactor(0, 2);
    right_splitter_->setStretchFactor(1, 3);

    main_splitter_ = new QSplitter(Qt::Horizontal, this);
    main_splitter_->addWidget(folder_tree_);
    main_splitter_->addWidget(right_splitter_);
    main_layout->addWidget(main_splitter_);

    connect(email_list_, &EmailListWidget::email_selected, this, &MainWindow::on_email_selected);

    // Status bar
    status_label_ = new QLabel(this);
    statusBar()->addWidget(status_label_);
}

void MainWindow::setup_toolbar() {
    toolbar_ = addToolBar(QString::fromUtf8("Main"));

    // Account selector
    QLabel* account_label = new QLabel(QString::fromUtf8(" Account: "), this);
    toolbar_->addWidget(account_label);
    account_combo_ = new QComboBox(this);
    account_combo_->setMinimumWidth(180);
    toolbar_->addWidget(account_combo_);
    connect(account_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::on_account_changed);

    toolbar_->addSeparator();

    btn_receive_ = new QPushButton(QString::fromUtf8("Receive"), this);
    toolbar_->addWidget(btn_receive_);
    connect(btn_receive_, &QPushButton::clicked, this, &MainWindow::on_receive);

    btn_compose_ = new QPushButton(QString::fromUtf8("Compose"), this);
    toolbar_->addWidget(btn_compose_);
    connect(btn_compose_, &QPushButton::clicked, this, &MainWindow::on_compose);

    btn_reply_ = new QPushButton(QString::fromUtf8("Reply"), this);
    toolbar_->addWidget(btn_reply_);
    connect(btn_reply_, &QPushButton::clicked, this, &MainWindow::on_reply);

    btn_delete_ = new QPushButton(QString::fromUtf8("Delete"), this);
    toolbar_->addWidget(btn_delete_);
    connect(btn_delete_, &QPushButton::clicked, this, &MainWindow::on_delete_mail);

    btn_refresh_ = new QPushButton(QString::fromUtf8("Refresh"), this);
    toolbar_->addWidget(btn_refresh_);
    connect(btn_refresh_, &QPushButton::clicked, this, &MainWindow::on_refresh);
}

void MainWindow::setup_menu() {
    QMenu* file_menu = menuBar()->addMenu(QString::fromUtf8("&File"));

    QAction* add_account_action = file_menu->addAction(QString::fromUtf8("Add Account..."));
    connect(add_account_action, &QAction::triggered, [this]() {
        AccountDialog dlg(db_mgr_, this);
        if (dlg.exec() == QDialog::Accepted) {
            Account acc = dlg.get_account();
            db_mgr_->add_account(acc);
            load_accounts();
        }
    });

    QAction* edit_account_action = file_menu->addAction(QString::fromUtf8("Edit Account..."));
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

    file_menu->addSeparator();

    QAction* exit_action = file_menu->addAction(QString::fromUtf8("E&xit"));
    exit_action->setShortcut(QKeySequence::Quit);
    connect(exit_action, &QAction::triggered, qApp, &QApplication::quit);

    QMenu* help_menu = menuBar()->addMenu(QString::fromUtf8("&Help"));
    QAction* about_action = help_menu->addAction(QString::fromUtf8("About"));
    connect(about_action, &QAction::triggered, [this]() {
        QMessageBox::about(this, QString::fromUtf8("About Mail System"),
                           QString::fromUtf8("Mail System v1.0\n"
                                             "SMTP & POP3 Email Client\n"
                                             "Computer Network Practice Project\n\n"
                                             "C++ / Qt / MySQL / OpenSSL"));
    });
}

void MainWindow::load_accounts() {
    accounts_ = db_mgr_->get_all_accounts();
    account_combo_->blockSignals(true);
    account_combo_->clear();
    for (const auto& acc : accounts_) {
        account_combo_->addItem(QString::fromStdString(acc.account_name + " (" + acc.email_address + ")"));
    }
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

    if (item == inbox_item_) current_folder_ = "inbox";
    else if (item == sent_item_) current_folder_ = "sent";
    else if (item == drafts_item_) current_folder_ = "drafts";
    else if (item == trash_item_) current_folder_ = "trash";

    load_emails(current_folder_);
}

void MainWindow::load_emails(const std::string& folder) {
    if (current_account_id_ < 0) {
        email_list_->clear();
        email_view_->clear();
        return;
    }

    auto emails = db_mgr_->get_emails(current_account_id_, folder);
    email_list_->set_emails(emails);
    email_view_->clear();
    status_label_->setText(QString::fromStdString(
        std::to_string(emails.size()) + " messages in " + folder));
}

void MainWindow::update_folder_counts() {
    if (current_account_id_ < 0) return;

    int inbox_count = db_mgr_->get_total_count(current_account_id_, "inbox");
    int unread_count = db_mgr_->get_unread_count(current_account_id_, "inbox");

    inbox_item_->setText(0, QString::fromUtf8("Inbox") +
                         (unread_count > 0 ? QString(" (%1)").arg(unread_count) : ""));

    int sent_count = db_mgr_->get_total_count(current_account_id_, "sent");
    sent_item_->setText(0, QString::fromUtf8("Sent") +
                        (sent_count > 0 ? QString(" (%1)").arg(sent_count) : ""));

    int drafts_count = db_mgr_->get_total_count(current_account_id_, "drafts");
    drafts_item_->setText(0, QString::fromUtf8("Drafts") +
                          (drafts_count > 0 ? QString(" (%1)").arg(drafts_count) : ""));
}

void MainWindow::on_email_selected(int email_id) {
    if (email_id < 0) return;

    Email email = db_mgr_->get_email(email_id);
    email_view_->show_email(email);

    if (!email.is_read) {
        db_mgr_->mark_read(email_id);
        update_folder_counts();
    }
}

void MainWindow::on_receive() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8("Warning"),
                             QString::fromUtf8("Please add an email account first."));
        return;
    }

    Account acc = db_mgr_->get_account(current_account_id_);
    status_label_->setText(QString::fromUtf8("Connecting to POP3 server..."));

    // Run POP3 receive in background thread
    auto* watcher = new QFutureWatcher<std::vector<Email>>(this);
    connect(watcher, &QFutureWatcher<std::vector<Email>>::finished, [this, watcher, acc]() {
        auto emails = watcher->result();

        MimeDecoder decoder;
        int new_count = 0;

        for (auto& email : emails) {
            // Check UIDL dedup
            if (!email.pop3_uid.empty() &&
                db_mgr_->is_uid_downloaded(acc.id, email.pop3_uid)) {
                continue;
            }

            // Parse raw email
            email = decoder.decode(email.body_plain, acc.id);
            email.folder = "inbox";
            email.received_date = email.received_date.empty() ? "now" : email.received_date;

            int email_id = db_mgr_->save_email(email);
            if (email_id > 0) {
                if (!email.pop3_uid.empty()) {
                    db_mgr_->mark_uid_downloaded(acc.id, email.pop3_uid, email_id);
                }
                new_count++;
            }
        }

        status_label_->setText(QString::fromStdString(
            "Received " + std::to_string(new_count) + " new messages"));
        load_emails(current_folder_);
        update_folder_counts();

        watcher->deleteLater();
    });

    QFuture<std::vector<Email>> future = QtConcurrent::run([acc]() {
        Pop3Client pop3;
        int new_count = 0;
        auto emails = pop3.receive_emails(
            acc.pop3_server, acc.pop3_port, acc.pop3_ssl,
            acc.username, acc.password, &new_count);
        return emails;
    });

    watcher->setFuture(future);
}

void MainWindow::on_compose() {
    if (current_account_id_ < 0) {
        QMessageBox::warning(this, QString::fromUtf8("Warning"),
                             QString::fromUtf8("Please add an email account first."));
        return;
    }

    ComposeDialog dlg(db_mgr_, current_account_id_, this);
    if (dlg.exec() == QDialog::Accepted) {
        Email email = dlg.get_email();
        Account acc = db_mgr_->get_account(current_account_id_);

        status_label_->setText(QString::fromUtf8("Sending email..."));

        // Run SMTP send in background thread
        auto* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher, email, acc]() {
            bool success = watcher->result();
            if (success) {
                // Save to sent folder
                Email sent_email = email;
                sent_email.account_id = acc.id;
                sent_email.folder = "sent";
                // Set current date
                auto now = std::time(nullptr);
                char buf[64];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
                sent_email.received_date = buf;

                db_mgr_->save_email(sent_email);
                status_label_->setText(QString::fromUtf8("Email sent successfully!"));
                load_emails(current_folder_);
                update_folder_counts();
            } else {
                QMessageBox::critical(this, QString::fromUtf8("Error"),
                                      QString::fromUtf8("Failed to send email.\nPlease check your SMTP settings and network connection."));
                status_label_->setText(QString::fromUtf8("Send failed"));
            }
            watcher->deleteLater();
        });

        QFuture<bool> future = QtConcurrent::run([email, acc]() {
            MimeEncoder encoder;
            std::string mime = encoder.encode(email);

            SmtpClient smtp;
            Email enc_email = email;
            enc_email.body_plain = mime;

            return smtp.send_email(enc_email,
                                   acc.smtp_server, acc.smtp_port, acc.smtp_ssl,
                                   acc.username, acc.password);
        });

        watcher->setFuture(future);
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

        MimeEncoder encoder;
        std::string mime = encoder.encode(email);

        QFuture<bool> future = QtConcurrent::run([email, acc, mime]() {
            SmtpClient smtp;
            Email enc_email = email;
            enc_email.body_plain = mime;
            return smtp.send_email(enc_email,
                                   acc.smtp_server, acc.smtp_port, acc.smtp_ssl,
                                   acc.username, acc.password);
        });

        (void)future; // Fire and forget (reply sent in background)
        status_label_->setText(QString::fromUtf8("Reply sent..."));
    }
}

void MainWindow::on_delete_mail() {
    int email_id = email_list_->current_email_id();
    if (email_id < 0) return;

    auto reply = QMessageBox::question(this, QString::fromUtf8("Confirm Delete"),
                                        QString::fromUtf8("Are you sure you want to delete this email?"),
                                        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        db_mgr_->mark_deleted(email_id);
        load_emails(current_folder_);
        update_folder_counts();
        status_label_->setText(QString::fromUtf8("Email deleted"));
    }
}

void MainWindow::on_refresh() {
    load_emails(current_folder_);
    update_folder_counts();
}
