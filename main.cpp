#include <QApplication>
#include <QStyleFactory>

#include "WaylandGlobals.h"
#include "LayerShellUtil.h"
#include "bg.h"
#include "Taskbar.h"
#include "Dock.h"

#include <cstdio>

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", "wayland");
    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");

    QApplication app(argc, argv);

    // Forces Fusion, which always paints QPushButton exactly as its
    // stylesheet says. Some system themes (Breeze, Kvantum, ...)
    // partially ignore QSS text colors on buttons, which can look
    // exactly like "the text is invisible" -- this sidesteps that
    // whole class of bug regardless of what desktop theme is installed.
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // --- wlr-foreign-toplevel-management + wl_seat (for the Dock) ------
    WaylandGlobals wayland;
    if (!wayland.connect()) {
        std::printf("mervshell: could not connect to Wayland globals\n");
        return 1;
    }

    // --- Build the three bars -------------------------------------------
    Background background;

    Taskbar taskbar;

    Dock dock;
    background.setWallpaper(
    "/home/mervlot/Wallpaper/photo_2026-09-22_08-33-08.jpg"
); 
    dock.setWayland(wayland.display(), wayland.seat());

    // Attach the manager-level listener now that we have a Dock to hand
    // it events. Every already-open window fires its "toplevel" event
    // during WaylandGlobals::connect()'s second roundtrip, and any
    // future roundtrip picks up newly-opened ones the same way.
    zwlr_foreign_toplevel_manager_v1_add_listener(
        wayland.toplevelManager(), &Dock::kManagerListener, &dock);
    wl_display_roundtrip(wayland.display());

    wayland.pumpEventsWithQt();

    // --- Turn each widget into a real layer-shell surface ---------------
    // Same function, three times -- see LayerShellUtil.h. This is the
    // pattern to copy if you add a fourth bar later.

    // Note the explicit Anchors(...) wrapping the first flag in each
    // group below. LayerShellQt::Window::Anchor is a plain enum; ORing
    // two raw enum values together (`AnchorTop | AnchorBottom`) uses
    // C++'s built-in integer OR and produces a plain `int`, which QFlags
    // refuses to implicitly convert back from (that's the
    // "invalid conversion from 'int' to 'Anchor'" error). Wrapping the
    // first value in Anchors(...) makes every following `|` resolve to
    // QFlags's own operator| instead, which returns Anchors the whole
    // way through.
    using Anchors = QFlags<LayerShellQt::Window::Anchor>;

    makeLayerSurface(
        &background,
        LayerShellQt::Window::LayerBackground,
        Anchors(LayerShellQt::Window::AnchorTop) | LayerShellQt::Window::AnchorBottom |
        LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight,
        -1,               // exclusive zone: background doesn't reserve space
        QSize(0, 0));      // 0,0 = fill the whole output
    background.show();

    makeLayerSurface(
        &taskbar,
        LayerShellQt::Window::LayerTop,
        Anchors(LayerShellQt::Window::AnchorTop) | LayerShellQt::Window::AnchorLeft |
        LayerShellQt::Window::AnchorRight,
        32,
        QSize(0, 32));
    taskbar.show();

    makeLayerSurface(
        &dock,
        LayerShellQt::Window::LayerTop,
        Anchors(LayerShellQt::Window::AnchorBottom) | LayerShellQt::Window::AnchorLeft |
        LayerShellQt::Window::AnchorRight,
        40,
        QSize(0, 40));
    dock.show();

    return app.exec();
}