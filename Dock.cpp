#include "Dock.h"
#include "Theme.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QPainter>
#include <QColor>
#include <QDebug>

// One button style, shared by every button in the dock: a background
// color, rounded corners, and a lighter color while the mouse hovers
// over it. Written as a plain function (not a class) since it doesn't
// need to remember anything -- it just builds a string.
static QString dockButtonStyle()
{
    return QString(
        "QPushButton {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  border-radius: %3px;"
        "  padding: 6px 16px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %4;"
        "}"
    ).arg(Theme::ButtonBackground, Theme::TextColor)
     .arg(Theme::ButtonRadius)
     .arg(Theme::ButtonHover);
}

Dock::Dock()
{
    setAttribute(Qt::WA_TranslucentBackground);
    resize(1, Theme::DockHeight); // real size comes from the layer-shell surface, set in main.cpp

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(Theme::Margin, Theme::Margin, Theme::Margin, Theme::Margin);
    layout->setSpacing(Theme::Spacing);

    QPushButton *terminalButton = new QPushButton("Terminal");
    terminalButton->setStyleSheet(dockButtonStyle());
    terminalButton->setFixedHeight(Theme::ButtonHeight);
    layout->addWidget(terminalButton);

    QPushButton *filesButton = new QPushButton("Files");
    filesButton->setStyleSheet(dockButtonStyle());
    filesButton->setFixedHeight(Theme::ButtonHeight);
    layout->addWidget(filesButton);

    QPushButton *browserButton = new QPushButton("Browser");
    browserButton->setStyleSheet(dockButtonStyle());
    browserButton->setFixedHeight(Theme::ButtonHeight);
    layout->addWidget(browserButton);

    // connect() wires a button's "clicked" signal to a small function
    // that runs whenever the button is pressed. The [] {...} part is a
    // lambda -- a tiny function written inline instead of given its own
    // name. For now these just print a message; later they will launch
    // real programs (QProcess::startDetached, when we get to that).
    connect(terminalButton, &QPushButton::clicked, [] {
        qDebug() << "Terminal button clicked";
    });
    connect(filesButton, &QPushButton::clicked, [] {
        qDebug() << "Files button clicked";
    });
    connect(browserButton, &QPushButton::clicked, [] {
        qDebug() << "Browser button clicked";
    });
}

void Dock::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor(Theme::BarBackground));
}
