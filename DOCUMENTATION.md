# MervShell — Full Usage & Extension Documentation

This is the complete "how to use this code" manual: how the shell is built,
**how to draw on the desktop**, **how to add drag & drop**, and **where to put
what** when you change something.

> Requirements: a wlroots-based Wayland compositor (labwc, sway, hyprland, …),
> Qt6 Widgets, LayerShellQt, and the generated `wlr-ftm.h/.c` protocol files.
> GNOME and stock KDE do **not** advertise `wlr-foreign-toplevel-management`,
> so the Dock will refuse to start there.

---

## 1. Build & run

```bash
cd mervshell-qt
make            # builds everything except wlr-ftm.o (yours, already present)
./mervshell     # or: make run

# debug output (icon resolution, dock sizing, window events):
MERVSHELL_DEBUG=1 ./mervshell
```

Two compiler rules you must never break:

| File | Compiler | Why |
|---|---|---|
| `wlr-ftm.c` | **`gcc -c wlr-ftm.c`** | generated protocol code is C; `g++` would make `wl_interface` symbols internal → "undefined reference" |
| every `.cpp` | `g++` | normal C++ |

The `Makefile` already does this. **If you add a new `.cpp` file you must add
it to `CXX_SOURCES`** (line 31) or it simply won't be compiled/linked.

---

## 2. The mental model: 3 surfaces, 2 Wayland connections

MervShell is not one window. It is **three separate layer-shell surfaces**
stacked by the compositor, plus a private raw-Wayland connection:

```
 stacking order (bottom → top)          widget          file
 ─────────────────────────────────────────────────────────────
 LayerBackground  wallpaper/fill        Background      bg.h/.cpp
 LayerBottom      (nothing yet)         ← put desktop gadgets HERE
  ── normal xdg-shell app windows live here ──
 LayerTop         top bar 32px          Taskbar         Taskbar.h/.cpp
 LayerTop         floating dock pill     Dock            Dock.h/.cpp
 LayerOverlay     (nothing yet)         ← put popups/tooltips HERE
```

Two connections:

1. **LayerShellQt's internal connection** — places the three bars (anchors,
   layer, exclusive zone). Created by `makeLayerSurface()` in `LayerShellUtil.h`.
2. **`WaylandGlobals`' own `wl_display`** (`WaylandGlobals.cpp`) — only for
   `wl_seat` + `wlr-foreign-toplevel-management`, i.e. "which windows exist"
   and "focus that window". Its fd is wired into Qt's event loop by
   `pumpEventsWithQt()`, so compositor events arrive automatically.

`main.cpp` is pure glue: connect globals → build widgets → call
`makeLayerSurface()` once per widget → `app.exec()`.

---

## 3. Where to put what (cheat sheet)

| I want to change… | Put it here |
|---|---|
| Any color | `Theme.h` (`BackgroundColor`, `DockBackground`, `ButtonHover`, `TextColor`, …) |
| Any size/radius/spacing/alpha | `Theme.h` (`IconSize`, `DockRadius`, `DockBackgroundAlpha`, `DockExclusiveZone`, …) |
| Wallpaper image | `main.cpp` → `background.setWallpaper("…")` (already set to your absolute path) |
| Solid background color | `Theme::BackgroundColor` — note it is currently `"#0000"` = **fully transparent**, so with no wallpaper you get black. Use `"#10141c"` for a solid fill. |
| Which screen edge a bar sits on, its layer, reserved space | the `makeLayerSurface(...)` call in `main.cpp` (anchors / layer / exclusiveZone / size / margins) |
| Top bar content (clock, battery, new widgets) | `Taskbar.cpp` constructor, next to `batteryLabel_`/`clockLabel_` |
| Top bar height | `main.cpp` (both the `32` exclusive zone and `QSize(0, 32)`) |
| Dock look (pill, icon, hover, active bar) | `DockButton::paintEvent()` + `Dock::paintEvent()` in `Dock.cpp`, sizes from `Theme.h` |
| Dock button click behavior | `QObject::connect(button, …)` lambda in `Dock::rebuild()` (`Dock.cpp`) |
| Dock icon lookup (how app icons are found) | `resolveDockIcon()` + helpers in the anonymous namespace at the top of `Dock.cpp` |
| Dock → window focus action | `Dock::activate()` in `Dock.cpp` |
| Wayland globals (`wl_seat`, toplevel manager) | `WaylandGlobals.cpp` |
| Protocol bindings (`wlr-ftm.h/.c`) | regenerate from `wlr-foreign-toplevel-management-unstable-v1.xml` |
| **Drawing on the desktop** | `Background::paintEvent()` (`bg.cpp`) or a new widget on `LayerBottom` — see §4 |
| **Drag & drop** | doesn't exist yet — see §5 |
| Adding a **new bar/surface** | new `Foo.h/.cpp` + `makeLayerSurface()` block in `main.cpp` + entry in `Makefile` `CXX_SOURCES` |
| Autostart / running it as your shell | your compositor config (e.g. labwc `autostart`): `mervshell &` |

