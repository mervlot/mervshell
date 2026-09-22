#include "Taskbar.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

Taskbar::Taskbar()
{
    setAttribute(Qt::WA_TranslucentBackground);

    resize(1, 32); // real width/height arrive via the layer-shell configure

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(Theme::BarMargin, Theme::BarMargin,
                                Theme::BarMargin, Theme::BarMargin);
    layout->setSpacing(Theme::BarSpacing * 2);

    // Right-aligned status cluster: stretch pushes everything else to the
    // right edge. Add new widgets between the stretch and the labels
    // below to grow this cluster (a launcher button would go the other
    // side, before the stretch).
    layout->addStretch();

    batteryLabel_ = new QLabel(this);
    batteryLabel_->setStyleSheet(QString("color: %1;").arg(Theme::MutedTextColor));
    layout->addWidget(batteryLabel_);

    clockLabel_ = new QLabel(this);
    clockLabel_->setStyleSheet(QString("color: %1; font-weight: 600;").arg(Theme::TextColor));
    layout->addWidget(clockLabel_);

    batteryPath_ = findBatteryPath();
    batteryLabel_->setVisible(!batteryPath_.isEmpty());

    updateClock();
    updateBattery();

    auto *clockTimer = new QTimer(this);
    QObject::connect(clockTimer, &QTimer::timeout, this, &Taskbar::updateClock);
    clockTimer->start(1000); // once a second is plenty for HH:mm

    if (!batteryPath_.isEmpty()) {
        auto *batteryTimer = new QTimer(this);
        QObject::connect(batteryTimer, &QTimer::timeout, this, &Taskbar::updateBattery);
        batteryTimer->start(30000); // battery % doesn't need per-second polling
    }
}

void Taskbar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(Theme::TaskBackground));
}

void Taskbar::updateClock()
{
    clockLabel_->setText(QDateTime::currentDateTime().toString("HH:mm"));
}

QString Taskbar::findBatteryPath() const
{
    QDir powerSupply("/sys/class/power_supply");
    if (!powerSupply.exists()) return {};

    const QStringList batteries = powerSupply.entryList(QStringList() << "BAT*", QDir::Dirs);
    if (batteries.isEmpty()) return {};

    return powerSupply.absoluteFilePath(batteries.first());
}

void Taskbar::updateBattery()
{
    if (batteryPath_.isEmpty()) return;

    QFile capacityFile(batteryPath_ + "/capacity");
    if (!capacityFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        batteryLabel_->setVisible(false);
        return;
    }

    QTextStream stream(&capacityFile);
    const QString capacity = stream.readAll().trimmed();

    QString statusGlyph = "";
    QFile statusFile(batteryPath_ + "/status");
    if (statusFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString status = QTextStream(&statusFile).readAll().trimmed();
        if (status == "Charging") statusGlyph = "+";
    }

    batteryLabel_->setText(QString("%1%2%").arg(statusGlyph, capacity));
}
