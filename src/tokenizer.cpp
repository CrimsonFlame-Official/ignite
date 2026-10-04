// Ignite HTML5 tokenizer DFA implementation.
#include "tokenizer.h"

#include <cctype>

namespace ignite {

std::string token_type_name(TokenType t) {
    switch (t) {
        case TokenType::Doctype:   return "DOCTYPE";
        case TokenType::StartTag:  return "StartTag";
        case TokenType::EndTag:    return "EndTag";
        case TokenType::Character: return "Character";
        case TokenType::Comment:   return "Comment";
        case TokenType::Eof:       return "EOF";
    }
    return "?";
}

Tokenizer::Tokenizer(const std::string& input) : input_(input) {}

void Tokenizer::emit_current_tag(Token& out, bool is_end) {
    out.type = is_end ? TokenType::EndTag : TokenType::StartTag;
    out.tag_name = current_tag_name_;
    out.attributes = current_attrs_;
    out.self_closing = current_self_closing_;
    current_tag_name_.clear();
    current_attrs_.clear();
    current_attr_name_.clear();
    current_attr_value_.clear();
    current_self_closing_ = false;
    current_is_end_ = false;
}

static bool is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

static char lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

bool Tokenizer::next_token(Token& out) {
    out = Token{};
    char_buffer_.clear();

    while (true) {
        char c = pos_ < input_.size() ? input_[pos_] : '\0';
        bool eof = pos_ >= input_.size();

        switch (state_) {
            case State::Data:
                if (eof) {
                    if (!char_buffer_.empty()) {
                        out.type = TokenType::Character;
                        out.data = char_buffer_;
                        return true;
                    }
                    out.type = TokenType::Eof;
                    return false;
                }
                if (c == '<') {
                    state_ = State::TagOpen;
                    pos_++;
                } else {
                    char_buffer_ += c;
                    pos_++;
                }
                break;

            case State::TagOpen:
                if (eof) { // treat as text
                    char_buffer_ = "<";
                    state_ = State::Data;
                } else if (c == '/') {
                    current_is_end_ = true;
                    state_ = State::EndTagOpen;
                    pos_++;
                } else if (c == '!') {
                    // <!-- comment or <!DOCTYPE
                    if (input_.compare(pos_ + 1, 2, "--") == 0) {
                        pos_ += 3;
                        comment_buffer_.clear();
                        state_ = State::Comment;
                    } else {
                        pos_++;
                        state_ = State::Doctype;
                        char_buffer_.clear();
                    }
                } else if (std::isalpha(static_cast<unsigned char>(c))) {
                    current_is_end_ = false;
                    current_tag_name_.clear();
                    state_ = State::TagName;
                    // don't consume; TagName handles it
                } else {
                    char_buffer_ = "<";
                    state_ = State::Data;
                }
                break;

            case State::EndTagOpen:
                if (std::isalpha(static_cast<unsigned char>(c))) {
                    current_tag_name_.clear();
                    state_ = State::TagName;
                } else {
                    // bogus: bail back to data
                    state_ = State::Data;
                }
                break;

            case State::TagName:
                if (eof) { state_ = State::Data; break; }
                if (is_ws(c)) {
                    state_ = State::BeforeAttributeName;
                    pos_++;
                } else if (c == '/') {
                    state_ = State::SelfClosingStartTag;
                    pos_++;
                } else if (c == '>') {
                    pos_++;
                    state_ = State::Data;
                    emit_current_tag(out, current_is_end_);
                    return true;
                } else {
                    current_tag_name_ += lower(c);
                    pos_++;
                }
                break;

            case State::BeforeAttributeName:
                if (eof) { state_ = State::Data; break; }
                if (is_ws(c)) { pos_++; }
                else if (c == '/') { state_ = State::SelfClosingStartTag; pos_++; }
                else if (c == '>') {
                    pos_++;
                    state_ = State::Data;
                    emit_current_tag(out, current_is_end_);
                    return true;
                } else {
                    current_attr_name_.clear();
                    current_attr_value_.clear();
                    state_ = State::AttributeName;
                }
                break;

            case State::AttributeName:
                if (eof) { state_ = State::Data; break; }
                if (is_ws(c)) { state_ = State::AfterAttributeName; pos_++; }
                else if (c == '/') {
                    current_attrs_[current_attr_name_] = current_attr_value_;
                    state_ = State::SelfClosingStartTag; pos_++;
                } else if (c == '=') {
                    state_ = State::BeforeAttributeValue; pos_++;
                } else if (c == '>') {
                    current_attrs_[current_attr_name_] = current_attr_value_;
                    pos_++;
                    state_ = State::Data;
                    emit_current_tag(out, current_is_end_);
                    return true;
                } else {
                    current_attr_name_ += lower(c);
                    pos_++;
                }
                break;

            case State::AfterAttributeName:
                if (eof) { state_ = State::Data; break; }
                if (is_ws(c)) { pos_++; }
                else if (c == '=') { state_ = State::BeforeAttributeValue; pos_++; }
                else {
                    current_attrs_[current_attr_name_] = current_attr_value_;
                    state_ = State::BeforeAttributeName;
                }
                break;

            case State::BeforeAttributeValue:
                if (eof) { state_ = State::Data; break; }
                if (is_ws(c)) { pos_++; }
                else if (c == '"' || c == '\'') {
                    quote_char_ = c;
                    current_attr_value_.clear();
                    state_ = State::AttributeValueQuoted;
                    pos_++;
                } else if (c == '>') {
                    current_attrs_[current_attr_name_] = "";
                    pos_++;
                    state_ = State::Data;
                    emit_current_tag(out, current_is_end_);
                    return true;
                } else {
                    current_attr_value_.clear();
                    state_ = State::AttributeValueUnquoted;
                }
                break;

            case State::AttributeValueQuoted:
                if (eof) { state_ = State::Data; break; }
                if (c == quote_char_) {
                    current_attrs_[current_attr_name_] = current_attr_value_;
                    state_ = State::BeforeAttributeName;
                    pos_++;
                } else {
                    current_attr_value_ += c;
                    pos_++;
                }
                break;

            case State::AttributeValueUnquoted:
                if (eof) { state_ = State::Data; break; }
                if (is_ws(c)) {
                    current_attrs_[current_attr_name_] = current_attr_value_;
                    state_ = State::BeforeAttributeName;
                    pos_++;
                } else if (c == '>') {
                    current_attrs_[current_attr_name_] = current_attr_value_;
                    pos_++;
                    state_ = State::Data;
                    emit_current_tag(out, current_is_end_);
                    return true;
                } else {
                    current_attr_value_ += c;
                    pos_++;
                }
                break;

            case State::SelfClosingStartTag:
                if (c == '>') {
                    pos_++;
                    state_ = State::Data;
                    current_self_closing_ = true;
                    emit_current_tag(out, current_is_end_);
                    return true;
                }
                state_ = State::BeforeAttributeName;
                break;

            case State::Comment:
                if (eof) {
                    out.type = TokenType::Comment;
                    out.data = comment_buffer_;
                    state_ = State::Data;
                    return true;
                }
                if (c == '-' && input_.compare(pos_, 3, "-->") == 0) {
                    pos_ += 3;
                    state_ = State::Data;
                    out.type = TokenType::Comment;
                    out.data = comment_buffer_;
                    return true;
                }
                comment_buffer_ += c;
                pos_++;
                break;

            case State::Doctype:
                if (eof || c == '>') {
                    if (!eof) pos_++;
                    state_ = State::Data;
                    out.type = TokenType::Doctype;
                    out.data = char_buffer_;
                    return true;
                }
                char_buffer_ += c;
                pos_++;
                break;
        }

        // Flush coalesced character data when leaving Data state with content.
        if (state_ != State::Data && !char_buffer_.empty() &&
            (state_ == State::TagOpen)) {
            out.type = TokenType::Character;
            out.data = char_buffer_;
            char_buffer_.clear();
            return true;
        }
    }
}

} // namespace ignite