---

## 4. Drawing on the desktop

### 4.1 How painting works here

Every surface is an ordinary `QWidget` with a `paintEvent()` override. Three rules:

1. **Order = stacking.** Whatever is on `LayerBackground` is *under* everything,
   `LayerBottom` is above the wallpaper but **under app windows**, `LayerTop` is
   above app windows, `LayerOverlay` is above everything.
2. **Translucency.** Bars call
   `setAttribute(Qt::WA_TranslucentBackground)` and only paint their rounded
   shape → the corners are see-through. `Background` deliberately does *not*
   set it: it must be fully opaque or windows would show through the desktop.
3. **Antialiasing.** Always `p.setRenderHint(QPainter::Antialiasing)` before
   drawing rounded rects (both existing bars do).

### 4.2 The simplest drawing change: wallpaper / fill

```cpp
// main.cpp — before background.show()
background.setWallpaper("/home/mervlot/Wallpaper/wall.jpg"); // cover-scaling
// ...or no call at all -> solid Theme::BackgroundColor fill
```

### 4.3 Draw your own graphics on the desktop

Anything painted in `Background::paintEvent()` (`bg.cpp`) is literally drawn on
the desktop, under all windows. Example — add a big clock to the desktop:

```cpp
// bg.h  →  add under `protected:`
    void paintEvent(QPaintEvent *) override;   // already exists

// bg.cpp  →  in paintEvent(), after the wallpaper blit:
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(255, 255, 255, 90));
    QFont f = font(); f.setPixelSize(96); p.setFont(f);
    p.drawText(rect().adjusted(40, 40, -40, -40),
               Qt::AlignTop | Qt::AlignLeft,
               QDateTime::currentDateTime().toString("HH:mm"));
```

To repaint on a timer (clock) or when something changes:

```cpp
auto *t = new QTimer(this);
connect(t, &QTimer::timeout, this, qOverload<>(&Background::update));
t->start(1000);
```

`QWidget::update()` schedules a repaint — never call `repaint()` directly.

### 4.4 Real desktop widgets (icons, gadgets, panels that windows can cover)

If the thing must be **interactive** (clickable desktop icons), don't stuff it
into `Background` — make a fourth surface on **`LayerBottom`** so it sits above
the wallpaper but below app windows, exactly like a real desktop:

1. Create `Desktop.h` / `Desktop.cpp` (copy the structure of `bg.h`/`bg.cpp`):
   ```cpp
   class Desktop : public QWidget {
   public:
       Desktop();
   protected:
       void paintEvent(QPaintEvent *) override;
       void mousePressEvent(QMouseEvent *) override;   // icon clicks
   };
   ```
2. Register it in the **`Makefile`**: add `Desktop.cpp` to `CXX_SOURCES`.
3. Build + place it in **`main.cpp`**:
   ```cpp
   #include "Desktop.h"
   ...
   Desktop desktop;

   makeLayerSurface(
       &desktop,
       LayerShellQt::Window::LayerBottom,                       // under windows
       Anchors(LayerShellQt::Window::AnchorTop) | LayerShellQt::Window::AnchorBottom |
       LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight,
       -1,               // don't reserve space
       QSize(0, 0));     // full screen
   desktop.show();
   ```
4. If it must be **clickable**, remember rule 4.6 below (input region).

