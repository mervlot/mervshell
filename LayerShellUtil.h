#pragma once

#include <QWidget>
#include <QWindow>
#include <QSize>
#include <LayerShellQt/Window>

// Turns a plain QWidget into a wlr-layer-shell surface: forces the native
// window to exist, grabs its LayerShellQt::Window handle, and applies the
// anchor/exclusive-zone/layer settings every MervShell bar needs.
//
// This is exactly the block that used to be copy-pasted into main() for
// the Dock. Background/Taskbar/Dock now all call this one function
// instead of repeating it three times -- if MervShell ever needs another
// bar, this is the only place its layer-shell setup has to be written.
//
// Call this AFTER the widget's content (layout, labels, buttons, ...) is
// built, and call widget->show() yourself right after -- this function
// only configures the surface, it doesn't show it.
inline LayerShellQt::Window *makeLayerSurface(
    QWidget *widget,
    LayerShellQt::Window::Layer layer,
    QFlags<LayerShellQt::Window::Anchor> anchors,
    int exclusiveZone,
    QSize desiredSize)
{
    // winId() forces Qt to create the underlying native (QWindow) surface
    // right now instead of lazily on first show() -- LayerShellQt::Window
    // needs that QWindow to already exist to attach to it.
    widget->winId();

    QWindow *window = widget->windowHandle();
    if (!window) {
        return nullptr;
    }

    auto *layerWindow = LayerShellQt::Window::get(window);
    layerWindow->setLayer(layer);
    layerWindow->setAnchors(anchors);
    layerWindow->setExclusiveZone(exclusiveZone);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setDesiredSize(desiredSize);

    return layerWindow;
}
