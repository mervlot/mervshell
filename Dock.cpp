#include "Dock.h"
#include "Theme.h"

#include <LayerShellQt/Window>

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPixmap>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QWindow>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

// =====================================================================
// Icon resolution
//
// Goal: given a window's Wayland app_id (e.g. "foot", "pcmanfm",
// "org.telegram.desktop"), show that app's official icon, the same one
// its .desktop file asks for. Tried in order:
//
//   1. the icon theme, using the app_id as the icon name
//   2. the app's .desktop file's Icon= key (theme, then file on disk)
//   3. an icon file named like the app_id, anywhere under the XDG icon
//      or pixmaps directories (this is what finds e.g. Telegram and
//      code-oss on a bare system where the theme lookup fails)
//   4. a monochrome "<name>-symbolic" variant (drawn on a chip so it
//      stays visible)
//   5. nothing -- DockButton draws a letter chip instead
//
// Every result (including "nothing") is cached per app_id.
// =====================================================================

namespace
{

// MervShell IS the desktop, so no KDE/GNOME/GTK platform theme has
// configured Qt's icon theme by the time we start: QIcon::themeName()
// comes back empty and every QIcon::fromTheme() resolves to nothing.
// Point Qt at the XDG icon directories and pick the theme the user
// actually chose (GTK's settings file names it; otherwise the first
// installed theme that doesn't mark itself Hidden -- Hidden=true is
// exactly how themes like Adwaita say "I'm fallback only").
void ensureIconTheme()
{
    static bool done = false;
    if (done) return;
    done = true;

    const QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);

    QStringList paths;
    for (const QString &dir : dataDirs)
        paths << dir + "/icons";
    paths << QIcon::themeSearchPaths(); // keep Qt's own (":/icons")
    QIcon::setThemeSearchPaths(paths);

    if (!QIcon::themeName().isEmpty())
        return;

    // 1) The theme GTK is configured to use.
    QString wanted;
    for (const QString &configDir : QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation)) {
        for (const char *gtk : {"gtk-3.0/settings.ini", "gtk-4.0/settings.ini"}) {
            QFile f(configDir + '/' + gtk);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QStringList lines = QTextStream(&f).readAll().split('\n');
            for (const QString &raw : lines) {
                const QString line = raw.trimmed();
                if (line.startsWith("gtk-icon-theme-name=")) {
                    wanted = line.mid(QStringLiteral("gtk-icon-theme-name=").size()).trimmed();
                    break;
                }
            }
            if (!wanted.isEmpty()) break;
        }
        if (!wanted.isEmpty()) break;
    }
    if (!wanted.isEmpty()) {
        for (const QString &dir : dataDirs) {
            if (QDir(dir + "/icons/" + wanted).exists()) {
                QIcon::setThemeName(wanted);
                return;
            }
        }
    }

    // 2) Otherwise the first installed theme that isn't Hidden.
    for (const QString &dir : dataDirs) {
        const QDir iconsDir(dir + "/icons");
        const QStringList themes = iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &theme : themes) {
            if (theme == "hicolor") continue; // base theme, checked last
            QFile index(iconsDir.filePath(theme + "/index.theme"));
            if (!index.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString text = QTextStream(&index).readAll();
            if (text.contains("\nHidden=true")) continue;
            QIcon::setThemeName(theme);
            return;
        }
    }

    QIcon::setThemeName("hicolor");
}

// A desktop-file index: app_id -> Icon= value.
//
// Two keys per file: the file's stem (a Wayland app_id usually IS the
// desktop file name, e.g. "org.kde.falkon") and StartupWMClass (XWayland
// apps report their WM_CLASS as the app_id, and .desktop files set
// StartupWMClass to match). First hit wins -- standardLocations() lists
// the user directory first, so ~/.local/share overrides /usr/share as
// XDG prescribes.
class DesktopIndex
{
public:
    static DesktopIndex &instance()
    {
        static DesktopIndex index;
        return index;
    }

