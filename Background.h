#pragma once

#include <QWidget>
#include <QPixmap>
#include <QString>

// The desktop background. Shows a wallpaper image if one is set with
// setWallpaper(), otherwise just fills the screen with a solid color.
class Background : public QWidget
{
public:
    Background();

    void setWallpaper(const QString &path);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap wallpaper;
};
