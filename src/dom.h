#pragma once
// Ignite DOM: reference-counted node tree.
//   - children: shared_ptr (strong, parent owns children)
//   - parent:   weak_ptr   (no cycles, allows child->parent traversal)
// Node types: Document, Element, Text.

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ignite {

enum class NodeType {
    Document,
    Element,
    Text,
};

class Node : public std::enable_shared_from_this<Node> {
public:
    explicit Node(NodeType type) : type_(type) {}
    virtual ~Node() = default;

    NodeType type() const { return type_; }

    std::shared_ptr<Node> parent() const { return parent_.lock(); }
    const std::vector<std::shared_ptr<Node>>& children() const { return children_; }

    void append_child(const std::shared_ptr<Node>& child) {
        child->parent_ = shared_from_this();
        children_.push_back(child);
    }

    virtual std::string debug_string(int depth = 0) const = 0;

protected:
    NodeType type_;
    std::weak_ptr<Node> parent_;
    std::vector<std::shared_ptr<Node>> children_;
};

class DocumentNode : public Node {
public:
    DocumentNode() : Node(NodeType::Document) {}
    std::string debug_string(int depth = 0) const override;
};

class ElementNode : public Node {
public:
    explicit ElementNode(const std::string& tag_name)
        : Node(NodeType::Element), tag_name_(tag_name) {}

    const std::string& tag_name() const { return tag_name_; }

    void set_attribute(const std::string& name, const std::string& value) {
        attributes_[name] = value;
    }
    std::string get_attribute(const std::string& name) const {
        auto it = attributes_.find(name);
        return it != attributes_.end() ? it->second : "";
    }
    bool has_attribute(const std::string& name) const {
        return attributes_.find(name) != attributes_.end();
    }
    const std::map<std::string, std::string>& attributes() const { return attributes_; }

    std::string debug_string(int depth = 0) const override;

private:
    std::string tag_name_;
    std::map<std::string, std::string> attributes_;
};

class TextNode : public Node {
public:
    explicit TextNode(const std::string& content)
        : Node(NodeType::Text), content_(content) {}

    const std::string& content() const { return content_; }

    std::string debug_string(int depth = 0) const override;

private:
    std::string content_;
};

// Builds a DOM tree from a token stream. Handles implicit nesting via a
// stack of open elements; mismatched end tags pop to the matching element.
std::shared_ptr<DocumentNode> build_dom(class Tokenizer& tokenizer);

} // namespace ignite
