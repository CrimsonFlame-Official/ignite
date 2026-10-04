#pragma once
// Ignite HTML5 tokenizer: deterministic finite automaton over a byte stream.
// States: Data, TagOpen, EndTagOpen, TagName, BeforeAttributeName,
//          AfterAttributeName, BeforeAttributeValue, Comment, Doctype.
// Emits: DOCTYPE, StartTag, EndTag, Character, Comment, EOF.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ignite {

enum class TokenType {
    Doctype,
    StartTag,
    EndTag,
    Character,
    Comment,
    Eof,
};

struct Token {
    TokenType type = TokenType::Eof;
    std::string tag_name;                          // StartTag / EndTag
    std::map<std::string, std::string> attributes; // StartTag
    bool self_closing = false;                     // StartTag
    std::string data;                              // Character / Comment / Doctype
};

enum class State {
    Data,
    TagOpen,
    EndTagOpen,
    TagName,
    BeforeAttributeName,
    AttributeName,
    AfterAttributeName,
    BeforeAttributeValue,
    AttributeValueQuoted,
    AttributeValueUnquoted,
    SelfClosingStartTag,
    CommentStart,
    Comment,
    CommentEnd,
    Doctype,
};

class Tokenizer {
public:
    explicit Tokenizer(const std::string& input);

    // Returns false when the EOF token has been emitted.
    bool next_token(Token& out);

private:
    void emit_current_tag(Token& out, bool is_end);

    std::string input_;
    size_t pos_ = 0;
    State state_ = State::Data;

    // In-progress tag assembly
    bool current_is_end_ = false;
    std::string current_tag_name_;
    std::map<std::string, std::string> current_attrs_;
    std::string current_attr_name_;
    std::string current_attr_value_;
    char quote_char_ = 0;
    bool current_self_closing_ = false;
    std::string char_buffer_;
    std::string comment_buffer_;
};

std::string token_type_name(TokenType t);

} // namespace ignite