    QString iconNameFor(const QString &appId) const
    {
        if (appId.isEmpty()) return {};
        build();

        QString stem = appId;
        if (stem.endsWith(".desktop", Qt::CaseInsensitive))
            stem.chop(8);

        if (byFile_.contains(stem)) return byFile_.value(stem);
        if (byFileLower_.contains(stem.toLower())) return byFileLower_.value(stem.toLower());
        return byWmClass_.value(stem);
    }

private:
    DesktopIndex() = default;

    void build() const
    {
        if (built_) return;
        built_ = true;

        const QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
        for (const QString &dir : dataDirs) {
            const QDir applications(dir + "/applications");
            const QStringList files = applications.entryList({QStringLiteral("*.desktop")}, QDir::Files);
            for (const QString &file : files) {
                QString icon, wmClass;
                readDesktopEntry(applications.filePath(file), icon, wmClass);

                const QString stem = file.left(file.size() - 8); // strip ".desktop"
                if (!icon.isEmpty() && !byFile_.contains(stem)) {
                    byFile_.insert(stem, icon);
                    byFileLower_.insert(stem.toLower(), icon);
                }
                if (!wmClass.isEmpty() && !icon.isEmpty() && !byWmClass_.contains(wmClass))
                    byWmClass_.insert(wmClass, icon);
            }
        }
    }

    // Reads Icon= and StartupWMClass= out of the [Desktop Entry] section
    // (deliberately not using KService/GIO: this is ~20 lines and has no
    // dependency beyond QtCore).
    static void readDesktopEntry(const QString &path, QString &icon, QString &wmClass)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        QTextStream in(&f);
        bool inEntry = false;
        while (!in.atEnd()) {
            const QString line = in.readLine();
            if (line.startsWith('[')) {
                if (inEntry) break; // reached the next section
                inEntry = (line == "[Desktop Entry]");
                continue;
            }
            if (!inEntry) continue;
            if (line.startsWith("Icon=")) icon = line.mid(5).trimmed();
            else if (line.startsWith("StartupWMClass=")) wmClass = line.mid(16).trimmed();
            if (!icon.isEmpty() && !wmClass.isEmpty()) break;
        }
    }

    mutable bool built_ = false;
    mutable QHash<QString, QString> byFile_;      // desktop stem -> icon name
    mutable QHash<QString, QString> byFileLower_; // lowercased stem -> icon name
    mutable QHash<QString, QString> byWmClass_;   // StartupWMClass -> icon name
};

// Finds an icon file by name anywhere in the XDG directories:
//   <dataDir>/pixmaps/<name>.<ext>                       (classic X11)
//   <dataDir>/icons/<theme>/<size>/<context>/<name>.<ext> (freedesktop)
// The user's data dir wins (standardLocations() order); within a dir the
// apps context and scalable/large sizes win, so the best-resolution icon
// is picked.
QString iconFileForName(const QString &name)
{
    static const QStringList kExts = {
        QStringLiteral("png"), QStringLiteral("svg"),
        QStringLiteral("xpm"), QStringLiteral("svgz"),
    };

    const QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &dataDir : dataDirs) {
        for (const QString &ext : kExts) {
            const QString path = dataDir + "/pixmaps/" + name + '.' + ext;
            if (QFile::exists(path)) return path;
        }

        QString best;
        int bestScore = -1;
        const QDir iconsDir(dataDir + "/icons");
        const QStringList themes = iconsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &theme : themes) {
            const QDir themeDir(iconsDir.filePath(theme));
            const QStringList sizes = themeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &size : sizes) {
                int score = 0;
                if (size == "scalable") score += 100000;
                else if (size == "symbolic") score += 90000;
                else {
                    const int x = size.indexOf('x');
                    if (x > 0) score += size.left(x).toInt(); // "32x32" -> 32
                }

                const QDir sizeDir(themeDir.filePath(size));
                const QStringList contexts = sizeDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QString &context : contexts) {
                    int contextScore = score;
                    if (context == "apps") contextScore += 1000000;
                    if (context != "apps" && context != "mimetypes" && context != "legacy")
                        continue;
                    for (const QString &ext : kExts) {
                        const QString path = sizeDir.filePath(context) + '/' + name + '.' + ext;
                        if (QFile::exists(path) && contextScore > bestScore) {
                            best = path;
                            bestScore = contextScore;
                        }
                    }
                }
            }
        }
        if (!best.isEmpty()) return best; // user dir beats system dirs
    }
    return {};
}

