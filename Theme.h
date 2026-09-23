#pragma once

// All of MervShell's colors and sizes, in one place. These are plain
// compile-time constants -- no config file, no loading code. Change a
// value here and rebuild to see the difference.
namespace Theme
{
    constexpr const char *BackgroundColor  = "#101418";
    constexpr const char *BarBackground    = "#10141c";
    constexpr const char *ButtonBackground = "#202632";
    constexpr const char *ButtonHover      = "#303949";
    constexpr const char *TextColor        = "#ffffff";

    constexpr int TaskbarHeight = 32;
    constexpr int DockHeight    = 60;
    constexpr int ButtonHeight  = 40;
    constexpr int ButtonRadius  = 8;
    constexpr int Spacing       = 8;
    constexpr int Margin        = 8;
}
