# MervShell (Qt / LayerShellQt version)

> **Full manual:** see [DOCUMENTATION.md](DOCUMENTATION.md) for how to draw on
> the desktop, add drag & drop, and the complete "where to put what" map.

Your working dock, split into files, with the actual bug fixed, plus a
top bar (clock + battery) and a background layer.

## The bug: invisible button text

Not a styling problem. In the original single-file version,
`zwlr_foreign_toplevel_handle_v1_listener` (the `handleListener` struct
with `onTitle`, `onClosed`, etc.) was defined but **never attached to a
window handle** -- there was no call to
`zwlr_foreign_toplevel_handle_v1_add_listener()` anywhere. Without that
call, the compositor has no reason to ever send a title event, so
`onTitle` never runs, and every `AppWindow::title` stays `""` forever --
which is exactly "the button exists but the text is invisible."

Fixed in `Dock::addApp()` (`Dock.cpp`):

```cpp
void Dock::addApp(zwlr_foreign_toplevel_handle_v1 *handle)
{
    auto *app = new AppWindow;
    app->handle = handle;
    apps_.push_back(app);

    // *** This call was missing. ***
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &kHandleListener, this);

    rebuild();
}
```

I also forced the Fusion Qt style in `main.cpp`. That's unrelated to the
real bug above, but it's cheap insurance against a *different*,
easy-to-confuse-with-this-one problem: some system themes (Breeze,
Kvantum) partially ignore stylesheet text colors on `QPushButton`. Fusion
always respects the stylesheet exactly as written.

## File layout

```
main.cpp             thin glue: connects everything, ~90 lines
WaylandGlobals.h/.cpp the raw wl_seat + foreign-toplevel-manager connection
LayerShellUtil.h      one function that turns a QWidget into a layer-shell surface
Theme.h                all colors/sizes in one place
bg.h/bg.cpp            background layer (solid fill, optional wallpaper)
Taskbar.h/.cpp         top bar: clock + battery
Dock.h/.cpp            bottom bar: one button per open window (your code, fixed)
Makefile
```

`main.cpp` no longer knows what a `wl_registry_listener` or a
`LayerShellQt::Window::Anchor` bitmask looks like -- it just calls
`wayland.connect()`, builds the three widgets, and calls
`makeLayerSurface()` three times. If you want a fourth bar later, copy
one of those three `makeLayerSurface()` blocks in `main.cpp` and write
its widget the same way `Taskbar` or `bg` is written.

## Your existing protocol files

I didn't ship `wlr-ftm.h`/`wlr-ftm.c`/`wlr-ftm.o` -- keep using the ones
you already have (generated from your
`wlr-foreign-toplevel-management-unstable-v1.xml`). Two things to check:

1. **Build `wlr-ftm.c` with `gcc`, not `g++`.** Generated protocol code
   declares things like `const struct wl_interface X = {...};` at file
   scope. In C that's externally linked; compiled as C++ (which is what
   `g++ file.c` actually does -- it ignores the `.c` extension and
   compiles as C++ regardless) it becomes internally linked, like
   `static`, and every other file that references `X` fails to link
   against it with "undefined reference". If your existing `wlr-ftm.o`
   already works with your current single-file build, it's already fine
   -- just make sure whatever produced it used `gcc -c wlr-ftm.c`, not
   `g++ -c wlr-ftm.c`. The included `Makefile` does this for you
   automatically.

2. **Check for the `parent` event.** `Dock.cpp`'s listener includes a
   `.parent = Dock::handleParent` entry, from a newer revision of the
   protocol than your pasted code was written against (it adds a
   "this window is a dialog of that window" event). If your `wlr-ftm.h`
   predates that and you get a compile error on that line, just delete
   the `.parent = Dock::handleParent,` line in `Dock.cpp` and the
   `handleParent` declaration/definition around it -- everything else is
   unaffected.

## Build

```bash
cp /path/to/your/wlr-ftm.h /path/to/your/wlr-ftm.c mervshell-qt/
cd mervshell-qt
make
./mervshell
```

This runs the same compiler invocation you had, just split per file --
see the comment at the top of `Makefile` if you want to see the
equivalent single `g++` command.

## What I could not verify here

I don't have a way to run a real Wayland compositor with GPU access,
Qt6, *and* the actual `LayerShellQt` library together in this sandbox
(Qt6's LayerShellQt isn't in the package repos I have access to, and I
can't reach `invent.kde.org` to build it from source). So:

- **Verified for real:** every file here compiles and links cleanly
  against real Qt6Widgets and real generated protocol code, using your
  exact build approach (`gcc` for the protocol file, `g++` for the rest).
  I built this with `g++ -Wall -Wextra` and it's warning-free.
  I stubbed out only `LayerShellQt::Window` itself (matching its real,
  stable public API -- the same calls your original code already used:
  `setLayer`, `setAnchors`, `setExclusiveZone`,
  `setKeyboardInteractivity`, `setDesiredSize`) purely so the compiler
  could check everything else.
- **Not verified here:** the actual on-screen result under labwc with the
  real LayerShellQt. Since this is your existing, previously-working
  approach (just fixed + reorganized) rather than a new rendering stack,
  the risk is much lower than if this were new from scratch -- but please
  still run it for real on your machine before relying on it.

## How to customize things

- **Colors:** all in `Theme.h`. Change `Theme::BarBackground`,
  `ButtonHover`, etc. and every bar picks it up.
- **Bar sizes/margins:** `Theme::ButtonHeight`, `BarMargin`, `BarSpacing`
  in `Theme.h`; the exclusive zone / desired size for each bar is set
  where `makeLayerSurface()` is called in `main.cpp`.
- **Wallpaper:** call `background.setWallpaper("assets/wallpaper.png")`
  in `main.cpp` before `background.show()`.
- **Taskbar content:** add more `QLabel`/`QPushButton`s in `Taskbar`'s
  constructor (`Taskbar.cpp`), next to `clockLabel_`/`batteryLabel_`, the
  same way they're added to `layout`.
- **Dock buttons:** these come from whatever windows are actually open
  (via wlr-foreign-toplevel-management) -- there's nothing to hand-edit
  there. To add a *launcher* button (not tied to an open window), add a
  plain `QPushButton` to `layout_` in `Dock`'s constructor the same way
  `Taskbar` adds its labels, and connect its `clicked` signal to
  `QProcess::startDetached("foot")` (or whatever command) instead of
  `activate()`.
- **Click behavior:** `QObject::connect(button, &QPushButton::clicked, ...)`
  in `Dock::rebuild()` -- change what the lambda does.
