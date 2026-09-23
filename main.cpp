#include <QApplication>
#include <QStyleFactory>

#include "LayerShellUtil.h"
#include "Background.h"
#include "Taskbar.h"
#include "Dock.h"

// This is the simplified, learning version of MervShell.
//
// It creates three plain Qt widgets and turns each one into a Wayland
// "layer-shell" surface using LayerShellQt:
//
//   Background -> fills the whole screen, on the "background" layer
//   Taskbar    -> a bar at the top, on the "top" layer
//   Dock       -> a bar at the bottom, on the "top" layer
//
// A layer-shell surface is a special kind of window that sticks to an
// edge (or edges) of the screen instead of floating around like a normal
// application window. "Anchors" describe which edges it sticks to; the
// "layer" decides whether it draws above or below normal windows.
//
// The full-featured version of MervShell -- real running-window
// tracking, icon lookup, pinning apps, dragging the dock, and so on --
// lives on the "advanced" git branch. This branch is deliberately much
// simpler, to make the Qt + Wayland basics easier to see.

int main(int argc, char *argv[])
{
    // Tell Qt to talk to Wayland, and to create layer-shell surfaces
    // instead of normal windows.
    qputenv("QT_QPA_PLATFORM", "wayland");
    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");

    QApplication app(argc, argv);

    // Fusion always draws our buttons using the exact colors we set in
    // their style sheet, regardless of what desktop theme is installed.
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Create our three widgets. At this point they are just plain Qt
    // widgets -- nothing Wayland-specific has happened yet.
    Background background;
    Taskbar taskbar;
    Dock dock;

    // Put an image file named "wallpaper.jpg" in this folder, or change
    // this path to point at any image you like.
    background.setWallpaper("wallpaper.jpg");

    // LayerShellQt::Window::Anchor is a plain enum. ORing two of its
    // values together (AnchorTop | AnchorLeft) normally produces a plain
    // int in C++, and QFlags refuses to accept a plain int back --
    // wrapping the first value in Anchors(...) avoids that error. You
    // don't need to fully understand this line yet; just copy the
    // pattern when you add a new surface.
    using Anchor = LayerShellQt::Window::Anchor;
    using Anchors = QFlags<Anchor>;

    // Background: anchored to all four edges, so it fills the screen.
    makeLayerSurface(
        &background,
        LayerShellQt::Window::LayerBackground,
        Anchors(Anchor::AnchorTop) | Anchor::AnchorBottom |
        Anchor::AnchorLeft | Anchor::AnchorRight,
        -1,          // exclusive zone: -1 means "don't reserve screen space"
        QSize(0, 0)); // 0,0 = let the compositor fill the whole output
    background.show();

    // Taskbar: anchored to the top edge, stretched full width.
    makeLayerSurface(
        &taskbar,
        LayerShellQt::Window::LayerTop,
        Anchors(Anchor::AnchorTop) | Anchor::AnchorLeft | Anchor::AnchorRight,
        32,                 // reserve 32 pixels so windows don't sit under it
        QSize(0, 32));
    taskbar.show();

    // Dock: anchored to the bottom edge, stretched full width.
    makeLayerSurface(
        &dock,
        LayerShellQt::Window::LayerTop,
        Anchors(Anchor::AnchorBottom) | Anchor::AnchorLeft | Anchor::AnchorRight,
        60,
        QSize(0, 60));
    dock.show();

    return app.exec();
}
