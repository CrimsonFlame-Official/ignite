#pragma once
// Ignite paint: records draw commands per layout box.
// (v0: command list only; a raster backend plugs in later.)

#include <string>
#include <vector>

#include "layout.h"

namespace ignite {

enum class PaintCmdType {
    FillRect,
    DrawText,
    DrawBorder,
};

struct PaintCommand {
    PaintCmdType type;
    double x, y, w, h;
    std::string color; // css color string, e.g. "#ff0000" or "red"
    std::string text;  // DrawText only
};

// Walks the layout tree and emits paint commands in tree order.
std::vector<PaintCommand> paint(const std::shared_ptr<LayoutNode>& root);

} // namespace ignite
