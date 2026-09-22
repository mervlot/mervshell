#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

#include <wayland-client.h>
#include "wlr-ftm.h"

#include <string>
#include <vector>

// One open window's dock button.
struct AppWindow
{
    zwlr_foreign_toplevel_handle_v1 *handle = nullptr;
    std::string title;
    QPushButton *button = nullptr;
};

// The bottom bar: one button per open window, via
// wlr-foreign-toplevel-management. Clicking a button asks the compositor
// to focus that window.
//
// The real bug in the original single-file version was here: the
// per-window listener (title/closed/...) was declared but never attached
// to a handle with zwlr_foreign_toplevel_handle_v1_add_listener(), so the
// compositor never told us any window's title and every button stayed
// blank. Fixed in addApp() below -- see the comment there.
class Dock : public QWidget
{
public:
    Dock();

    void setWayland(wl_display *display, wl_seat *seat);

    // Called once per already-open or newly-opened window (from the
    // manager-level "toplevel" event -- see kManagerListener in Dock.cpp).
    void addApp(zwlr_foreign_toplevel_handle_v1 *handle);
    void removeApp(zwlr_foreign_toplevel_handle_v1 *handle);
    void setTitle(zwlr_foreign_toplevel_handle_v1 *handle, const char *title);
    void activate(zwlr_foreign_toplevel_handle_v1 *handle);

    // The manager-level listener (one "a new window appeared" event).
    // main.cpp attaches this to the toplevel manager with `this` Dock as
    // the listener's data, so it can call addApp() directly -- no global
    // pointer needed.
    static const zwlr_foreign_toplevel_manager_v1_listener kManagerListener;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void rebuild();

    // Per-window listener callbacks (title changed, window closed, ...).
    // Signatures are fixed by the protocol (plain C function pointers),
    // so these have to be static; `data` is the Dock instance, passed in
    // when we call add_listener() in addApp().
    static void handleTitle(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *title);
    static void handleAppId(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *appId);
    static void handleOutputEnter(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output);
    static void handleOutputLeave(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_output *output);
    static void handleState(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_array *state);
    static void handleDone(void *data, zwlr_foreign_toplevel_handle_v1 *handle);
    static void handleClosed(void *data, zwlr_foreign_toplevel_handle_v1 *handle);
    // "parent" (which window this one is a dialog/child of) was added in
    // a later protocol revision than the version your original code was
    // written against. Not needed for a flat dock of top-level windows,
    // but the listener struct must fill in every field or the compiler
    // (rightly) warns that some events are left wired to garbage.
    static void handleParent(void *data, zwlr_foreign_toplevel_handle_v1 *handle,
                              zwlr_foreign_toplevel_handle_v1 *parent);
    static const zwlr_foreign_toplevel_handle_v1_listener kHandleListener;

    // The manager-level "toplevel" callback itself (part of kManagerListener).
    static void handleNewToplevel(void *data, zwlr_foreign_toplevel_manager_v1 *manager,
                                   zwlr_foreign_toplevel_handle_v1 *handle);

    QHBoxLayout *layout_ = nullptr;
    std::vector<AppWindow *> apps_;

    wl_display *display_ = nullptr;
    wl_seat *seat_ = nullptr;
};
