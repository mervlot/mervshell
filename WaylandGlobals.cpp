#include "WaylandGlobals.h"

#include <QSocketNotifier>
#include <QCoreApplication>
#include <cstdio>
#include <cstring>

bool WaylandGlobals::connect()
{
    display_ = wl_display_connect(nullptr);
    if (!display_) {
        std::printf("WaylandGlobals: could not connect to Wayland\n");
        return false;
    }

    registry_ = wl_display_get_registry(display_);

    wl_registry_listener listener{};
    listener.global = handleGlobal;
    listener.global_remove = handleGlobalRemove;
    wl_registry_add_listener(registry_, &listener, this);

    // First roundtrip: receive every currently-existing global, including
    // wl_seat and zwlr_foreign_toplevel_manager_v1.
    wl_display_roundtrip(display_);

    if (!toplevelManager_) {
        std::printf("WaylandGlobals: compositor does not support "
                     "wlr-foreign-toplevel-management\n");
        return false;
    }

    // A second roundtrip picks up the toplevel events for windows that
    // already existed before we connected (each existing window fires a
    // "toplevel" event once we're listening).
  

    return true;
}

void WaylandGlobals::pumpEventsWithQt()
{
    int fd = wl_display_get_fd(display_);

    auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, qApp);
    QObject::connect(notifier, &QSocketNotifier::activated, [this]() {
        wl_display_dispatch(display_);
    });
}

void WaylandGlobals::handleGlobal(void *data, wl_registry *registry, uint32_t name,
                                   const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandGlobals *>(data);

    if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        self->seat_ = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface,
                              version > 1 ? 1 : version));
    } else if (std::strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        self->toplevelManager_ = static_cast<zwlr_foreign_toplevel_manager_v1 *>(
            wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface,
                              version > 3 ? 3 : version));
    }
}

void WaylandGlobals::handleGlobalRemove(void *, wl_registry *, uint32_t)
{
    // Not handled: MervShell doesn't currently react to the seat or the
    // toplevel manager disappearing mid-session.
}
