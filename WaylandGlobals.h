#pragma once

#include <wayland-client.h>
#include "wlr-ftm.h"

// LayerShellQt opens its own internal Wayland connection to place bars on
// screen (anchors, exclusive zone, layer). It does NOT know about
// wlr-foreign-toplevel-management, which is how the Dock finds out which
// windows exist and asks the compositor to focus one when clicked. So
// MervShell needs a second, plain wl_display connection of its own, just
// for that -- this class owns it.
//
// Everything here is raw C Wayland (connect, registry, roundtrip) -- the
// same three calls that used to sit inline in main(). Pulling them out
// means main.cpp no longer needs to know what a wl_registry_listener even
// looks like.
class WaylandGlobals
{
public:
    // Connects, binds wl_seat + the foreign-toplevel manager, and does the
    // first roundtrip to receive them. Returns false if the compositor
    // doesn't support wlr-foreign-toplevel-management.
    bool connect();

    // Wires the Wayland connection's file descriptor into Qt's event
    // loop, so `wl_display_dispatch()` runs automatically whenever the
    // compositor has something to tell us (a new window, a title change,
    // ...) instead of us having to poll for it.
    void pumpEventsWithQt();

    wl_display *display() const { return display_; }
    wl_seat *seat() const { return seat_; }
    zwlr_foreign_toplevel_manager_v1 *toplevelManager() const { return toplevelManager_; }

private:
    static void handleGlobal(void *data, wl_registry *registry, uint32_t name,
                              const char *interface, uint32_t version);
    static void handleGlobalRemove(void *data, wl_registry *registry, uint32_t name);

    wl_display *display_ = nullptr;
    wl_registry *registry_ = nullptr;
    wl_seat *seat_ = nullptr;
    zwlr_foreign_toplevel_manager_v1 *toplevelManager_ = nullptr;
};
