// Ignite demo driver: tokenize -> DOM -> CSSOM -> layout -> paint.
#include <iostream>
#include <string>

#include "css.h"
#include "dom.h"
#include "layout.h"
#include "paint.h"
#include "tokenizer.h"

int main() {
    const std::string html = R"(
<!DOCTYPE html>
<html>
<head><title>Ignite demo</title></head>
<body>
  <div id="main" class="wrap">
    <p class="note">Hello, <b>Ignite</b>!</p>
    <p style="display:none">hidden</p>
  </div>
</body>
</html>)";

    const std::string css = R"(
#main { width: 800px; padding: 10px; background-color: #f0f0f0; }
.note { color: #333333; }
.wrap p { margin-top: 8px; }
)";

    ignite::Tokenizer tok(html);
    auto dom = ignite::build_dom(tok);
    std::cout << "=== DOM ===\n" << dom->debug_string() << "\n";

    auto rules = ignite::css::parse_stylesheet(css);
    std::cout << "=== CSS rules: " << rules.size() << " ===\n\n";

    auto layout = ignite::build_layout_tree(dom, rules);
    ignite::reflow(layout, 1024.0);
    std::cout << "=== LAYOUT (1024px viewport) ===\n"
              << ignite::layout_debug_string(layout) << "\n";

    auto cmds = ignite::paint(layout);
    std::cout << "=== PAINT (" << cmds.size() << " commands) ===\n";
    for (auto& c : cmds) {
        std::cout << (c.type == ignite::PaintCmdType::FillRect ? "FillRect" : "DrawText")
                  << " x=" << c.x << " y=" << c.y << " w=" << c.w << " h=" << c.h
                  << " color=" << c.color;
        if (!c.text.empty()) std::cout << " text=\"" << c.text.substr(0, 24) << "\"";
        std::cout << "\n";
    }
    return 0;
}
