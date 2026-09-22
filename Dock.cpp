#include "Dock.h"
#include "Theme.h"

#include <QPainter>
#include <QString>
#include <cstdio>

Dock::Dock()
{
    setAttribute(Qt::WA_TranslucentBackground);

    resize(1, Theme::ButtonHeight + Theme::BarMargin * 2);

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(Theme::BarMargin, Theme::BarMargin,
                                 Theme::BarMargin, Theme::BarMargin);
    layout_->setSpacing(Theme::BarSpacing);
}

void Dock::setWayland(wl_display *display, wl_seat *seat)
{
    display_ = display;
    seat_ = seat;
}

void Dock::addApp(zwlr_foreign_toplevel_handle_v1 *handle)
{
    auto *app = new AppWindow;
    app->handle = handle;
    apps_.push_back(app);

    // *** This call was missing in the original code. ***
    // Without it, the compositor has no reason to ever send us this
    // window's title -- handleTitle() below would simply never run, and
    // every button would stay stuck on its initial empty text. Passing
    // `this` as the data pointer is what lets the static handleTitle()
    // callback reach back into this Dock instance.
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &kHandleListener, this);

    std::printf("Dock: new window\n");

    rebuild();
}

void Dock::removeApp(zwlr_foreign_toplevel_handle_v1 *handle)
{
    for (auto it = apps_.begin(); it != apps_.end(); ++it) {
        if ((*it)->handle == handle) {
            delete *it;
            apps_.erase(it);
            rebuild();
            return;
        }
    }
}

void Dock::setTitle(zwlr_foreign_toplevel_handle_v1 *handle, const char *title)
{
    for (auto *app : apps_) {
        if (app->handle == handle) {
            app->title = title ? title : "";
            if (app->button) {
                app->button->setText(QString::fromUtf8(app->title.c_str()));
            }
            return;
        }
    }
}

void Dock::activate(zwlr_foreign_toplevel_handle_v1 *handle)
{
    if (!seat_) return;
    zwlr_foreign_toplevel_handle_v1_activate(handle, seat_);
    wl_display_flush(display_);
}

void Dock::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(Theme::DockBackground));
}

void Dock::rebuild()
{
    while (QLayoutItem *item = layout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (auto *app : apps_) {
        auto *button = new QPushButton(this);
        button->setText(QString::fromUtf8(app->title.c_str()));
        button->setFixedHeight(Theme::ButtonHeight);

        // `background-color` (not the `background` shorthand) and an
        // explicit palette below are both belt-and-suspenders against a
        // separate, unrelated issue: some system Qt styles/themes
        // (Breeze, Kvantum, ...) partially ignore QSS text colors on
        // QPushButton. Forcing the Fusion style in main() is the real
        // fix for that class of bug -- these are just a fallback in case
        // Fusion isn't available for some reason.
        button->setStyleSheet(QString(
            "QPushButton {"
            "  background-color: %1;"
            "  color: %2;"
            "  border: none;"
            "  border-radius: %3px;"
            "  padding: 4px 12px;"
            "}"
            "QPushButton:hover {"
            "  background-color: %4;"
            "}"
        ).arg(Theme::ButtonBackground, Theme::TextColor)
         .arg(Theme::ButtonRadius)
         .arg(Theme::ButtonHover));

        QPalette pal = button->palette();
        pal.setColor(QPalette::ButtonText, QColor(Theme::TextColor));
        button->setPalette(pal);

        auto handle = app->handle;
        QObject::connect(button, &QPushButton::clicked, [this, handle]() {
            activate(handle);
        });

        app->button = button;
        layout_->addWidget(button);
    }
}

// --- Manager-level listener: one window appeared -----------------------

const zwlr_foreign_toplevel_manager_v1_listener Dock::kManagerListener = {
    .toplevel = Dock::handleNewToplevel,
    .finished = [](void *, zwlr_foreign_toplevel_manager_v1 *) {},
};

void Dock::handleNewToplevel(void *data, zwlr_foreign_toplevel_manager_v1 *,
                              zwlr_foreign_toplevel_handle_v1 *handle)
{
    static_cast<Dock *>(data)->addApp(handle);
}

// --- Per-window listener: title changes, closing, ... ------------------

const zwlr_foreign_toplevel_handle_v1_listener Dock::kHandleListener = {
    .title = Dock::handleTitle,
    .app_id = Dock::handleAppId,
    .output_enter = Dock::handleOutputEnter,
    .output_leave = Dock::handleOutputLeave,
    .state = Dock::handleState,
    .done = Dock::handleDone,
    .closed = Dock::handleClosed,
    .parent = Dock::handleParent,
};

void Dock::handleTitle(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *title)
{
    static_cast<Dock *>(data)->setTitle(handle, title);
}

void Dock::handleClosed(void *data, zwlr_foreign_toplevel_handle_v1 *handle)
{
    static_cast<Dock *>(data)->removeApp(handle);
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}

void Dock::handleAppId(void *, zwlr_foreign_toplevel_handle_v1 *, const char *) {}
void Dock::handleOutputEnter(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void Dock::handleOutputLeave(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void Dock::handleState(void *, zwlr_foreign_toplevel_handle_v1 *, wl_array *) {}
void Dock::handleDone(void *, zwlr_foreign_toplevel_handle_v1 *) {}
void Dock::handleParent(void *, zwlr_foreign_toplevel_handle_v1 *, zwlr_foreign_toplevel_handle_v1 *) {}
