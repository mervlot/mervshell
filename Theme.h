#pragma once

// Every color MervShell's bars use, in one place. Change a value here and
// it updates Background/Taskbar/Dock together -- nothing else to touch.
namespace Theme
{
constexpr const char *BackgroundColor = "#0000";
constexpr const char *DockBackground = "#10141c";
constexpr const char *TaskBackground = "#10141c";
    constexpr const char *ButtonBackground = "#202632";
    constexpr const char *ButtonHover      = "#303949";
    constexpr const char *TextColor        = "#ffffff";
    constexpr const char *MutedTextColor   = "#9aa4b2";

    constexpr int ButtonHeight  = 50;  // dock icon cell: icon strip + indicator dot row
    constexpr int ButtonRadius  = 6;   // corner radius of the hover highlight
    constexpr int BarSpacing    = 4;
    constexpr int BarMargin     = 4;

    // --- Dock: the floating macOS-style pill --------------------------------
    // The dock anchors to the bottom edge ONLY (see main.cpp), so the
    // compositor centers it horizontally and it is exactly as wide as its
    // icons -- it grows/shrinks as windows open/close.
    constexpr int ButtonWidth      = 46;   // width of one icon cell
    constexpr int IconSize         = 34;   // drawn icon size (x1.10 while hovered)
    constexpr int DockRadius       = 16;   // pill corner radius
    constexpr int DockPadding      = 6;    // icons -> pill edge
    constexpr int DockSpacing      = 3;    // gap between icon cells
    constexpr int DockMarginBottom = 2 ;    // gap between pill and screen bottom
    constexpr int DockBorderAlpha  = 28;   // 0-255: subtle rim highlight on the pill
    constexpr int DockHoverAlpha   = 36;   // 0-255: highlight behind the hovered icon
    constexpr int DockBackgroundAlpha = 200; // 0-255: pill translucency (255 = opaque)

    // The focused-window indicator: a flat capsule near the bottom of the
    // icon cell, drawn as a pure overlay (see DockButton::paintEvent) so
    // it never shifts or resizes the icon itself.
    constexpr int ActiveIndicatorWidth  = 16;
    constexpr int ActiveIndicatorHeight = 3;
    constexpr int ActiveIndicatorGap    = 4;  // gap between the bar and the very bottom of the cell

    // How much vertical space at the bottom of the screen the dock
    // reserves for itself (the layer-shell "exclusive zone"). Windows
    // will not extend into this strip -- this is what makes the dock
    // "clip" windows above it instead of floating over their content.
    // Matches the pill's own real height (ButtonHeight + top/bottom
    // padding) plus the gap it sits above the screen edge, so the
    // reserved strip is exactly as tall as the dock actually is -- no
    // gap where a window could still sneak underneath, no wasted space
    // above it either.
    constexpr int DockExclusiveZone = ButtonHeight + DockPadding * 2 + DockMarginBottom;
}