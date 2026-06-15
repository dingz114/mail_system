#include "account_dialog.h"
#include "app_style.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include "../network/smtp_client.h"
#include "../network/pop3_client.h"

namespace {
constexpr int ProviderCustom = 0;
constexpr int ProviderQQ = 1;
constexpr int Provider163 = 2;
constexpr int Provider126 = 3;
constexpr int ProviderOutlook = 4;
constexpr int ProviderGmail = 5;
constexpr int ProviderWHU = 6;

QString email_domain(const QString& email) {
    const QString lower = email.trimmed().toLower();
    const int at = lower.lastIndexOf('@');
    if (at < 0 || at == lower.size() - 1) return {};
    return lower.mid(at + 1);
}
}

AccountDialog::AccountDialog(DbManager* db_mgr, QWidget* parent)
    : QDialog(parent), db_mgr_(db_mgr), edit_id_(-1) {
    setWindowTitle(QStringLiteral("账号设置"));
    setMinimumWidth(560);
    setMaximumHeight(700);

    // 外层布局：标题 + 可滚动区域 + 按钮
    QVBoxLayout* outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(0, 0, 0, 0);
    outer_layout->setSpacing(0);

    // 固定标题
    QLabel* title = new QLabel(QStringLiteral("邮箱账号配置"), this);
    title->setStyleSheet("font-size: 18px; font-weight: 700; color: #111827; padding: 18px 26px 8px 26px;");
    outer_layout->addWidget(title);

    // 可滚动区域
    QScrollArea* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { border: none; background: #F4F6F8; }");

    QWidget* content = new QWidget();
    content->setStyleSheet("background: #F4F6F8;");
    QVBoxLayout* main_layout = new QVBoxLayout(content);
    main_layout->setSpacing(12);
    main_layout->setContentsMargins(24, 8, 24, 16);

    scroll->setWidget(content);
    outer_layout->addWidget(scroll, 1);

    // --- 基本信息 ---
    QGroupBox* basic_group = new QGroupBox(QStringLiteral("账号信息"), this);
    QFormLayout* basic_form = new QFormLayout(basic_group);
    basic_form->setSpacing(12);
    basic_form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    name_edit_ = new QLineEdit(this);
    name_edit_->setPlaceholderText(QStringLiteral("例如：我的 QQ 邮箱"));
    basic_form->addRow(QStringLiteral("账号名称："), name_edit_);

    email_edit_ = new QLineEdit(this);
    email_edit_->setPlaceholderText("user@qq.com / user@gmail.com");
    basic_form->addRow(QStringLiteral("邮箱地址："), email_edit_);
    connect(email_edit_, &QLineEdit::textChanged, this, &AccountDialog::on_email_changed);

    username_edit_ = new QLineEdit(this);
    username_edit_->setPlaceholderText(QStringLiteral("通常与邮箱地址相同"));
    basic_form->addRow(QStringLiteral("用户名："), username_edit_);

    password_edit_ = new QLineEdit(this);
    password_edit_->setEchoMode(QLineEdit::Password);
    password_edit_->setPlaceholderText(QStringLiteral("按邮箱要求填写授权码、应用专用密码或登录密码"));
    basic_form->addRow(QStringLiteral("授权码或密码："), password_edit_);

    // 授权码提示
    auth_hint_ = new QLabel(
        QStringLiteral("输入邮箱地址后会自动识别常见邮箱。QQ/网易/Gmail 等通常需要先开启 POP3/SMTP，并使用授权码或应用专用密码。"),
        this);
    auth_hint_->setStyleSheet("color: #B45309; font-size: 12px; padding: 4px 0;");
    auth_hint_->setWordWrap(true);
    basic_form->addRow(QString(), auth_hint_);

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
    provider_combo_->addItem(QStringLiteral("武汉大学邮箱"));
    connect(provider_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AccountDialog::on_provider_changed);
    provider_layout->addWidget(provider_combo_);
    provider_layout->addStretch();
    main_layout->addLayout(provider_layout);

    // --- SMTP ---
    QGroupBox* smtp_group = new QGroupBox(QStringLiteral("SMTP 发送服务器"), this);
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
    QGroupBox* pop3_group = new QGroupBox(QStringLiteral("POP3 接收服务器"), this);
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
    QHBoxLayout* test_layout = new QHBoxLayout();
    test_layout->setContentsMargins(0, 4, 0, 0);
    test_layout->setSpacing(8);

    test_smtp_btn_ = new QPushButton(QStringLiteral("测试发送 SMTP"), this);
    UiStyle::applySecondaryButton(test_smtp_btn_, 128);
    connect(test_smtp_btn_, &QPushButton::clicked, this, &AccountDialog::on_test_smtp);
    test_layout->addWidget(test_smtp_btn_);

    test_pop3_btn_ = new QPushButton(QStringLiteral("测试接收 POP3"), this);
    UiStyle::applySecondaryButton(test_pop3_btn_, 128);
    connect(test_pop3_btn_, &QPushButton::clicked, this, &AccountDialog::on_test_pop3);
    test_layout->addWidget(test_pop3_btn_);

    test_layout->addStretch();
    main_layout->addLayout(test_layout);

    // 保存/取消（外层底部固定不随滚动）
    QHBoxLayout* btn_layout = new QHBoxLayout();
    btn_layout->setContentsMargins(24, 8, 24, 14);

    cancel_btn_ = new QPushButton(QStringLiteral("取消"), this);
    cancel_btn_->setFixedHeight(36);
    UiStyle::applyGhostButton(cancel_btn_, 78);
    connect(cancel_btn_, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(cancel_btn_);

    save_btn_ = new QPushButton(QStringLiteral("保存"), this);
    save_btn_->setFixedHeight(36);
    UiStyle::applyPrimaryButton(save_btn_, 92);
    connect(save_btn_, &QPushButton::clicked, this, &AccountDialog::on_save);
    btn_layout->addWidget(save_btn_);

    outer_layout->addLayout(btn_layout);
    update_provider_help(provider_combo_->currentIndex());
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
    last_email_text_ = QString::fromStdString(acc.email_address);

    const int provider = detect_provider_index(last_email_text_);
    {
        QSignalBlocker blocker(provider_combo_);
        provider_combo_->setCurrentIndex(provider);
    }
    update_provider_help(provider);
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
    apply_provider_settings(index);
    update_provider_help(index);
}

void AccountDialog::on_email_changed(const QString& text) {
    const QString email = text.trimmed();

    if (username_edit_->text().trimmed().isEmpty() ||
        username_edit_->text().trimmed() == last_email_text_) {
        username_edit_->setText(email);
    }
    last_email_text_ = email;

    const int provider = detect_provider_index(email);
    if (provider != provider_combo_->currentIndex()) {
        QSignalBlocker blocker(provider_combo_);
        provider_combo_->setCurrentIndex(provider);
        if (provider == ProviderCustom) {
            apply_domain_defaults(email);
        } else {
            apply_provider_settings(provider);
        }
        update_provider_help(provider);
    } else {
        if (provider == ProviderCustom) {
            apply_domain_defaults(email);
        }
        update_provider_help(provider);
    }
}

void AccountDialog::apply_provider_settings(int index) {
    switch (index) {
    case ProviderQQ:
        smtp_server_edit_->setText("smtp.qq.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.qq.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case Provider163:
        smtp_server_edit_->setText("smtp.163.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.163.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case Provider126:
        smtp_server_edit_->setText("smtp.126.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.126.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case ProviderOutlook:
        smtp_server_edit_->setText("smtp-mail.outlook.com");
        smtp_port_spin_->setValue(587);
        smtp_ssl_check_->setChecked(false);
        pop3_server_edit_->setText("outlook.office365.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case ProviderGmail:
        smtp_server_edit_->setText("smtp.gmail.com");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop.gmail.com");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    case ProviderWHU:
        smtp_server_edit_->setText("smtp.whu.edu.cn");
        smtp_port_spin_->setValue(465);
        smtp_ssl_check_->setChecked(true);
        pop3_server_edit_->setText("pop3.whu.edu.cn");
        pop3_port_spin_->setValue(995);
        pop3_ssl_check_->setChecked(true);
        break;
    default:
        break;
    }
}

void AccountDialog::apply_domain_defaults(const QString& email) {
    const QString domain = email_domain(email);
    if (domain.isEmpty() || !domain.contains('.')) return;

    smtp_server_edit_->setText(QStringLiteral("smtp.%1").arg(domain));
    smtp_port_spin_->setValue(465);
    smtp_ssl_check_->setChecked(true);
    pop3_server_edit_->setText(QStringLiteral("pop.%1").arg(domain));
    pop3_port_spin_->setValue(995);
    pop3_ssl_check_->setChecked(true);
}

void AccountDialog::update_provider_help(int index) {
    if (!auth_hint_) return;

    QString text;
    switch (index) {
    case ProviderQQ:
        text = QStringLiteral("QQ 邮箱：需在网页邮箱设置中开启 POP3/SMTP 服务，密码栏填写“授权码/独立密码”，用户名通常填写完整邮箱地址。");
        break;
    case Provider163:
        text = QStringLiteral("163 邮箱：需在网页邮箱设置中开启 POP3/SMTP/IMAP 服务，密码栏填写客户端授权密码，不是网页登录密码。");
        break;
    case Provider126:
        text = QStringLiteral("126 邮箱：需在网页邮箱设置中开启 POP3/SMTP/IMAP 服务，密码栏填写客户端授权密码，不是网页登录密码。");
        break;
    case ProviderOutlook:
        text = QStringLiteral("Outlook/Hotmail：个人账号可尝试网页登录密码；开启两步验证时通常要用应用密码。部分账号会要求 OAuth，本客户端暂不支持网页授权。");
        break;
    case ProviderGmail:
        text = QStringLiteral("Gmail：需在 Gmail 设置中开启 POP，并使用 Google 应用专用密码；SMTP 默认使用 465 + SSL/TLS。普通 Google 密码通常不能用于本客户端。");
        break;
    case ProviderWHU:
        text = QStringLiteral("武汉大学邮箱：按学校官方客户端设置，SMTP 为 smtp.whu.edu.cn，POP3 为 pop3.whu.edu.cn，用户名填写完整邮箱地址。");
        break;
    default:
        text = QStringLiteral("自定义邮箱：会按邮箱后缀自动尝试 smtp.<后缀> 和 pop.<后缀>，端口默认 465/995 SSL/TLS；不适用时请手动改成服务商提供的地址。");
        break;
    }
    auth_hint_->setText(text);
}

int AccountDialog::detect_provider_index(const QString& email) const {
    const QString domain = email_domain(email);
    if (domain.isEmpty()) return ProviderCustom;

    if (domain == "qq.com" || domain == "foxmail.com") {
        return ProviderQQ;
    }
    if (domain == "163.com" || domain == "yeah.net" || domain == "188.com") {
        return Provider163;
    }
    if (domain == "126.com") {
        return Provider126;
    }
    if (domain == "outlook.com" || domain == "hotmail.com" ||
        domain == "live.com" || domain == "msn.com") {
        return ProviderOutlook;
    }
    if (domain == "gmail.com" || domain == "googlemail.com") {
        return ProviderGmail;
    }
    if (domain == "whu.edu.cn") {
        return ProviderWHU;
    }
    return ProviderCustom;
}

void AccountDialog::on_test_smtp() {
    Account test_acc = get_account();
    test_smtp_btn_->setEnabled(false);
    test_smtp_btn_->setText(QStringLiteral("SMTP 测试中..."));

    QFuture<std::string> future = QtConcurrent::run([test_acc]() -> std::string {
        auto test_once = [&test_acc](const std::string& server, int port, bool use_ssl) {
            SmtpClient smtp;
            if (smtp.test_login(server, port, use_ssl, test_acc.username, test_acc.password)) {
                return std::pair<bool, std::string>(true, "SMTP 登录验证通过");
            }
            std::string err = smtp.get_last_error();
            if (err.empty()) err = smtp.get_last_response();
            return std::pair<bool, std::string>(false, err.empty() ? "未收到服务器响应" : err);
        };

        std::string lower_server = test_acc.smtp_server;
        std::transform(lower_server.begin(), lower_server.end(), lower_server.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower_server == "smtp.gmail.com") {
            struct Candidate {
                int port;
                bool use_ssl;
                std::string label;
            };

            const Candidate candidates[] = {
                {test_acc.smtp_port, test_acc.smtp_ssl,
                 std::to_string(test_acc.smtp_port) +
                     (test_acc.smtp_ssl ? " SSL/TLS" : " STARTTLS")},
                {465, true, "465 SSL/TLS"},
                {587, false, "587 STARTTLS"}
            };

            std::string failures;
            for (const auto& candidate : candidates) {
                const bool is_duplicate =
                    &candidate != &candidates[0] &&
                    candidate.port == candidates[0].port &&
                    candidate.use_ssl == candidates[0].use_ssl;
                if (is_duplicate) continue;

                auto result = test_once(test_acc.smtp_server, candidate.port, candidate.use_ssl);
                if (result.first) {
                    return "成功: " + result.second + "（" + candidate.label + "）";
                }
                failures += "\n" + candidate.label + ": " + result.second;
            }

            return "失败: Gmail SMTP 两种官方端口均未通过。" + failures +
                   "\n如果 POP3 已通过，多半是当前代理/TUN/防火墙没有正确转发 SMTP 原始 TCP。";
        }

        auto result = test_once(test_acc.smtp_server, test_acc.smtp_port, test_acc.smtp_ssl);
        if (!result.first) {
            return "失败: " + result.second;
        }
        return "成功: " + result.second;
    });

    auto* watcher = new QFutureWatcher<std::string>(this);
    connect(watcher, &QFutureWatcher<std::string>::finished, [this, watcher]() {
        std::string result = watcher->result();
        test_smtp_btn_->setEnabled(true);
        test_smtp_btn_->setText(QStringLiteral("测试发送 SMTP"));

        if (result.find("成功") == 0) {
            QMessageBox::information(this, QStringLiteral("SMTP 测试通过"),
                                     QString::fromStdString(result));
        } else {
            QMessageBox::warning(this, QStringLiteral("SMTP 测试失败"),
                                 QString::fromStdString(result));
        }
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void AccountDialog::on_test_pop3() {
    Account test_acc = get_account();
    test_pop3_btn_->setEnabled(false);
    test_pop3_btn_->setText(QStringLiteral("POP3 测试中..."));

    QFuture<std::string> future = QtConcurrent::run([test_acc]() -> std::string {
        Pop3Client pop3;
        if (!pop3.connect(test_acc.pop3_server, test_acc.pop3_port, test_acc.pop3_ssl)) {
            return "连接失败: " + pop3.get_last_error();
        }
        pop3.recv_line();
        if (!pop3.is_success()) {
            return "服务器问候失败: " + pop3.get_last_response();
        }
        if (!pop3.user(test_acc.username)) {
            return "用户名验证失败: " + pop3.get_last_response();
        }
        if (!pop3.pass(test_acc.password)) {
            return "授权码/密码验证失败: " + pop3.get_last_response();
        }
        int count = 0;
        int total_size = 0;
        if (!pop3.stat(&count, &total_size)) {
            return "STAT 失败: " + pop3.get_last_response();
        }
        std::string resp = pop3.get_last_response();
        pop3.quit();
        if (resp.empty()) return "未收到服务器响应";
        return "成功: " + resp + "，邮箱中共 " + std::to_string(count) + " 封邮件";
    });

    auto* watcher = new QFutureWatcher<std::string>(this);
    connect(watcher, &QFutureWatcher<std::string>::finished, [this, watcher]() {
        std::string result = watcher->result();
        test_pop3_btn_->setEnabled(true);
        test_pop3_btn_->setText(QStringLiteral("测试接收 POP3"));

        if (result.find("成功") == 0) {
            QMessageBox::information(this, QStringLiteral("POP3 测试通过"),
                                     QString::fromStdString(result));
        } else {
            QMessageBox::warning(this, QStringLiteral("POP3 测试失败"),
                                 QString::fromStdString(result));
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
                             QStringLiteral("请填写邮箱地址、用户名和授权码或密码。"));
        return;
    }
    accept();
}
