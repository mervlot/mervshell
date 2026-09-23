#pragma once

#include <QWidget>
#include <QLabel>

// The top bar. For now it just shows the current time, updated once a
// second. More widgets (battery, workspace buttons, ...) can be added
// the same way clockLabel is added, later.
class Taskbar : public QWidget
{
public:
    Taskbar();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void updateClock();

    QLabel *clockLabel;
};