// Looks at the icon's actual pixels once and decides whether it needs a
// contrasting chip behind it on the dark dock: a near-black or
// near-white *monochrome* glyph would otherwise disappear. Full-color
// icons (anything with real saturation) are returned as "draw bare".
QColor backingColorFor(const QIcon &icon)
{
    const QPixmap pm = icon.pixmap(32, 32);
    if (pm.isNull()) return {};
    const QImage img = pm.toImage().convertToFormat(QImage::Format_ARGB32);
    if (img.isNull()) return {};

    qint64 lumSum = 0;
    qint64 satSum = 0;
    int count = 0;
    for (int y = 0; y < img.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = line[x];
            if (qAlpha(px) < 64) continue;
            const int r = qRed(px), g = qGreen(px), b = qBlue(px);
            lumSum += (r * 299 + g * 587 + b * 114) / 1000;
            satSum += std::max(r, std::max(g, b)) - std::min(r, std::min(g, b));
            ++count;
        }
    }
    if (count == 0) return {};

    const double luminance = double(lumSum) / count; // 0..255
    const double saturation = double(satSum) / count;
    if (saturation >= 30) return {};                 // full-color icon: bare
    if (luminance < 90) return QColor(245, 246, 248, 235); // dark glyph -> light chip
    if (luminance > 215) return QColor(24, 28, 36, 235);   // light glyph -> dark chip
    return {};
}

DockIcon resolveDockIcon(const QString &appId)
{
    ensureIconTheme();

    static QHash<QString, DockIcon> cache;
    const auto cached = cache.constFind(appId);
    if (cached != cache.constEnd()) return *cached;

    auto themed = [](const QString &name) -> QIcon {
        if (name.isEmpty() || !QIcon::hasThemeIcon(name)) return {};
        return QIcon::fromTheme(name);
    };
    auto onDisk = [](const QString &name) -> QIcon {
        if (name.isEmpty()) return {};
        if (QFile::exists(name)) return QIcon(name);
        const QString path = iconFileForName(name);
        return path.isEmpty() ? QIcon() : QIcon(path);
    };

    QString stem = appId;
    if (stem.endsWith(".desktop", Qt::CaseInsensitive))
        stem.chop(8);

    const bool dbg = std::getenv("MERVSHELL_DEBUG") != nullptr;

    QIcon icon = themed(stem);                              // 1) name == icon name
    const QString desktopIcon = DesktopIndex::instance().iconNameFor(stem);
    if (dbg) std::printf("resolve '%s': desktopIcon='%s' themed1=%d\n",
                          qPrintable(stem), qPrintable(desktopIcon), !icon.isNull());
    if (dbg) std::fflush(stdout);
    if (icon.isNull()) icon = themed(desktopIcon);          // 2) .desktop file's Icon=
    const int tier2 = !icon.isNull();
    if (icon.isNull()) icon = onDisk(stem);                 // 3) icon file named like app_id
    const int tier3 = !icon.isNull();
    if (icon.isNull()) icon = onDisk(desktopIcon);          // 4) icon file named like Icon=
    const int tier4 = !icon.isNull();
    if (icon.isNull()) icon = themed(stem + "-symbolic");   // 5) monochrome fallback
    if (icon.isNull()) icon = onDisk(stem + "-symbolic");
    const int tier5a = !icon.isNull();
    if (icon.isNull() && !desktopIcon.isEmpty())
        icon = onDisk(desktopIcon + "-symbolic");
    if (dbg) std::printf("  tiers: themed=%d diskName=%d diskIcon=%d symName=%d symIcon=%d\n",
                          tier2, tier3, tier4, tier5a, !icon.isNull());
    if (dbg) std::fflush(stdout);

    DockIcon result;
    if (!icon.isNull()) {
        result.icon = icon;
        result.backing = backingColorFor(icon);
    }
    cache.insert(appId, result);
    return result;
}

} // namespace

