#include "bg.h"
#include "Theme.h"

#include <QPainter>

Background::Background()
{
    // The background is the one bar that should NOT be translucent --
    // it's the thing everything else shows through to, so it needs to be
    // a fully opaque fill (or wallpaper) covering the whole screen.
}

void Background::setWallpaper(const QString &path)
{
    wallpaper_.load(path);
    update();
}

void Background::paintEvent(QPaintEvent *)
{
    QPainter painter(this);

    if (wallpaper_.isNull()) {
        painter.fillRect(rect(), QColor(Theme::BackgroundColor));
        return;
    }

    // "Cover" scaling: fill the whole widget, cropping whichever
    // dimension overhangs, same behavior as CSS `background-size: cover`.
    QPixmap scaled = wallpaper_.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
    const int x = (scaled.width() - width()) / 2;
    const int y = (scaled.height() - height()) / 2;
    painter.drawPixmap(0, 0, scaled, x, y, width(), height());
}
