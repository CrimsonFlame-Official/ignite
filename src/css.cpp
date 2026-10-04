// Ignite CSSOM implementation.
#include "css.h"

#include <cctype>
#include <cstring>

namespace ignite {
namespace css {

namespace {
// --- tiny lexer -----------------------------------------------------------
struct Lexer {
    const std::string& s;
    size_t i = 0;
    Lexer(const std::string& src) : s(src) {}
    bool eof() const { return i >= s.size(); }
    char peek() const { return eof() ? '\0' : s[i]; }
    void skip_ws() {
        while (!eof() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
    }
    // reads until one of the delimiter chars (delimiter not consumed)
    std::string read_until(const char* delims) {
        size_t start = i;
        while (!eof() && !std::strchr(delims, s[i])) i++;
        return s.substr(start, i - start);
    }
};

std::string trim(const std::string& v) {
    size_t a = v.find_first_not_of(" \t\n\r");
    if (a == std::string::npos) return "";
    size_t b = v.find_last_not_of(" \t\n\r");
    return v.substr(a, b - a + 1);
}

// "div#main.note" -> SimpleSelector{tag=div,id=main,classes=[note]}
SimpleSelector parse_simple(const std::string& text) {
    SimpleSelector out;
    size_t i = 0, n = text.size();
    // tag (or * for universal)
    size_t start = i;
    while (i < n && text[i] != '#' && text[i] != '.') i++;
    std::string tag = text.substr(start, i - start);
    if (tag != "*" && !tag.empty()) out.tag = tag;
    while (i < n) {
        char kind = text[i++];
        start = i;
        while (i < n && text[i] != '#' && text[i] != '.') i++;
        std::string name = text.substr(start, i - start);
        if (kind == '#') out.id = name;
        else out.classes.push_back(name);
    }
    return out;
}

Specificity specificity_of(const SimpleSelector& s) {
    Specificity sp;
    if (!s.id.empty()) sp.ids += 1;
    sp.classes += static_cast<int>(s.classes.size());
    if (!s.tag.empty()) sp.tags += 1;
    return sp;
}
} // namespace

std::vector<Declaration> parse_declarations(const std::string& style_attr) {
    std::vector<Declaration> decls;
    Lexer lx(style_attr);
    while (!lx.eof()) {
        lx.skip_ws();
        if (lx.eof()) break;
        std::string prop = trim(lx.read_until(":;"));
        if (lx.peek() == ':') {
            lx.i++;
            std::string val = trim(lx.read_until(";"));
            if (!prop.empty() && !val.empty()) decls.push_back({prop, val});
        }
        if (!lx.eof() && lx.peek() == ';') lx.i++;
    }
    return decls;
}

std::vector<Rule> parse_stylesheet(const std::string& css) {
    std::vector<Rule> rules;
    Lexer lx(css);
    while (!lx.eof()) {
        lx.skip_ws();
        if (lx.eof()) break;
        if (lx.peek() == '}') { lx.i++; continue; }
        // selector list up to '{'
        std::string sel_text = trim(lx.read_until("{"));
        if (lx.eof()) break;
        lx.i++; // consume '{'
        std::vector<Declaration> decls;
        while (!lx.eof() && lx.peek() != '}') {
            lx.skip_ws();
            if (lx.peek() == '}') break;
            std::string prop = trim(lx.read_until(":;}"));
            if (lx.peek() == ':') {
                lx.i++;
                std::string val = trim(lx.read_until(";})"));
                if (!prop.empty()) decls.push_back({prop, val});
            }
            if (lx.peek() == ';') lx.i++;
        }
        if (!lx.eof()) lx.i++; // consume '}'

        Rule rule;
        rule.declarations = std::move(decls);
        // split selector list on ','
        size_t p = 0;
        while (p <= sel_text.size()) {
            size_t q = sel_text.find(',', p);
            std::string one = trim(sel_text.substr(p, q == std::string::npos ? q : q - p));
            if (!one.empty()) {
                // split descendant chain on whitespace; store rightmost first
                Selector sel;
                size_t a = 0;
                std::vector<std::string> chain;
                while (a <= one.size()) {
                    size_t b = one.find(' ', a);
                    std::string part = trim(one.substr(a, b == std::string::npos ? b : b - a));
                    if (!part.empty()) chain.push_back(part);
                    if (b == std::string::npos) break;
                    a = b + 1;
                }
                for (int k = static_cast<int>(chain.size()) - 1; k >= 0; --k) {
                    SimpleSelector ss = parse_simple(chain[k]);
                    sel.specificity.ids += specificity_of(ss).ids;
                    sel.specificity.classes += specificity_of(ss).classes;
                    sel.specificity.tags += specificity_of(ss).tags;
                    sel.parts.push_back(std::move(ss));
                }
                rule.selectors.push_back(std::move(sel));
            }
            if (q == std::string::npos) break;
            p = q + 1;
        }
        if (!rule.selectors.empty()) rules.push_back(std::move(rule));
    }
    return rules;
}

namespace {
bool simple_matches(const SimpleSelector& s, const MatchElement& el) {
    if (!s.tag.empty() && s.tag != el.tag) return false;
    if (!s.id.empty() && s.id != el.id) return false;
    for (auto& c : s.classes) {
        bool found = false;
        for (auto& ec : el.classes)
            if (ec == c) { found = true; break; }
        if (!found) return false;
    }
    return true;
}
} // namespace

// Right-to-left: parts[0] matches the element; each further part must match
// some ancestor (descendant combinator).
bool matches(const Selector& sel, const MatchElement& el) {
    if (sel.parts.empty()) return false;
    if (!simple_matches(sel.parts[0], el)) return false;
    const MatchElement* anc = el.parent;
    for (size_t k = 1; k < sel.parts.size(); ++k) {
        bool found = false;
        while (anc) {
            if (simple_matches(sel.parts[k], *anc)) { found = true; anc = anc->parent; break; }
            anc = anc->parent;
        }
        if (!found) return false;
    }
    return true;
}

ComputedStyle cascade(const std::vector<Rule>& rules, const MatchElement& el) {
    // property -> (specificity, order, value); later rules win ties.
    struct Winner { Specificity sp; size_t order; std::string value; };
    std::map<std::string, Winner> winners;
    size_t order = 0;
    for (auto& rule : rules) {
        for (auto& sel : rule.selectors) {
            if (!matches(sel, el)) continue;
            for (auto& d : rule.declarations) {
                auto it = winners.find(d.property);
                if (it == winners.end() || it->second.sp < sel.specificity ||
                    (it->second.sp == sel.specificity)) {
                    winners[d.property] = {sel.specificity, order, d.value};
                }
            }
        }
        ++order;
    }
    ComputedStyle out;
    for (auto& [k, w] : winners) out[k] = w.value;
    return out;
}

} // namespace css
} // namespace ignite
