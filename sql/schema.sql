-- Mail System Database Schema
-- MySQL 8.0+

CREATE DATABASE IF NOT EXISTS mail_system
    CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE mail_system;

-- Email accounts configured by the user
CREATE TABLE IF NOT EXISTS accounts (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    account_name    VARCHAR(100) NOT NULL,
    email_address   VARCHAR(255) NOT NULL UNIQUE,
    username        VARCHAR(255) NOT NULL,
    password        VARCHAR(255) NOT NULL,
    smtp_server     VARCHAR(255) NOT NULL,
    smtp_port       INT NOT NULL DEFAULT 465,
    smtp_ssl        TINYINT(1) NOT NULL DEFAULT 1,
    pop3_server     VARCHAR(255) NOT NULL,
    pop3_port       INT NOT NULL DEFAULT 995,
    pop3_ssl        TINYINT(1) NOT NULL DEFAULT 1,
    leave_on_server TINYINT(1) NOT NULL DEFAULT 1,
    created_at      DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- Emails (received, sent, drafts, trash)
CREATE TABLE IF NOT EXISTS emails (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    account_id      INT NOT NULL,
    folder          ENUM('inbox','sent','drafts','trash') NOT NULL DEFAULT 'inbox',
    message_id      VARCHAR(512),
    pop3_uid        VARCHAR(255),
    -- Headers
    sender_name     VARCHAR(255) CHARACTER SET utf8mb4,
    sender_addr     VARCHAR(255) CHARACTER SET utf8mb4 NOT NULL,
    recipients_to   TEXT CHARACTER SET utf8mb4 NOT NULL,
    recipients_cc   TEXT CHARACTER SET utf8mb4,
    recipients_bcc  TEXT CHARACTER SET utf8mb4,
    subject         VARCHAR(998) CHARACTER SET utf8mb4,
    -- Body
    body_plain      LONGTEXT CHARACTER SET utf8mb4,
    body_html       LONGTEXT CHARACTER SET utf8mb4,
    -- Flags
    is_read         TINYINT(1) NOT NULL DEFAULT 0,
    is_deleted      TINYINT(1) NOT NULL DEFAULT 0,
    is_flagged      TINYINT(1) NOT NULL DEFAULT 0,
    has_attachments TINYINT(1) NOT NULL DEFAULT 0,
    -- Metadata
    received_date   DATETIME NOT NULL,
    created_at      DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at      DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE,
    INDEX idx_account_folder (account_id, folder),
    INDEX idx_account_deleted (account_id, is_deleted)
) ENGINE=InnoDB;

-- Attachments metadata (files stored on filesystem)
CREATE TABLE IF NOT EXISTS attachments (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    email_id        INT NOT NULL,
    file_name       VARCHAR(255) NOT NULL,
    file_path       VARCHAR(512) NOT NULL,
    mime_type       VARCHAR(255),
    file_size       BIGINT NOT NULL DEFAULT 0,
    content_id      VARCHAR(255),
    FOREIGN KEY (email_id) REFERENCES emails(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- Contacts / Address book
CREATE TABLE IF NOT EXISTS contacts (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    account_id      INT NOT NULL,
    display_name    VARCHAR(255),
    email_address   VARCHAR(255) NOT NULL,
    created_at      DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE,
    UNIQUE KEY uk_account_email (account_id, email_address)
) ENGINE=InnoDB;

-- Per-account POP3 sync tracking (which UIDs already retrieved)
CREATE TABLE IF NOT EXISTS sync_state (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    account_id      INT NOT NULL,
    pop3_uid        VARCHAR(255) NOT NULL,
    email_id        INT NOT NULL,
    retrieved_at    DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE,
    FOREIGN KEY (email_id) REFERENCES emails(id) ON DELETE CASCADE,
    UNIQUE KEY uk_account_pop3uid (account_id, pop3_uid)
) ENGINE=InnoDB;

-- ============================================================
-- 升级迁移：将已有表的文本列改为 utf8mb4（支持 emoji 等 4 字节字符）
-- 如果建表时未指定 CHARACTER SET，运行以下语句修复：
-- ============================================================
-- ALTER TABLE emails
--   MODIFY sender_name     VARCHAR(255) CHARACTER SET utf8mb4,
--   MODIFY sender_addr     VARCHAR(255) CHARACTER SET utf8mb4 NOT NULL,
--   MODIFY recipients_to   TEXT         CHARACTER SET utf8mb4 NOT NULL,
--   MODIFY recipients_cc   TEXT         CHARACTER SET utf8mb4,
--   MODIFY recipients_bcc  TEXT         CHARACTER SET utf8mb4,
--   MODIFY subject         VARCHAR(998) CHARACTER SET utf8mb4,
--   MODIFY body_plain      LONGTEXT     CHARACTER SET utf8mb4,
--   MODIFY body_html       LONGTEXT     CHARACTER SET utf8mb4;
