// Ignite paint implementation.
#include "paint.h"

#include "dom.h"

namespace ignite {

namespace {
void paint_node(const std::shared_ptr<LayoutNode>& n, std::vector<PaintCommand>& out) {
    auto bg = n->style.find("background-color");
    if (bg != n->style.end()) {
        out.push_back({PaintCmdType::FillRect, n->box.x, n->box.y,
                       n->box.border_box_w(), n->box.border_box_h(), bg->second, ""});
    }
    if (n->dom_node && n->dom_node->type() == NodeType::Text) {
        auto* t = static_cast<TextNode*>(n->dom_node.get());
        auto fg = n->style.find("color");
        out.push_back({PaintCmdType::DrawText, n->box.x, n->box.y,
                       n->box.content_w, n->box.content_h,
                       fg == n->style.end() ? "#000000" : fg->second, t->content()});
    }
    for (auto& c : n->children) paint_node(c, out);
}
} // namespace

std::vector<PaintCommand> paint(const std::shared_ptr<LayoutNode>& root) {
    std::vector<PaintCommand> cmds;
    paint_node(root, cmds);
    return cmds;
}

} // namespace ignite
