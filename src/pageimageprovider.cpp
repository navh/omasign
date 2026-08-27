#include "pageimageprovider.h"

#include "backend.h"

PageImageProvider::PageImageProvider(Backend *backend)
    : QQuickImageProvider(QQuickImageProvider::Image), m_backend(backend)
{
}

QImage PageImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int pageIndex = id.section(QLatin1Char('/'), 0, 0).toInt();
    const QImage image = m_backend ? m_backend->renderPage(pageIndex, requestedSize) : QImage();
    if (size)
        *size = image.size();
    return image;
}