// =====================================================================
// DockButton
// =====================================================================

DockButton::DockButton(QWidget *parent)
    : QPushButton(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(Theme::ButtonWidth, Theme::ButtonHeight);
}

void DockButton::setDockIcon(const DockIcon &icon, const QString &name)
{
    icon_ = icon;

    // Placeholder letter: last segment of a reverse-DNS name
    // ("org.kde.falkon" -> F, not O), so it reads like the app.
    QString trimmed = name.trimmed();
    const int dot = trimmed.lastIndexOf('.');
    if (dot >= 0 && dot + 1 < trimmed.size())
        trimmed = trimmed.mid(dot + 1);
    letter_ = trimmed.left(1).toUpper();
    if (letter_.isEmpty()) letter_ = QStringLiteral("?");

    update();
}

void DockButton::setActive(bool active)
{
    if (active_ == active) return;
    active_ = active;
    update();
}

void DockButton::enterEvent(QEnterEvent *event)
{
    update(); // repaint with the hover highlight + enlarged icon
    QPushButton::enterEvent(event);
}

void DockButton::leaveEvent(QEvent *event)
{
    update();
    QPushButton::leaveEvent(event);
}

void DockButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const bool hovered = underMouse();

    // The icon is centered in the FULL cell -- not a shrunk-down area
    // with a strip reserved at the bottom for an indicator. That
    // reserved strip used to be there for the old dot, and since most
    // icons aren't the active window (no dot drawn), the empty strip
    // made every icon look pushed up off-center. The active-window bar
    // below is now just drawn as an overlay near the bottom of this same
    // full-height area, so it never shifts or resizes the icon itself.
    const QRectF iconArea(0, 0, width(), height());

    if (hovered) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, Theme::DockHoverAlpha));
        p.drawRoundedRect(iconArea.adjusted(1, 1, -1, -1),
                          Theme::ButtonRadius, Theme::ButtonRadius);
    }

    const qreal iconSize = Theme::IconSize * (hovered ? 1.10 : 1.0);
    const QRectF iconRect((width() - iconSize) / 2.0,
                          iconArea.center().y() - iconSize / 2.0,
                          iconSize, iconSize);

    if (icon_.icon.isNull()) {
        // No icon found anywhere: a chip with the app's initial, in the
        // theme's own button colors so it still looks deliberate.
        QLinearGradient gradient(iconRect.topLeft(), iconRect.bottomLeft());
        gradient.setColorAt(0, QColor(Theme::ButtonBackground));
        gradient.setColorAt(1, QColor(Theme::ButtonHover));
        p.setPen(Qt::NoPen);
        p.setBrush(gradient);
        p.drawRoundedRect(iconRect, Theme::ButtonRadius + 3, Theme::ButtonRadius + 3);

        QFont font = this->font();
        font.setPixelSize(qMax(12, int(Theme::IconSize * 0.42)));
        font.setBold(true);
        p.setFont(font);
        p.setPen(QColor(Theme::TextColor));
        p.drawText(iconRect, Qt::AlignCenter, letter_);
    } else {
        if (icon_.backing.isValid()) {
            p.setPen(Qt::NoPen);
            p.setBrush(icon_.backing);
            p.drawRoundedRect(iconRect, Theme::ButtonRadius + 3, Theme::ButtonRadius + 3);
        }
        icon_.icon.paint(&p, iconRect.toRect());
    }

    if (active_) {
        // A flat, centered capsule instead of the old dot -- drawn as a
        // pure overlay near the bottom edge of the cell, entirely
        // independent of iconRect above, so it can never nudge the icon
        // off its own centered position or touch its aspect ratio.
        const QRectF bar((width() - Theme::ActiveIndicatorWidth) / 2.0,
                          height() - Theme::ActiveIndicatorGap - Theme::ActiveIndicatorHeight,
                          Theme::ActiveIndicatorWidth, Theme::ActiveIndicatorHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 215));
        p.drawRoundedRect(bar, Theme::ActiveIndicatorHeight / 2.0, Theme::ActiveIndicatorHeight / 2.0);
    }
}

