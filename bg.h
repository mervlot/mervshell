#pragma once

#include <QWidget>
#include <QPixmap>
#include <QString>

// The desktop background. A plain, opaque layer-shell surface on the
// "background" layer (i.e. below every normal window) covering the whole
// screen. Kept deliberately simple: a solid fill color, with an optional
// wallpaper image you can drop in later.
class Background : public QWidget
{
public:
    Background();

    // Optional: call this before show() if you want a wallpaper image
    // instead of (or over) the solid fill. Scaled to cover the screen,
    // cropping any overhang, same as a typical desktop wallpaper.
    void setWallpaper(const QString &path);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    QPixmap wallpaper_;
};
