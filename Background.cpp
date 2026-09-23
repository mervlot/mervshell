#include "Background.h"
#include "Theme.h"

#include <QPainter>
#include <QColor>

Background::Background()
{
    // Nothing to set up yet -- setWallpaper() gets called later, from
    // main.cpp, once the widget exists.
}

void Background::setWallpaper(const QString &path)
{
    wallpaper.load(path); // does nothing bad if the file doesn't exist -- wallpaper just stays null

    update(); // ask Qt to call paintEvent() again, now that we have an image
}

void Background::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    if (wallpaper.isNull()) {
        // No wallpaper loaded (or the path was wrong): just use a solid color.
        painter.fillRect(rect(), QColor(Theme::BackgroundColor));
        return;
    }

    // Scale the wallpaper so it always covers the whole screen, then draw
    // it centered. This crops whichever edge ends up sticking out, the
    // same way a phone's wallpaper fills the screen without stretching.
    QPixmap scaled = wallpaper.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);

    int cropX = (scaled.width() - width()) / 2;
    int cropY = (scaled.height() - height()) / 2;

    painter.drawPixmap(0, 0, scaled, cropX, cropY, width(), height());
}
