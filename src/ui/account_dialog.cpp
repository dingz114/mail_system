#include "account_dialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include "../network/smtp_client.h"

AccountDialog::AccountDialog(DbManager* db_mgr, QWidget* parent)
    : QDialog(parent), db_mgr_(db_mgr), edit_id_(-1) {
    setWindowTitle(QStringLiteral("账号设置"));
    setMinimumWidth(500);
    setStyleSheet("QDialog { background: #F8FAFC; }");

    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(16);
    main_layout->setContentsMargins(24, 24, 24, 24);

    // 标题
    QLabel* title = new QLabel(QStringLiteral("📧 邮箱账号配置"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 700; color: #1A1A1A; padding-bottom: 8px;");
    main_layout->addWidget(title);

    // --- 基本信息 ---
    QGroupBox* basic_group = new QGroupBox(QStringLiteral("账号信息"), this);
    QFormLayout* basic_form = new QFormLayout(basic_group);
    basic_form->setSpacing(12);
    basic_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText(QStringLiteral("例如：我的 QQ 邮箱"));
    basic_form->addRow(QStringLiteral("账号名称："), name_edit_);

    email_edit_ = new QLineEdit(this);
    email_edit_->setPlaceholderText("user@qq.com");
    basic_form->addRow(QStringLiteral("邮箱地址："), email_edit_);

    username_edit_ = new QLineEdit(this);
    username_edit_->setPlaceholderText(QStringLiteral("通常与邮箱地址相同"));
    basic_form->addRow(QStringLiteral("用户名："), username_edit_);

    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setPlaceholderText(QStringLiteral("授权码（非登录密码）"));
    basic_form->addRow(QStringLiteral("授权码："), password_edit_);

    main_layout->addWidget(basic_group);

    // --- 快速设置 ---
    QHBoxLayout* provider_layout = new QHBoxLayout();
    QLabel* quick_label = new QLabel(QStringLiteral("快速设置："), this);
    quick_label->setStyleSheet("font-weight: 600; color: #555;");
    provider_layout->addWidget(quick_label);

    provider_combo_ = new QComboBox(this);
    provider_combo_->addItem(QStringLiteral("自定义"));
    provider_combo_->addItem(QStringLiteral("QQ 邮箱"));
    provider_combo_->addItem(QStringLiteral("163 邮箱"));
    provider_combo_->addItem(QStringLiteral("126 邮箱"));
    provider_combo_->addItem("Outlook");
    provider_combo_->addItem("Gmail");
    connect(provider_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AccountDialog::on_provider_changed);
    provider_layout->addWidget(provider_combo_);
    provider_layout->addStretch();
    main_layout->addLayout(provider_layout);

    // --- SMTP ---
    QGroupBox* smtp_group = new QGroupBox(QStringLiteral("📤 SMTP 发送服务器"), this);
    QFormLayout* smtp_form = new QFormLayout(smtp_group);
    smtp_form->setSpacing(10);

    smtp_server_edit_ = new QLineEdit("smtp.qq.com", this);
    smtp_form->addRow(QStringLiteral("服务器地址："), smtp_server_edit_);

    smtp_port_spin_ = new QSpinBox(this);
    smtp_port_spin_->setRange(1, 65535);
    smtp_port_spin_->setValue(465);
    smtp_form->addRow(QStringLiteral("端口号："), smtp_port_spin_);

    smtp_ssl_check_ = new QCheckBox(QStringLiteral("使用 SSL/TLS 加密"), this);
    smtp_ssl_check_->setChecked(true);
    smtp_form->addRow(smtp_ssl_check_);

    main_layout->addWidget(smtp_group);

    // --- POP3 ---
    QGroupBox* pop3_group = new QGroupBox(QStringLiteral("📥 POP3 接收服务器"), this);
    QFormLayout* pop3_form = new QFormLayout(pop3_group);
    pop3_form->setSpacing(10);

    pop3_server_edit_ = new QLineEdit("pop.qq.com", this);
    pop3_form->addRow(QStringLiteral("服务器地址："), pop3_server_edit_);

    pop3_port_spin_ = new QSpinBox(this);
    pop3_port_spin_->setRange(1, 65535);
    pop3_port_spin_->setValue(995);
    pop3_form->addRow(QStringLiteral("端口号："), pop3_port_spin_);

    pop3_ssl_check_ = new QCheckBox(QStringLiteral("使用 SSL/TLS 加密"), this);
    pop3_ssl_check_->setChecked(true);
    pop3_form->addRow(pop3_ssl_check_);

    leave_on_server_check_ = new QCheckBox(QStringLiteral("在服务器上保留邮件副本"), this);
    leave_on_server_check_->setChecked(true);
    pop3_form->addRow(leave_on_server_check_);

    main_layout->addWidget(pop3_group);

    // --- 按钮 ---
    QHBoxLayout* btn_layout = new QHBoxLayout();
    btn_layout->setContentsMargins(0, 8, 0, 0);

    test_btn_ = new QPushButton(QStringLiteral("🔍 测试连接"), this);
    test_btn_->setStyleSheet(
        "QPushButton { background: #FFF; border: 1px solid #D9D9D9; border-radius: 6px; "
        "padding: 8px 20px; color: #555; }"
        "QPushButton:hover { border-color: #1890FF; color: #1890FF; }");
    connect(test_btn_, &QPushButton::clicked, this, &AccountDialog::on_test_connection);
    btn_layout->addWidget(test_btn_);

    btn_layout->addStretch();

    cancel_btn_ = new QPushButton(QStringLiteral("取消"), this);
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    save_btn_ = new QPushButton(QStringLiteral("保　存"), this);
    save_btn_->setStyleSheet(
        "QPushButton { background: #1890FF; color: #FFF; border: none; border-radius: 6px; "
        "padding: 8px 28px; font-weight: 600; }"
        "QPushButton:hover { background: #40A9FF; }");
    connect(save_btn_, &QPushButton::clicked, this, &AccountDialog::on_save);
    btn_layout->addWidget(save_btn_);

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
    case 1: // QQ 邮箱
        smtp_server_edit_->setText("smtp.qq.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.qq.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case 2: // 163 邮箱
        smtp_server_edit_->setText("smtp.163.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.163.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case 3: // 126 邮箱
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
        smtp_ssl_check_->setChecked(false);
        pop3_server_edit_->setText("outlook.office365.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case 5: // Gmail
        smtp_server_edit_->setText("smtp.gmail.com");
        smtp_port_spin_->setValue(587);
        smtp_ssl_check_->setChecked(false);
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
    test_btn_->setText(QStringLiteral("测试中..."));

    QFuture<bool> future = QtConcurrent::run([test_acc]() {
        SmtpClient smtp;
        if (!smtp.connect(test_acc.smtp_server, test_acc.smtp_port, test_acc.smtp_ssl)) {
            return false;
        }
        smtp.quit();
        return true;
    });

    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, [this, watcher]() {
        bool success = watcher->result();
        test_btn_->setEnabled(true);
        test_btn_->setText(QStringLiteral("🔍 测试连接"));

        if (success) {
            QMessageBox::information(this, QStringLiteral("连接成功"),
                                     QStringLiteral("SMTP 服务器连接成功！"));
        } else {
            QMessageBox::warning(this, QStringLiteral("连接失败"),
                                 QStringLiteral("无法连接到 SMTP 服务器。\n"
                                                "请检查服务器地址、端口和 SSL 设置。"));
        }
        watcher->deleteLater();
    });

    watcher->setFuture(future);
}

void AccountDialog::on_save() {
    if (email_edit_->text().trimmed().isEmpty() ||
        username_edit_->text().trimmed().isEmpty() ||
        password_edit_->text().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("信息不完整"),
                             QStringLiteral("请填写邮箱地址、用户名和授权码。"));
        return;
    }
    accept();
}
