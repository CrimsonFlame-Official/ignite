// Ignite layout implementation.
#include "layout.h"

#include <cstdio>

namespace ignite {

namespace {
// Lengths: "12px" -> 12.0, "50%" -> fraction of containing block, else 0.
double parse_length(const std::string& v, double containing) {
    if (v.empty()) return 0;
    if (v.size() > 2 && v.compare(v.size() - 2, 2, "px") == 0)
        return std::stod(v.substr(0, v.size() - 2));
    if (!v.empty() && v.back() == '%')
        return containing * std::stod(v.substr(0, v.size() - 1)) / 100.0;
    try { return std::stod(v); } catch (...) { return 0; }
}

bool is_hidden_tag(const std::string& tag) {
    return tag == "head" || tag == "script" || tag == "style" ||
           tag == "meta" || tag == "link" || tag == "title";
}

css::MatchElement to_match(const std::shared_ptr<Node>& node,
                           std::vector<css::MatchElement>& storage) {
    // Build a MatchElement chain up the ancestor axis.
    std::vector<std::shared_ptr<Node>> chain;
    for (auto n = node; n; n = n->parent()) chain.push_back(n);
    storage.clear();
    storage.reserve(chain.size());
    for (int i = static_cast<int>(chain.size()) - 1; i >= 0; --i) {
        css::MatchElement m;
        if (chain[i]->type() == NodeType::Element) {
            auto* el = static_cast<ElementNode*>(chain[i].get());
            m.tag = el->tag_name();
            m.id = el->get_attribute("id");
            std::string cls = el->get_attribute("class");
            size_t p = 0;
            while (p <= cls.size()) {
                size_t q = cls.find(' ', p);
                std::string c = cls.substr(p, q == std::string::npos ? q : q - p);
                if (!c.empty()) m.classes.push_back(c);
                if (q == std::string::npos) break;
                p = q + 1;
            }
        }
        m.parent = storage.empty() ? nullptr : &storage.back();
        storage.push_back(std::move(m));
    }
    return storage.back();
}

std::shared_ptr<LayoutNode> convert(const std::shared_ptr<Node>& node,
                                    const std::vector<css::Rule>& rules) {
    if (node->type() == NodeType::Text) {
        auto ln = std::make_shared<LayoutNode>();
        ln->dom_node = node;
        ln->is_block = false;
        return ln;
    }
    if (node->type() != NodeType::Element) {
        // document node: transparent wrapper
        auto ln = std::make_shared<LayoutNode>();
        ln->dom_node = node;
        for (auto& c : node->children()) {
            auto child = convert(c, rules);
            if (child) ln->children.push_back(child);
        }
        return ln;
    }
    auto* el = static_cast<ElementNode*>(node.get());
    if (is_hidden_tag(el->tag_name())) return nullptr;

    std::vector<css::MatchElement> storage;
    css::MatchElement me = to_match(node, storage);
    css::ComputedStyle style = css::cascade(rules, me);
    // Inline style attribute: highest specificity, always wins.
    std::string inline_css = el->get_attribute("style");
    if (!inline_css.empty()) {
        for (auto& d : css::parse_declarations(inline_css))
            style[d.property] = d.value;
    }

    auto disp = style.find("display");
    if (disp != style.end() && disp->second == "none") return nullptr;

    auto ln = std::make_shared<LayoutNode>();
    ln->dom_node = node;
    ln->style = std::move(style);
    ln->is_block = true;
    auto it = ln->style.find("display");
    if (it != ln->style.end() && (it->second == "inline" || it->second == "inline-block"))
        ln->is_block = false;
    for (auto& c : node->children()) {
        auto child = convert(c, rules);
        if (child) ln->children.push_back(child);
    }
    return ln;
}

// Pass 1: top-down. Resolve widths, x/y origins; recurse.
void pass1(const std::shared_ptr<LayoutNode>& n, double containing_w, double cursor_y) {
    auto& b = n->box;
    auto get = [&](const char* prop) {
        auto it = n->style.find(prop);
        return it == n->style.end() ? "" : it->second;
    };
    b.margin_l = parse_length(get("margin-left"), containing_w);
    b.margin_r = parse_length(get("margin-right"), containing_w);
    b.pad_l = parse_length(get("padding-left"), containing_w);
    b.pad_r = parse_length(get("padding-right"), containing_w);
    b.border_l = parse_length(get("border-left-width"), containing_w);
    b.border_r = parse_length(get("border-right-width"), containing_w);

    double avail = containing_w - b.margin_l - b.margin_r;
    std::string w = get("width");
    b.content_w = w.empty() ? avail - b.pad_l - b.pad_r - b.border_l - b.border_r
                            : parse_length(w, containing_w);
    if (b.content_w < 0) b.content_w = 0;

    b.x = b.margin_l;
    b.y = cursor_y + parse_length(get("margin-top"), containing_w);

    double child_y = b.y + b.border_t + b.pad_t;
    double child_w = b.content_w;
    for (auto& c : n->children) {
        if (c->dom_node && c->dom_node->type() == NodeType::Text) {
            // anonymous inline: simple line box advance (v0: fixed metrics)
            c->box.content_w = child_w;
            c->box.content_h = 16.0; // TODO: real text metrics
            c->box.x = 0;
            c->box.y = child_y;
            child_y += 16.0;
            continue;
        }
        pass1(c, child_w, child_y);
        child_y = c->box.y + c->box.border_box_h() +
                  parse_length(c->style.count("margin-bottom") ? c->style.at("margin-bottom") : "", containing_w);
    }
    // stash end cursor for pass 2
    b.content_h = child_y - (b.y + b.border_t + b.pad_t); // provisional
}

// Pass 2: bottom-up. Explicit heights win; otherwise use accumulated children.
void pass2(const std::shared_ptr<LayoutNode>& n, double containing_w) {
    for (auto& c : n->children) {
        if (c->dom_node && c->dom_node->type() == NodeType::Text) continue;
        pass2(c, n->box.content_w);
    }
    auto it = n->style.find("height");
    if (it != n->style.end() && !it->second.empty() && it->second != "auto") {
        n->box.content_h = parse_length(it->second, containing_w);
    }
    // else keep pass-1 provisional height from children
    auto& b = n->box;
    b.pad_t = b.pad_b = 0;
    auto get = [&](const char* prop) {
        auto it2 = n->style.find(prop);
        return it2 == n->style.end() ? "" : it2->second;
    };
    b.pad_t = parse_length(get("padding-top"), containing_w);
    b.pad_b = parse_length(get("padding-bottom"), containing_w);
    b.border_t = parse_length(get("border-top-width"), containing_w);
    b.border_b = parse_length(get("border-bottom-width"), containing_w);
}
} // namespace

std::shared_ptr<LayoutNode> build_layout_tree(const std::shared_ptr<Node>& root,
                                              const std::vector<css::Rule>& rules) {
    return convert(root, rules);
}

void reflow(const std::shared_ptr<LayoutNode>& root, double viewport_w) {
    pass1(root, viewport_w, 0.0);
    pass2(root, viewport_w);
}

std::string layout_debug_string(const std::shared_ptr<LayoutNode>& node, int depth) {
    std::string out(depth * 2, ' ');
    std::string label = "?";
    if (node->dom_node) {
        if (node->dom_node->type() == NodeType::Element)
            label = "<" + static_cast<ElementNode*>(node->dom_node.get())->tag_name() + ">";
        else if (node->dom_node->type() == NodeType::Text)
            label = "#text";
        else
            label = "#document";
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), " %s x=%.0f y=%.0f w=%.0f h=%.0f\n", label.c_str(),
                  node->box.x, node->box.y, node->box.border_box_w(),
                  node->box.border_box_h());
    out += buf;
    for (auto& c : node->children) out += layout_debug_string(c, depth + 1);
    return out;
}

} // namespace ignite
