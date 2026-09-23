#pragma once

#include <QWidget>

// The bottom dock. For now this is just a row of placeholder buttons --
// clicking one prints a message to the terminal. Later we will make
// these buttons launch real programs and show real open windows (that
// more advanced version already exists on the "advanced" git branch).
class Dock : public QWidget
{
public:
    Dock();

protected:
    void paintEvent(QPaintEvent *event) override;
};
