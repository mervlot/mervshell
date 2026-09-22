# MervShell build.
#
# This is the same command you were already using --
#   g++ -std=c++17 main.cpp wlr-ftm.o -o mervshell $(pkg-config --cflags Qt6Widgets) \
#       -lLayerShellQtInterface $(pkg-config --libs Qt6Widgets) -lwayland-client
# -- just split across the new files and written as a Makefile so you can
# type `make` instead of retyping that whole line after every edit.
#
# wlr-ftm.o is NOT built by this Makefile -- it uses the one you already
# have sitting in this directory, exactly like your original build
# command did. It only needs wlr-ftm.h (for the #include in
# WaylandGlobals.h/Dock.h); wlr-ftm.c itself doesn't need to be present.
#
# If you ever DO need to rebuild wlr-ftm.o from wlr-ftm.c, build it with
# `gcc`, not `g++`: generated Wayland protocol code declares things like
# `const struct wl_interface X = {...};` at file scope, which is
# externally linked in C but becomes internally linked (like `static`) if
# compiled as C++ -- and `g++ file.c` compiles as C++ regardless of the
# .c extension. That mismatch is a real "undefined reference" waiting to
# happen if this ever gets rebuilt with the wrong compiler:
#   gcc -fPIC -c wlr-ftm.c -o wlr-ftm.o

CXX := g++

CXXFLAGS := -std=c++17 -Wall -Wextra -fPIC $(shell pkg-config --cflags Qt6Widgets)

LIBS := $(shell pkg-config --libs Qt6Widgets) -lLayerShellQtInterface -lwayland-client

TARGET := mervshell

CXX_SOURCES := main.cpp Dock.cpp Taskbar.cpp bg.cpp WaylandGlobals.cpp
CXX_OBJECTS := $(CXX_SOURCES:.cpp=.o)
PROTOCOL_OBJECT := wlr-ftm.o

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(CXX_OBJECTS) $(PROTOCOL_OBJECT)
	$(CXX) $(CXX_OBJECTS) $(PROTOCOL_OBJECT) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(CXX_OBJECTS) $(TARGET)
	# wlr-ftm.o is never touched by clean -- it's yours, not a build product of this Makefile.