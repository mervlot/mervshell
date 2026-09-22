#pragma once

#include <QWidget>
#include <QLabel>

// The top bar. Deliberately minimal for now: just a clock and a battery
// percentage, right-aligned. Add more widgets to the layout in the
// constructor the same way clockLabel_/batteryLabel_ are added, and
// they'll line up automatically.
class Taskbar : public QWidget
{
public:
    Taskbar();

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void updateClock();
    void updateBattery();

    // Path to the first battery sysfs entry found (e.g.
    // "/sys/class/power_supply/BAT0"), or empty if this machine has none
    // -- in which case batteryLabel_ just stays hidden.
    QString findBatteryPath() const;

    QLabel *clockLabel_ = nullptr;
    QLabel *batteryLabel_ = nullptr;
    QString batteryPath_;
};
