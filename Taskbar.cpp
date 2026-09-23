#include "Taskbar.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QColor>
#include <QTimer>
#include <QDateTime>

Taskbar::Taskbar()
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(1, Theme::TaskbarHeight); // real size comes from the layer-shell surface, set in main.cpp

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(Theme::Margin, 0, Theme::Margin, 0);

    clockLabel = new QLabel(this);
    clockLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(Theme::TextColor));

    // addStretch() adds empty space that grows to fill the layout --
    // putting one before the label pushes it to the right side of the bar.
    layout->addStretch();
    layout->addWidget(clockLabel);

    updateClock();

    // A QTimer that fires every 1000 milliseconds (1 second). Each time
    // it fires, it emits the "timeout" signal, which we connect to our
    // updateClock() function.
    QTimer *clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &Taskbar::updateClock);
    clockTimer->start(1000);
}

void Taskbar::updateClock()
{
    clockLabel->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
}

void Taskbar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(Theme::BarBackground));
}