### 4.5 Anchors / layers / exclusive zone — what each argument does

```cpp
makeLayerSurface(widget, layer, anchors, exclusiveZone, desiredSize, margins);
```

| Argument | Meaning | Typical values |
|---|---|---|
| `layer` | stacking plane | `LayerBackground` wallpaper · `LayerBottom` desktop · `LayerTop` bars · `LayerOverlay` popups |
| `anchors` | which screen edges it sticks to | all 4 = full screen · `Top\|Left\|Right` = top bar · **`Bottom` only = horizontally centered** (that's the dock trick) · none = floating at `margins` |
| `exclusiveZone` | px of screen reserved so windows don't cover it | `-1` = none (background) · `32` top bar · `Theme::DockExclusiveZone` (64) dock · `0` = drawn over windows |
| `desiredSize` | size request; `0` on an axis = "stretch to fill" | `QSize(0,0)` full screen · `QSize(0,32)` top bar · dock passes `dock.sizeHint()` and re-syncs via `Dock::syncSurfaceSize()` |
| `margins` | gap from the anchored edge(s) | dock: `QMargins(0,0,0,Theme::DockMarginBottom)` |

⚠️ The `Anchors(...)` wrapper on the **first** flag is mandatory — raw
`AnchorTop | AnchorBottom` degenerates to `int` and QFlags refuses the
conversion ("invalid conversion from 'int' to 'Anchor'").

### 4.6 Input region gotcha (why your drawing may not receive clicks)

Qt derives the Wayland surface's input region from the widget's **mask**:

- No mask → the **whole surface** eats pointer input.
- That's fine for the background (nothing is below it), but a full-screen
  surface on `LayerTop`/`LayerOverlay` would **block every click** reaching
  windows underneath.
- To make parts click-through, punch holes: `widget->setMask(QRegion(...))`,
  or restrict input to your gadget's rect.
- `WA_TransparentForMouseEvents` only bypasses *Qt's* event dispatch — it does
  **not** remove the region from the Wayland input region. Use the mask for
  true click-through.

All three bars currently run with `KeyboardInteractivityNone` (set in
`LayerShellUtil.h`). Change to `KeyboardInteractivityOnDemand` for any surface
that needs a text field or shortcuts.

---

## 5. Drag & drop

### 5.1 Current state

**There is no drag & drop in the codebase today.** The Dock's buttons only
handle click → `activate()` (`Dock.cpp:638`). Everything below is where and how
to add it. Qt's DND works fine between layer-shell surfaces, and also to/from
other apps (file managers, terminals) — you just need real MIME types.

Two roles:
- **Drag source** → `DockButton` (drag an app out of the dock).
- **Drop target** → `Background` (drop a file on the desktop) and/or `Dock`
  (drop a file onto an app icon to open it).

### 5.2 Drag source: pull an app out of the dock

**`Dock.h`** — inside `class DockButton`:

```cpp
protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
private:
    QString appId_;        // remember what we're dragging
    QPoint  dragStartPos_;
    bool    dragged_ = false;
```

**`Dock.cpp`**:

```cpp
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QApplication>

// in setDockIcon(), store the name:
void DockButton::setDockIcon(const DockIcon &icon, const QString &name)
{
    appId_ = name;               // <-- add this line
    ...existing code...
}

void DockButton::mousePressEvent(QMouseEvent *e)
{
    dragged_ = false;
    if (e->button() == Qt::LeftButton) dragStartPos_ = e->pos();
    QPushButton::mousePressEvent(e);
}

void DockButton::mouseMoveEvent(QMouseEvent *e)
{
    if (!(e->buttons() & Qt::LeftButton) || appId_.isEmpty()) {
        QPushButton::mouseMoveEvent(e);
        return;
    }
    // Below this threshold it's still a click, not a drag.
    if ((e->pos() - dragStartPos_).manhattanLength()
            < QApplication::startDragDistance())
        return;

    auto *mime = new QMimeData;              // QDrag takes ownership
    mime->setData("application/x-mervshell-app", appId_.toUtf8());
    mime->setText(appId_);                   // plain-text fallback

    auto *drag = new QDrag(this);            // QDrag takes ownership of mime
    drag->setMimeData(mime);
    drag->setPixmap(grab().scaled(64, 64, Qt::KeepAspectRatio,
                                  Qt::SmoothTransformation));

    dragged_ = true;
    drag->exec(Qt::CopyAction);              // blocks until the drag ends
}

void DockButton::mouseReleaseEvent(QMouseEvent *e)
{
    if (dragged_) { e->accept(); return; }   // a drag just ended -> NOT a click
    QPushButton::mouseReleaseEvent(e);       // normal click -> clicked() -> activate()
}
```

Why `dragged_`: `QPushButton` emits `clicked()` on release, which would also
focus the window. The flag suppresses that when the gesture was a drag.

### 5.3 Drop target: drop a file on the desktop

**`bg.h`** — under `protected:`:

```cpp
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
```

**`bg.cpp`**:

```cpp
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QProcess>

Background::Background()
{
    setAcceptDrops(true);            // <-- required
}

static bool acceptable(const QMimeData *m)
{
    return m->hasUrls()                                   // files from a file manager
        || m->hasFormat("application/x-mervshell-app");   // our own dock drags
}

void Background::dragEnterEvent(QDragEnterEvent *e)
{ if (acceptable(e->mimeData())) e->acceptProposedAction(); }

void Background::dragMoveEvent(QDragMoveEvent *e)
{ if (acceptable(e->mimeData())) e->acceptProposedAction(); }

void Background::dragLeaveEvent(QDragLeaveEvent *) { update(); /* clear highlight */ }

void Background::dropEvent(QDropEvent *e)
{
    const QMimeData *m = e->mimeData();
    if (m->hasUrls() && !m->urls().isEmpty()) {
        QProcess::startDetached("xdg-open", { m->urls().first().toLocalFile() });
    } else if (m->hasFormat("application/x-mervshell-app")) {
        QProcess::startDetached(
            QString::fromUtf8(m->data("application/x-mervshell-app")), {});
    }
    e->acceptProposedAction();
    update();
}
```

Standard MIME types other apps understand:

| MIME | use |
|---|---|
| `text/uri-list` (`QMimeData::setUrls`) | files — works with file managers, terminals |
| `text/plain` (`setText`) | arbitrary strings |
| `application/x-mervshell-app` | private, only MervShell understands it |

To let a *file manager* accept a drag from us, build the mime with
`mime->setUrls({QUrl::fromLocalFile(path)})` instead of a private format.

### 5.4 Drop onto a specific dock icon

Child widgets get first refusal; anything a child **ignores** bubbles up to the
parent — so you can handle everything in `Dock`:

```cpp
// Dock::Dock()  ->  add:   setAcceptDrops(true);

void Dock::dragEnterEvent(QDragEnterEvent *e)
{ if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }

void Dock::dragMoveEvent(QDragMoveEvent *e)
{ if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }

void Dock::dropEvent(QDropEvent *e)
{
    // which icon is under the cursor?
    QWidget *w = childAt(e->position().toPoint());
    while (w && w != this && !qobject_cast<DockButton *>(w)) w = w->parentWidget();
    auto *button = qobject_cast<DockButton *>(w);
    if (!button) { e->ignore(); return; }

    // TODO: map button -> AppWindow and do your thing (launch file with it,
    // focus the window, ...). The handle lives in apps_[i]->button.
    e->acceptProposedAction();
}
```

### 5.5 DND gotchas

- **Threshold matters.** Never start a drag before
  `QApplication::startDragDistance()` (8 px default) or every click becomes a drag.
- **Wayland drags only start while the pointer is inside your surface.**
- `drag->exec()` **blocks** the calling code until the drag finishes.
- `setAcceptDrops(true)` is required on the **receiving** widget — forgetting
  it is the #1 reason drops silently do nothing.
- `dragLeaveEvent` is where you clear a "drop here" highlight.
- Keyboard interactivity (`None` today) is unrelated to DND — no change needed.
- Remember §4.6: the receiving surface only gets events where its input
  region is.

---

## 6. How the Dock works (event flow)

```
WaylandGlobals::connect()
   └─ roundtrip → binds wl_seat + zwlr_foreign_toplevel_manager_v1
main.cpp
   └─ zwlr_foreign_toplevel_manager_v1_add_listener(..., &Dock::kManagerListener, &dock)
        └─ handleNewToplevel() ─► Dock::addApp(handle)
                                   ├─ zwlr_foreign_toplevel_handle_v1_add_listener(...)  ← THE FIX
                                   └─ rebuild()  → one DockButton per window
        compositor events ─► kHandleListener:
           title  → setTitle()   (tooltip)
           app_id → setAppId()   → resolveDockIcon() (cached) → button icon
           state  → queueState() (pendingActive)
           done   → applyDone()  → button->setActive() → active capsule
           closed → removeApp()  → rebuild()
click → Dock::activate() → zwlr_foreign_toplevel_handle_v1_activate() + wl_display_flush()
any change → syncSurfaceSize() → adjustSize() + layer->setDesiredSize(sizeHint())
```

Startup visibility: `main.cpp` only shows the dock if `dock.hasApps()`;
`syncSurfaceSize()` hides it again when the last window closes.

---

## 7. Recipes

**Add a widget to the top bar** — `Taskbar.cpp` constructor:
```cpp
auto *workspaceLabel = new QLabel(this);
workspaceLabel->setStyleSheet(QString("color: %1;").arg(Theme::TextColor));
layout->addWidget(workspaceLabel);      // before layout->addStretch() = left side
```
(declare the member in `Taskbar.h`).

**Add a launcher button (not tied to a window)** — in `Dock::Dock()`:
```cpp
auto *launcher = new QPushButton(this);
launcher->setIcon(QIcon::fromTheme("utilities-terminal"));
QObject::connect(launcher, &QPushButton::clicked, []{
    QProcess::startDetached("foot", {});
});
layout_->addWidget(launcher);
```
Note: `Dock::rebuild()` **wipes the whole layout** — a static launcher added in
the constructor will be destroyed the first time a window opens/closes. If you
add one, either rebuild it inside `rebuild()` before the app loop, or give it
its own sibling layout that `rebuild()` doesn't touch.

**Add a fourth bar** — copy any `makeLayerSurface(...)` block in `main.cpp`
(§4.4) and write the widget like `Taskbar`.

**Change the dock's reserved strip / gap** — `Theme::DockExclusiveZone`,
`Theme::DockMarginBottom`.

**Make a surface translucent** — `setAttribute(Qt::WA_TranslucentBackground)`
in its constructor + paint only your rounded shape.

---

## 8. Debugging

```bash
MERVSHELL_DEBUG=1 ./mervshell
```

prints, per window: icon-resolution tiers (`resolve 'foot': themed1=1 …`),
`Dock: new window`, `app_id event: …`, and `sync: apps=… hint=… size=…` plus a
per-button layout dump (handy for the "dock shrank to a 12×12 blob" class of bug).

---

## 9. Pitfall checklist (all real, all previously hit)

1. **Invisible button text** = `zwlr_foreign_toplevel_handle_v1_add_listener()`
   missing in `Dock::addApp()`. Without it no `title` event ever arrives.
2. **`wlr-ftm.c` must be built with `gcc`**, never `g++`.
3. **`.parent` listener field** requires a newer `wlr-ftm.h`. If your generated
   header predates it, delete the `.parent = Dock::handleParent,` line and its
   declaration/definition.
4. **Invisible text via system themes** (Breeze/Kvantum ignore QSS button
   colors) → `main.cpp` forces the Fusion style. Don't remove that line.
5. **Buttons created after the parent is visible stay hidden** → always call
   `button->show()` (see `Dock::rebuild()`).
6. **Empty pill** = show/hide handled by `hasApps()` + `syncSurfaceSize()`.
7. **`AnchorTop | AnchorBottom` must be wrapped in `Anchors(...)`** (§4.5).
8. **Compositor must support `wlr-foreign-toplevel-management`** or
   `WaylandGlobals::connect()` returns false and `main()` exits with code 1.
9. **`Theme::BackgroundColor = "#0000"`** is fully transparent (4-digit #RGBA),
   not black-with-alpha — solid fill needs a real hex like `"#10141c"`.
10. **Full-screen `LayerTop` surfaces swallow clicks** — use `setMask()` (§4.6).
