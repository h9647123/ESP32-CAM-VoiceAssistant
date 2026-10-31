#pragma once
#include <string>
#include <cstring>

// A small display adapter, not a symbolic mathematics engine. Unknown commands
// and unbalanced groups are retained. The rich-text caller excludes code spans.
static bool latexDisplayToken(const char* text, size_t length, size_t& at,
    std::string& out, unsigned depth = 0) {
  if (depth >= 8 || at >= length || text[at] != '\\' || at+1 >= length) return false;
  const size_t start = at;
  size_t end = at+1;
  while (end < length && ((text[end] >= 'a' && text[end] <= 'z') ||
      (text[end] >= 'A' && text[end] <= 'Z'))) ++end;
  if (end == at+1) {
    if (strchr("()[]", text[end])) { at = end+1; return true; }
    return false;
  }
  const std::string command(text+at+1, end-at-1);
  struct Symbol { const char* name; const char* value; };
  static const Symbol symbols[] = {
    {"infty","∞"},{"in","∈"},{"notin","∉"},{"le","≤"},{"leq","≤"},
    {"ge","≥"},{"geq","≥"},{"ne","≠"},{"neq","≠"},{"times","×"},
    {"cdot","·"},{"pm","±"},{"mp","∓"},{"approx","≈"},{"equiv","≡"},
    {"subset","⊂"},{"subseteq","⊆"},{"supset","⊃"},{"supseteq","⊇"},
    {"cup","∪"},{"cap","∩"},{"emptyset","∅"},{"forall","∀"},{"exists","∃"},
    {"sum","∑"},{"prod","∏"},{"int","∫"},{"partial","∂"},{"nabla","∇"},
    {"alpha","α"},{"beta","β"},{"gamma","γ"},{"delta","δ"},{"Delta","Δ"},
    {"theta","θ"},{"lambda","λ"},{"mu","μ"},{"pi","π"},{"sigma","σ"},
    {"omega","ω"},{"Omega","Ω"},{"rightarrow","→"},{"to","→"},
    {"leftarrow","←"},{"Rightarrow","⇒"},{"Leftrightarrow","⇔"},
    {"sin","sin"},{"cos","cos"},{"tan","tan"},{"ln","ln"},{"log","log"},
    {"left",""},{"right",""}
  };
  for (const auto& symbol : symbols) if (command == symbol.name) {
    out += symbol.value; at = end; return true;
  }
  const bool wrapper = command=="boxed" || command=="fbox" || command=="mathrm" ||
      command=="mathbf" || command=="mathit" || command=="text";
  const bool root = command=="sqrt";
  if (!wrapper && !root) return false;
  size_t open = end;
  while (open < length && text[open]==' ') ++open;
  if (open >= length || text[open]!='{') return false;
  unsigned nesting = 1;
  size_t close = open+1;
  for (; close < length; ++close) {
    if (text[close]=='\\' && close+1<length && strchr("{}",text[close+1])) { ++close; continue; }
    if (text[close]=='{') ++nesting;
    if (text[close]=='}' && --nesting==0) break;
  }
  if (close >= length) { at = start; return false; }
  if (root) out += "√(";
  for (size_t i = open+1; i < close;) {
    if (!latexDisplayToken(text, close, i, out, depth+1)) out += text[i++];
  }
  if (root) out += ")";
  at = close+1;
  return true;
}
