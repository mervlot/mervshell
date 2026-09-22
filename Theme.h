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

    constexpr int ButtonHeight  = 32;
    constexpr int ButtonRadius  = 6;
    constexpr int BarSpacing    = 4;
    constexpr int BarMargin     = 4;
}
