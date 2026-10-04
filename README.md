# Ignite — Layout Engine

Part of the **Spark** browser runtime. Ignite ingests raw HTML/CSS byte streams,
compiles them into an in-memory tree, computes exact geometric bounds, and
executes paint routines.

## Pipeline

```
HTML bytes → Tokenizer (DFA) → DOM tree → + CSSOM → Layout tree → Paint
```

1. **Tokenizer** (`src/tokenizer.*`) — HTML5 tokenizer as a deterministic finite
   automaton. States: Data, TagOpen, EndTagOpen, TagName, BeforeAttributeName,
   Comment, Doctype. Emits DOCTYPE / StartTag / EndTag / Character / Comment / EOF.
2. **DOM** (`src/dom.*`) — `shared_ptr` node tree, `weak_ptr` parent links
   (no cyclic leaks), multi-directional traversal.
3. **CSSOM** (`src/css.*`) — CSS lexer, rule-tree construction, right-to-left
   selector matching, specificity cascade (Inline > ID > Class > Tag).
4. **Layout** (`src/layout.*`) — render-tree filtering (`<head>`, `<script>`,
   `display:none` dropped), box model (content/padding/border/margin), two-pass
   reflow: top-down constraint propagation, bottom-up size determination.
5. **Paint** (`src/paint.*`) — records paint commands per layout box.

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/ignite_demo
```

Requires a C++17 compiler and CMake 3.16+.

## Roadmap

- [x] HTML tokenizer DFA
- [x] DOM tree with smart pointers
- [x] CSS lexer + rule tree
- [ ] Full selector matching (descendant/child/sibling combinators)
- [ ] Inline layout & text metrics
- [ ] Real paint backend (Skia/CPU raster)
- [ ] Charcoal1 binding layer (see `charcoal1` repo)
