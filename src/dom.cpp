// Ignite DOM implementation + tree builder.
#include "dom.h"
#include "tokenizer.h"

namespace ignite {

namespace {
std::string indent(int depth) { return std::string(static_cast<size_t>(depth) * 2, ' '); }

// HTML void elements: never have children, no end tag required.
bool is_void_element(const std::string& tag) {
    static const char* voids[] = {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "param", "source", "track", "wbr", nullptr
    };
    for (int i = 0; voids[i]; ++i)
        if (tag == voids[i]) return true;
    return false;
}
} // namespace

std::string DocumentNode::debug_string(int depth) const {
    std::string out = indent(depth) + "#document\n";
    for (auto& c : children_) out += c->debug_string(depth + 1);
    return out;
}

std::string ElementNode::debug_string(int depth) const {
    std::string out = indent(depth) + "<" + tag_name_;
    for (auto& [k, v] : attributes_) out += " " + k + "=\"" + v + "\"";
    out += ">\n";
    for (auto& c : children_) out += c->debug_string(depth + 1);
    return out;
}

std::string TextNode::debug_string(int depth) const {
    std::string t = content_;
    // trim for display
    size_t a = t.find_first_not_of(" \t\n\r");
    if (a == std::string::npos) return "";
    size_t b = t.find_last_not_of(" \t\n\r");
    return indent(depth) + "\"" + t.substr(a, b - a + 1) + "\"\n";
}

std::shared_ptr<DocumentNode> build_dom(Tokenizer& tokenizer) {
    auto doc = std::make_shared<DocumentNode>();
    std::vector<std::shared_ptr<Node>> stack;
    stack.push_back(doc);

    Token tok;
    while (tokenizer.next_token(tok)) {
        auto& top = stack.back();
        switch (tok.type) {
            case TokenType::StartTag: {
                auto el = std::make_shared<ElementNode>(tok.tag_name);
                for (auto& [k, v] : tok.attributes) el->set_attribute(k, v);
                top->append_child(el);
                if (!tok.self_closing && !is_void_element(tok.tag_name))
                    stack.push_back(el);
                break;
            }
            case TokenType::EndTag: {
                // pop up to (and including) the matching open element
                for (int i = static_cast<int>(stack.size()) - 1; i > 0; --i) {
                    auto el = std::dynamic_pointer_cast<ElementNode>(stack[i]);
                    if (el && el->tag_name() == tok.tag_name) {
                        stack.resize(static_cast<size_t>(i));
                        break;
                    }
                }
                break;
            }
            case TokenType::Character:
                // (tokenizer already coalesces adjacent characters in Data state)
                top->append_child(std::make_shared<TextNode>(tok.data));
                break;
            case TokenType::Comment:
            case TokenType::Doctype:
                break; // not represented in the DOM tree (v0)
            case TokenType::Eof:
                break;
        }
    }
    return doc;
}

} // namespace ignite
