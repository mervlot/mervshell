# MervShell build -- simplified learning version.
#
# Unlike the "advanced" git branch, this version doesn't talk to
# wlr-foreign-toplevel-management at all, so there's no generated
# protocol code to compile or link here -- just plain Qt6 + LayerShellQt.

CXX := g++

CXXFLAGS := -std=c++17 -Wall -Wextra -fPIC $(shell pkg-config --cflags Qt6Widgets)

LIBS := $(shell pkg-config --libs Qt6Widgets) -lLayerShellQtInterface -lwayland-client

TARGET := mervshell

CXX_SOURCES := main.cpp Background.cpp Taskbar.cpp Dock.cpp
CXX_OBJECTS := $(CXX_SOURCES:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(CXX_OBJECTS)
	$(CXX) $(CXX_OBJECTS) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -f $(CXX_OBJECTS) $(TARGET)
