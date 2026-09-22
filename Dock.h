#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QIcon>
#include <QColor>

#include <wayland-client.h>
#include "wlr-ftm.h"

#include <string>
#include <vector>

// A resolved dock icon.
//
//   icon     the icon itself; null => draw the letter placeholder instead
//   backing  a chip color to paint behind the icon, or an invalid QColor
//            to draw it bare. Decided once at resolve time from the
//            icon's pixels: a monochrome glyph (e.g. a "-symbolic" icon)
//            would vanish on the dark dock without a contrasting chip,
//            while full-color icons look best bare.
struct DockIcon
{
    QIcon icon;
    QColor backing;
};

// One icon cell in the dock. Custom-painted (no QSS, which is why it
// draws exactly what you see): a rounded hover highlight, the app icon
// (slightly enlarged while hovered, macOS-style), a small dot under the
// focused window's icon, or a letter chip when no icon could be found.
class DockButton : public QPushButton
{
public:
    explicit DockButton(QWidget *parent = nullptr);

    // `name` is the app_id (or the title before the app_id arrives) and
    // is only used to pick the placeholder letter.
    void setDockIcon(const DockIcon &icon, const QString &name);
    void setActive(bool active);

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    DockIcon icon_;
    QString letter_ = QStringLiteral("?");
    bool active_ = false;
};

// One open window's dock entry.
struct AppWindow
{
    zwlr_foreign_toplevel_handle_v1 *handle = nullptr;
    std::string title;
    std::string appId;
    DockIcon icon;
    // The compositor sends "state" then "done"; the dot must only change
    // on done, so the last state event is queued here first.
    bool pendingActive = false;
    DockButton *button = nullptr;
};

// The floating bottom pill: one icon per open window, via
// wlr-foreign-toplevel-management. Clicking an icon asks the compositor
// to focus that window. Anchored bottom-only, so the compositor centers
// it and it is exactly as wide as its icons (see syncSurfaceSize()).
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

    // True once at least one window exists -- used by main.cpp to decide
    // whether to show the dock at startup (an empty pill floating at the
    // bottom of the screen would look like a bug).
    bool hasApps() const { return !apps_.empty(); }

    // Called once per already-open or newly-opened window (from the
    // manager-level "toplevel" event -- see kManagerListener in Dock.cpp).
    void addApp(zwlr_foreign_toplevel_handle_v1 *handle);
    void removeApp(zwlr_foreign_toplevel_handle_v1 *handle);
    void setTitle(zwlr_foreign_toplevel_handle_v1 *handle, const char *title);
    void setAppId(zwlr_foreign_toplevel_handle_v1 *handle, const char *appId);
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

    // Resizes the widget (and the layer surface) to the layout's size
    // hint, so the pill is always exactly as wide as its icons, and
    // re-checks visibility. Call after any change to apps_.
    void syncSurfaceSize();

    // state/done are a pair: queueState() stores the last state event,
    // applyDone() flips the dots once the compositor says it's done.
    void queueState(zwlr_foreign_toplevel_handle_v1 *handle, const wl_array *state);
    void applyDone(zwlr_foreign_toplevel_handle_v1 *handle);

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
