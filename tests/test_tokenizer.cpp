// Ignite tokenizer tests (no framework; assert-based).
#include <cassert>
#include <iostream>
#include <string>

#include "tokenizer.h"
#include "dom.h"

using namespace ignite;

static int failures = 0;
#define CHECK(cond) \
    do { if (!(cond)) { std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; failures++; } } while (0)

static std::vector<Token> tokenize_all(const std::string& html) {
    Tokenizer t(html);
    std::vector<Token> out;
    Token tok;
    while (t.next_token(tok)) out.push_back(tok);
    return out;
}

void test_basic_tags() {
    auto toks = tokenize_all("<div>hi</div>");
    CHECK(toks.size() == 3);
    CHECK(toks[0].type == TokenType::StartTag && toks[0].tag_name == "div");
    CHECK(toks[1].type == TokenType::Character && toks[1].data == "hi");
    CHECK(toks[2].type == TokenType::EndTag && toks[2].tag_name == "div");
}

void test_attributes() {
    auto toks = tokenize_all("<a href=\"https://x.y/z\" title=hi data-n='a b'>t</a>");
    CHECK(toks.size() == 3);
    CHECK(toks[0].attributes["href"] == "https://x.y/z");
    CHECK(toks[0].attributes["title"] == "hi");
    CHECK(toks[0].attributes["data-n"] == "a b");
}

void test_self_closing_and_comment() {
    auto toks = tokenize_all("<br/><!----><p>x</p>");
    CHECK(toks.size() == 5);
    CHECK(toks[0].type == TokenType::StartTag && toks[0].self_closing);
    CHECK(toks[1].type == TokenType::Comment);
    CHECK(toks[2].type == TokenType::StartTag && toks[2].tag_name == "p");
}

void test_doctype() {
    auto toks = tokenize_all("<!DOCTYPE html><html></html>");
    CHECK(toks.size() == 3);
    CHECK(toks[0].type == TokenType::Doctype);
}

void test_case_folding() {
    auto toks = tokenize_all("<DIV CLASS=\"a\">x</DIV>");
    CHECK(toks[0].tag_name == "div");
    CHECK(toks[0].attributes.count("class") == 1);
    CHECK(toks[2].tag_name == "div");
}

void test_dom_build() {
    auto toks_src = std::string("<div id=\"m\"><p>one</p><p>two</p></div>");
    Tokenizer t(toks_src);
    auto doc = build_dom(t);
    std::string dbg = doc->debug_string();
    CHECK(dbg.find("<div") != std::string::npos);
    CHECK(dbg.find("<p>") != std::string::npos);
    CHECK(dbg.find("\"one\"") != std::string::npos);
}

int main() {
    test_basic_tags();
    test_attributes();
    test_self_closing_and_comment();
    test_doctype();
    test_case_folding();
    test_dom_build();
    if (failures == 0) std::cout << "all tokenizer tests passed\n";
    return failures == 0 ? 0 : 1;
}
