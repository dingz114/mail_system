#include "remote_image_text_browser.h"

#include <QImage>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextDocument>

RemoteImageTextBrowser::RemoteImageTextBrowser(QWidget* parent)
    : QTextBrowser(parent) {
}

QVariant RemoteImageTextBrowser::loadResource(int type, const QUrl& name) {
    if (type == QTextDocument::ImageResource &&
        (name.scheme() == "http" || name.scheme() == "https")) {
        auto cached = resource_cache_.constFind(name);
        if (cached != resource_cache_.constEnd()) {
            return cached.value();
        }

        if (!pending_.contains(name)) {
            pending_.insert(name, true);
            QNetworkRequest request(name);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);
            request.setRawHeader("User-Agent", "MailSystem/1.0");

            QNetworkReply* reply = network_.get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply, name]() {
                pending_.remove(name);
                if (reply->error() == QNetworkReply::NoError) {
                    const QByteArray bytes = reply->readAll();
                    QImage image;
                    if (image.loadFromData(bytes)) {
                        resource_cache_.insert(name, image);
                        document()->addResource(QTextDocument::ImageResource, name, image);
                        document()->markContentsDirty(0, document()->characterCount());
                        viewport()->update();
                    }
                }
                reply->deleteLater();
            });
        }
    }

    return QTextBrowser::loadResource(type, name);
}
