#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QFont>
#include <QStandardPaths>
#include <iostream>

#include "network/tcp_socket.h"
#include "network/ssl_socket.h"
#include "database/db_connection.h"
#include "database/db_manager.h"
#include "ui/main_window.h"

// Default MySQL connection parameters
// Change these to match your MySQL setup
static const char* DB_HOST = "localhost";
static const int   DB_PORT = 3306;
static const char* DB_USER = "root";
static const char* DB_PASSWORD = "865486";
static const char* DB_NAME = "mail_system";

static void ensure_data_dirs() {
    // Create attachments directory
    QString app_data_path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    dir.mkpath(app_data_path);
    dir.mkpath(app_data_path + "/attachments");
}

int main(int argc, char* argv[]) {
    // Initialize Winsock
    if (!TcpSocket::init_winsock()) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return 1;
    }

    // Initialize OpenSSL
    if (!SslSocket::init_openssl()) {
        std::cerr << "Failed to initialize OpenSSL." << std::endl;
        return 1;
    }

    // Initialize Qt
    QApplication app(argc, argv);
    app.setApplicationName("MailSystem");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CNP");
    QFont::insertSubstitution(QStringLiteral("Fixedsys"), QStringLiteral("Microsoft YaHei UI"));
    QFont::insertSubstitution(QStringLiteral("Modern"), QStringLiteral("Microsoft YaHei UI"));
    QFont::insertSubstitution(QStringLiteral("MS Sans Serif"), QStringLiteral("Microsoft YaHei UI"));
    QFont::insertSubstitution(QStringLiteral("MS Serif"), QStringLiteral("Microsoft YaHei UI"));
    QFont::insertSubstitution(QStringLiteral("Roman"), QStringLiteral("Microsoft YaHei UI"));
    QFont::insertSubstitution(QStringLiteral("Script"), QStringLiteral("Microsoft YaHei UI"));
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));

    // Ensure data directories exist
    ensure_data_dirs();

    // Connect to MySQL database
    DbConnection db_conn;
    if (!db_conn.connect(DB_HOST, DB_PORT, DB_USER, DB_PASSWORD, DB_NAME)) {
        QMessageBox::critical(nullptr,
                              QObject::tr("Database Error"),
                              QObject::tr("Cannot connect to MySQL database.\n"
                                          "Please make sure MySQL server is running.\n\n"
                                          "Host: %1:%2\nDatabase: %3\nError: %4")
                              .arg(DB_HOST).arg(DB_PORT).arg(DB_NAME)
                              .arg(QString::fromStdString(db_conn.get_last_error())));
        return 1;
    }

    DbManager db_mgr(&db_conn);

    // Create and show main window
    MainWindow main_window(&db_mgr);
    main_window.show();

    int result = app.exec();

    // Cleanup
    db_conn.close();
    SslSocket::cleanup_openssl();
    TcpSocket::cleanup_winsock();

    return result;
}
