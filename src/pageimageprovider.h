#pragma once

#include <QQuickImageProvider>

class Backend;

// Serves rendered PDF pages to QML as image://page/<pageIndex>/<revision>.
// The revision is part of the URL so that opening a different document, or
// re-opening the signed copy, invalidates Qt's pixmap cache.
class PageImageProvider : public QQuickImageProvider {
public:
    explicit PageImageProvider(Backend *backend);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    Backend *m_backend;
};
