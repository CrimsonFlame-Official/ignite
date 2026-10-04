#pragma once
// Ignite layout: render-tree construction + two-pass reflow.
// Pass 1 (top-down): parents propagate width constraints to children.
// Pass 2 (bottom-up): children resolve heights; parents finalize.

#include <memory>
#include <string>
#include <vector>

#include "css.h"
#include "dom.h"

namespace ignite {

struct Box {
    double x = 0, y = 0; // border-box origin relative to parent
    double content_w = 0, content_h = 0;
    double pad_l = 0, pad_r = 0, pad_t = 0, pad_b = 0;
    double border_l = 0, border_r = 0, border_t = 0, border_b = 0;
    double margin_l = 0, margin_r = 0, margin_t = 0, margin_b = 0;

    double border_box_w() const {
        return content_w + pad_l + pad_r + border_l + border_r;
    }
    double border_box_h() const {
        return content_h + pad_t + pad_b + border_t + border_b;
    }
};

struct LayoutNode {
    std::shared_ptr<Node> dom_node; // null for anonymous boxes
    css::ComputedStyle style;
    Box box;
    std::vector<std::shared_ptr<LayoutNode>> children;
    bool is_block = true;
};

// Builds the render tree, dropping <head>, <script>, <style>, and
// display:none subtrees. Text nodes become anonymous inline boxes.
std::shared_ptr<LayoutNode> build_layout_tree(const std::shared_ptr<Node>& root,
                                              const std::vector<css::Rule>& rules);

// Runs the two-pass reflow. viewport_w is the initial containing-block width.
void reflow(const std::shared_ptr<LayoutNode>& root, double viewport_w);

std::string layout_debug_string(const std::shared_ptr<LayoutNode>& node, int depth = 0);

} // namespace ignite
