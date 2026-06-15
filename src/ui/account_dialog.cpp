#include "account_dialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QThread>
#include "../network/smtp_client.h"

AccountDialog::AccountDialog(DbManager* db_mgr, QWidget* parent)
    : QDialog(parent), db_mgr_(db_mgr), edit_id_(-1) {
    setWindowTitle(QString::fromUtf8("Account Settings"));
    setMinimumWidth(450);

    QVBoxLayout* main_layout = new QVBoxLayout(this);

    // Basic info
    QGroupBox* basic_group = new QGroupBox(QString::fromUtf8("Account Information"), this);
    QFormLayout* basic_form = new QFormLayout(basic_group);

    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText(QString::fromUtf8("e.g., My QQ Mail"));
    basic_form->addRow(QString::fromUtf8("Account Name:"), name_edit_);

    email_edit_ = new QLineEdit(this);
    email_edit_->setPlaceholderText("user@example.com");
    basic_form->addRow(QString::fromUtf8("Email Address:"), email_edit_);

    username_edit_ = new QLineEdit(this);
    username_edit_->setPlaceholderText(QString::fromUtf8("Usually same as email"));
    basic_form->addRow(QString::fromUtf8("Username:"), username_edit_);

    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setPlaceholderText(QString::fromUtf8("Password or Authorization Code"));
    basic_form->addRow(QString::fromUtf8("Password:"), password_edit_);

    main_layout->addWidget(basic_group);

    // Provider preset
    QHBoxLayout* provider_layout = new QHBoxLayout();
    provider_layout->addWidget(new QLabel(QString::fromUtf8("Quick Setup:"), this));
    provider_combo_ = new QComboBox(this);
    provider_combo_->addItem(QString::fromUtf8("Custom"));
    provider_combo_->addItem("QQ Mail");
    provider_combo_->addItem("163 Mail");
    provider_combo_->addItem("126 Mail");
    provider_combo_->addItem("Outlook");
    provider_combo_->addItem("Gmail");
    connect(provider_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AccountDialog::on_provider_changed);
    provider_layout->addWidget(provider_combo_);
    provider_layout->addStretch();
    main_layout->addLayout(provider_layout);

    // SMTP
    QGroupBox* smtp_group = new QGroupBox(QString::fromUtf8("SMTP (Outgoing Mail)"), this);
    QFormLayout* smtp_form = new QFormLayout(smtp_group);

    smtp_server_edit_ = new QLineEdit("smtp.qq.com", this);
    smtp_form->addRow(QString::fromUtf8("Server:"), smtp_server_edit_);

    smtp_port_spin_ = new QSpinBox(this);
    smtp_port_spin_->setRange(1, 65535);
    smtp_port_spin_->setValue(465);
    smtp_form->addRow(QString::fromUtf8("Port:"), smtp_port_spin_);

    smtp_ssl_check_ = new QCheckBox(QString::fromUtf8("Use SSL/TLS"), this);
    smtp_ssl_check_->setChecked(true);
    smtp_form->addRow(smtp_ssl_check_);

    main_layout->addWidget(smtp_group);

    // POP3
    QGroupBox* pop3_group = new QGroupBox(QString::fromUtf8("POP3 (Incoming Mail)"), this);
    QFormLayout* pop3_form = new QFormLayout(pop3_group);

    pop3_server_edit_ = new QLineEdit("pop.qq.com", this);
    pop3_form->addRow(QString::fromUtf8("Server:"), pop3_server_edit_);

    pop3_port_spin_ = new QSpinBox(this);
    pop3_port_spin_->setRange(1, 65535);
    pop3_port_spin_->setValue(995);
    pop3_form->addRow(QString::fromUtf8("Port:"), pop3_port_spin_);

    pop3_ssl_check_ = new QCheckBox(QString::fromUtf8("Use SSL/TLS"), this);
    pop3_ssl_check_->setChecked(true);
    pop3_form->addRow(pop3_ssl_check_);

    leave_on_server_check_ = new QCheckBox(QString::fromUtf8("Leave copy on server"), this);
    leave_on_server_check_->setChecked(true);
    pop3_form->addRow(leave_on_server_check_);

    main_layout->addWidget(pop3_group);

    // Buttons
    QHBoxLayout* btn_layout = new QHBoxLayout();
    test_btn_ = new QPushButton(QString::fromUtf8("Test Connection"), this);
    connect(test_btn_, &QPushButton::clicked, this, &AccountDialog::on_test_connection);
    btn_layout->addWidget(test_btn_);

    btn_layout->addStretch();

    save_btn_ = new QPushButton(QString::fromUtf8("Save"), this);
    save_btn_->setDefault(true);
    connect(save_btn_, &QPushButton::clicked, this, &AccountDialog::on_save);
    btn_layout->addWidget(save_btn_);

    cancel_btn_ = new QPushButton(QString::fromUtf8("Cancel"), this);
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    main_layout->addLayout(btn_layout);
}

