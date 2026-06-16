#ifndef REMOTE_IMAGE_TEXT_BROWSER_H
#define REMOTE_IMAGE_TEXT_BROWSER_H

#include <QHash>
#include <QNetworkAccessManager>
#include <QTextBrowser>

class RemoteImageTextBrowser : public QTextBrowser {
    Q_OBJECT

public:
    explicit RemoteImageTextBrowser(QWidget* parent = nullptr);

protected:
    QVariant loadResource(int type, const QUrl& name) override;

private:
    QNetworkAccessManager network_;
    QHash<QUrl, QVariant> resource_cache_;
    QHash<QUrl, bool> pending_;
};

#endif // REMOTE_IMAGE_TEXT_BROWSER_H
