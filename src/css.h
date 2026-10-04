#pragma once
// Ignite CSSOM: lexical analysis, rule-tree construction, right-to-left
// selector matching, and specificity cascade into computed styles.

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ignite {
namespace css {

// Specificity weight: (inline, ids, classes, tags)
struct Specificity {
    int inline_style = 0;
    int ids = 0;
    int classes = 0; // classes, attributes, pseudo-classes
    int tags = 0;    // type selectors, pseudo-elements

    bool operator<(const Specificity& o) const {
        if (inline_style != o.inline_style) return inline_style < o.inline_style;
        if (ids != o.ids) return ids < o.ids;
        if (classes != o.classes) return classes < o.classes;
        return tags < o.tags;
    }
    bool operator==(const Specificity& o) const {
        return inline_style == o.inline_style && ids == o.ids &&
               classes == o.classes && tags == o.tags;
    }
};

struct SimpleSelector {
    std::string tag;                 // empty = universal
    std::string id;                  // empty = none
    std::vector<std::string> classes;
};

// A selector is a chain evaluated right-to-left: selectors[0] must match the
// element itself, selectors[1] an ancestor (descendant combinator), etc.
struct Selector {
    std::vector<SimpleSelector> parts; // rightmost first
    Specificity specificity;
};

struct Declaration {
    std::string property;
    std::string value;
};

struct Rule {
    std::vector<Selector> selectors;
    std::vector<Declaration> declarations;
};

// Computed style: winning declaration per property after cascade.
using ComputedStyle = std::map<std::string, std::string>;

std::vector<Rule> parse_stylesheet(const std::string& css);

// Parses a style attribute ("color: red; display: none") into declarations.
std::vector<Declaration> parse_declarations(const std::string& style_attr);

// Element view for matching (avoids depending on dom.h here).
struct MatchElement {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    const MatchElement* parent = nullptr; // null at root
};

bool matches(const Selector& sel, const MatchElement& el);
ComputedStyle cascade(const std::vector<Rule>& rules, const MatchElement& el);

} // namespace css
} // namespace ignite