// =====================================================================
// Dock
// =====================================================================

Dock::Dock()
{
    setAttribute(Qt::WA_TranslucentBackground);

    resize(1, Theme::ButtonHeight + Theme::DockPadding * 2);

    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(Theme::DockPadding, Theme::DockPadding,
                                Theme::DockPadding, Theme::DockPadding);
    layout_->setSpacing(Theme::DockSpacing);
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
    // window's title/app_id/state -- handleTitle() below would simply
    // never run, and every button would stay stuck on its initial empty
    // text. Passing `this` as the data pointer is what lets the static
    // handleTitle() callback reach back into this Dock instance.
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &kHandleListener, this);

    if (std::getenv("MERVSHELL_DEBUG"))
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
            if (app->button)
                app->button->setToolTip(QString::fromUtf8(app->title.c_str()));
            return;
        }
    }
}

void Dock::setAppId(zwlr_foreign_toplevel_handle_v1 *handle, const char *appId)
{
    if (std::getenv("MERVSHELL_DEBUG")) {
        std::printf("app_id event: '%s'\n", appId ? appId : "(null)");
        std::fflush(stdout);
    }
    for (auto *app : apps_) {
        if (app->handle == handle) {
            app->appId = appId ? appId : "";

            // Icons are resolved once per app_id and cached (including
            // the "no icon found" case), so this stays cheap even when
            // windows are churned.
            app->icon = resolveDockIcon(QString::fromUtf8(app->appId.c_str()));

            if (app->button) {
                QString name = QString::fromUtf8(app->appId.c_str());
                if (name.isEmpty())
                    name = QString::fromUtf8(app->title.c_str());
                app->button->setDockIcon(app->icon, name);
            }
            return;
        }
    }
}

void Dock::queueState(zwlr_foreign_toplevel_handle_v1 *handle, const wl_array *state)
{
    // The array holds the complete current state each time (not a
    // delta), so compute it from scratch.
    bool active = false;
    const auto *values = static_cast<const uint32_t *>(state->data);
    const size_t count = state->size / sizeof(uint32_t);
    for (size_t i = 0; i < count; ++i) {
        if (values[i] == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED)
            active = true;
    }

    for (auto *app : apps_) {
        if (app->handle == handle) {
            app->pendingActive = active;
            return;
        }
    }
}