void AccountDialog::set_account(const Account& acc) {
    edit_id_ = acc.id;
    name_edit_->setText(QString::fromStdString(acc.account_name));
    email_edit_->setText(QString::fromStdString(acc.email_address));
    username_edit_->setText(QString::fromStdString(acc.username));
    password_edit_->setText(QString::fromStdString(acc.password));
    smtp_server_edit_->setText(QString::fromStdString(acc.smtp_server));
    smtp_port_spin_->setValue(acc.smtp_port);
    smtp_ssl_check_->setChecked(acc.smtp_ssl);
    pop3_server_edit_->setText(QString::fromStdString(acc.pop3_server));
    pop3_port_spin_->setValue(acc.pop3_port);
    pop3_ssl_check_->setChecked(acc.pop3_ssl);
    leave_on_server_check_->setChecked(acc.leave_on_server);
}

Account AccountDialog::get_account() const {
    Account acc;
    acc.id = edit_id_;
    acc.account_name = name_edit_->text().toStdString();
    acc.email_address = email_edit_->text().toStdString();
    acc.username = username_edit_->text().toStdString();
    acc.password = password_edit_->text().toStdString();
    acc.smtp_server = smtp_server_edit_->text().toStdString();
    acc.smtp_port = smtp_port_spin_->value();
    acc.smtp_ssl = smtp_ssl_check_->isChecked();
    acc.pop3_server = pop3_server_edit_->text().toStdString();
    acc.pop3_port = pop3_port_spin_->value();
    acc.pop3_ssl = pop3_ssl_check_->isChecked();
    acc.leave_on_server = leave_on_server_check_->isChecked();
    return acc;
}

void AccountDialog::on_provider_changed(int index) {
    switch (index) {
    case 1: // QQ Mail
        smtp_server_edit_->setText("smtp.qq.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.qq.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case 2: // 163 Mail
        smtp_server_edit_->setText("smtp.163.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.163.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case 3: // 126 Mail
        smtp_server_edit_->setText("smtp.126.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.126.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case 4: // Outlook
        smtp_server_edit_->setText("smtp-mail.outlook.com");
        smtp_port_spin_->setValue(587);
        smtp_ssl_check_->setChecked(false);  // STARTTLS
        pop3_server_edit_->setText("outlook.office365.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case 5: // Gmail
        smtp_server_edit_->setText("smtp.gmail.com");
        smtp_port_spin_->setValue(587);
        smtp_ssl_check_->setChecked(false);  // STARTTLS
        pop3_server_edit_->setText("pop.gmail.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    default:
        break;
    }
}

void AccountDialog::on_test_connection() {
    Account test_acc = get_account();
    test_btn_->setEnabled(false);
    test_btn_->setText(QString::fromUtf8("Testing..."));

    // Test SMTP connection
    QFuture<bool> future = QtConcurrent::run([test_acc]() {
        SmtpClient smtp;
        if (!smtp.connect(test_acc.smtp_server, test_acc.smtp_port, test_acc.smtp_ssl)) {
            return false;
        }
        std::string resp = smtp.get_last_response();
        smtp.quit();
        return true;
    });

    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher]() {
        bool success = watcher->result();
        test_btn_->setEnabled(true);
        test_btn_->setText(QString::fromUtf8("Test Connection"));

        if (success) {
            QMessageBox::information(this, QString::fromUtf8("Success"),
                                     QString::fromUtf8("SMTP server connection successful!"));
        } else {
            QMessageBox::warning(this, QString::fromUtf8("Failed"),
                                 QString::fromUtf8("Could not connect to SMTP server.\n"
                                                   "Please check server, port, and SSL settings."));
        }
        watcher->deleteLater();
    });

    watcher->setFuture(future);
}

void AccountDialog::on_save() {
    // Validate required fields
    if (email_edit_->text().trimmed().isEmpty() ||
        username_edit_->text().trimmed().isEmpty() ||
        password_edit_->text().isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("Validation Error"),
                             QString::fromUtf8("Please fill in email, username, and password."));
        return;
    }

    accept();
}