void Dock::applyDone(zwlr_foreign_toplevel_handle_v1 *handle)
{
    for (auto *app : apps_) {
        if (app->handle == handle) {
            if (app->button)
                app->button->setActive(app->pendingActive);
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
    // Rounded pill; WA_TranslucentBackground keeps the four corners
    // see-through, so only this rounded rect is actually drawn.
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Theme::DockBackground stays a plain opaque hex everywhere else
    // (it's shared with Taskbar's fill), so the translucency is applied
    // here as a separate alpha on top rather than baking it into the hex
    // string -- Theme::DockBackgroundAlpha is the one knob to turn for
    // "more/less see-through", independent of the actual color.
    QColor pillColor(Theme::DockBackground);
    pillColor.setAlpha(Theme::DockBackgroundAlpha);
    p.setPen(Qt::NoPen);
    p.setBrush(pillColor);
    p.drawRoundedRect(rect(), Theme::DockRadius, Theme::DockRadius);

    QPen pen(QColor(255, 255, 255, Theme::DockBorderAlpha));
    pen.setWidthF(1.0);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                      Theme::DockRadius, Theme::DockRadius);
}

void Dock::rebuild()
{
    while (QLayoutItem *item = layout_->takeAt(0)) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    for (auto *app : apps_) {
        auto *button = new DockButton(this);

        QString name = QString::fromUtf8(app->appId.c_str());
        if (name.isEmpty())
            name = QString::fromUtf8(app->title.c_str());
        button->setDockIcon(app->icon, name);
        button->setToolTip(QString::fromUtf8(app->title.c_str()));
        button->setActive(app->pendingActive);

        auto handle = app->handle;
        QObject::connect(button, &QPushButton::clicked, [this, handle]() {
            activate(handle);
        });

        app->button = button;
        layout_->addWidget(button);
        // Qt requirement: a widget created while its parent is ALREADY
        // visible stays in the "explicitly hidden" state until show() is
        // called -- and QLayout treats hidden widgets as 0x0 items, which
        // would collapse the whole dock's size hint to just its margins
        // (the pill shrank to a 12x12 blob on the second window). Calling
        // show() here is a no-op when the dock itself is hidden (buttons
        // appear together with it) and required when it's already up.
        button->show();
    }

    syncSurfaceSize();
}

void Dock::syncSurfaceSize()
{
    // The layer surface is bottom-anchored only (see main.cpp), which
    // makes the compositor center it horizontally -- so the widget just
    // has to be exactly as wide as its icons.
    adjustSize();

    // set_size() is double-buffered on the compositor side: it takes
    // effect at the next wl_surface commit, which the repaint requested
    // by update() causes. (windowHandle() is null until main.cpp calls
    // makeLayerSurface(); before that, main passes sizeHint() itself.)
    if (QWindow *window = windowHandle()) {
        if (auto *layer = LayerShellQt::Window::get(window))
            layer->setDesiredSize(sizeHint());
    }

    update();

    if (std::getenv("MERVSHELL_DEBUG")) {
        std::printf("sync: apps=%zu items=%d hint=%dx%d size=%dx%d shown=%d\n",
                    apps_.size(), layout_->count(),
                    sizeHint().width(), sizeHint().height(),
                    width(), height(), int(isVisible()));
        for (int i = 0; i < layout_->count(); ++i) {
            QLayoutItem *it = layout_->itemAt(i);
            QWidget *w = it->widget();
            std::printf("  item[%d] hint=%dx%d min=%dx%d w=%p vis=%d hidden=%d whint=%dx%d\n",
                        i, it->sizeHint().width(), it->sizeHint().height(),
                        w ? w->minimumSize().width() : -1,
                        w ? w->minimumSize().height() : -1,
                        static_cast<void *>(w),
                        w ? int(w->isVisible()) : -1,
                        w ? int(w->isHidden()) : -1,
                        w ? w->sizeHint().width() : -1,
                        w ? w->sizeHint().height() : -1);
        }
        std::fflush(stdout);
    }

    // No windows => no dock: an empty floating pill would look like a
    // bug. Hidden until the first window appears.
    if (windowHandle())
        setVisible(!apps_.empty());
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

void Dock::handleAppId(void *data, zwlr_foreign_toplevel_handle_v1 *handle, const char *appId)
{
    static_cast<Dock *>(data)->setAppId(handle, appId);
}

void Dock::handleState(void *data, zwlr_foreign_toplevel_handle_v1 *handle, wl_array *state)
{
    static_cast<Dock *>(data)->queueState(handle, state);
}

void Dock::handleDone(void *data, zwlr_foreign_toplevel_handle_v1 *handle)
{
    static_cast<Dock *>(data)->applyDone(handle);
}

void Dock::handleClosed(void *data, zwlr_foreign_toplevel_handle_v1 *handle)
{
    static_cast<Dock *>(data)->removeApp(handle);
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}

void Dock::handleOutputEnter(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void Dock::handleOutputLeave(void *, zwlr_foreign_toplevel_handle_v1 *, wl_output *) {}
void Dock::handleParent(void *, zwlr_foreign_toplevel_handle_v1 *, zwlr_foreign_toplevel_handle_v1 *) {}