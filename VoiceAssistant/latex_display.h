#pragma once
#include <string>
#include <cstring>

static bool latexGroup(const char* text,size_t length,size_t& at,std::string& group) {
  while(at<length && text[at]==' ')++at;
  if(at>=length || text[at]!='{')return false;
  const size_t start=++at;unsigned nesting=1;
  for(;at<length;++at) {
    if(text[at]=='\\' && at+1<length && strchr("{}",text[at+1])){++at;continue;}
    if(text[at]=='{')++nesting;
    if(text[at]=='}' && --nesting==0){group.assign(text+start,at-start);++at;return true;}
  }
  return false;
}

static bool latexDisplayToken(const char*,size_t,size_t&,std::string&,unsigned);
// Strip balanced grouping, retaining explicit literal braces and script groups.
// Incomplete groups remain visible until a later streaming fragment closes them.
static std::string latexDisplayContent(const std::string& text,unsigned depth) {
  if(depth>=8)return text;
  std::string out;
  for(size_t at=0;at<text.size();) {
    if(latexDisplayToken(text.c_str(),text.size(),at,out,depth+1))continue;
    const char ch=text[at];
    if(ch=='{' || ((ch=='^' || ch=='_') && at+1<text.size() && text[at+1]=='{')) {
      const bool script=ch!='{';size_t end=at+(script?1:0);std::string inner;
      if(latexGroup(text.c_str(),text.size(),end,inner)) {
        if(script){out+=ch;out+='{';}
        out+=latexDisplayContent(inner,depth+1);
        if(script)out+='}';
        at=end;continue;
      }
    }
    out+=text[at++];
  }
  return out;
}

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
    if(strchr("{}%_$",text[end])){out+=text[end];at=end+1;return true;}
    if(strchr(",;:! ",text[end])){out+=' ';at=end+1;return true;}
    if(text[end]=='\\'){out+='\n';at=end+1;return true;}
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
    {"longrightarrow","→"},{"longleftarrow","←"},{"longleftrightarrow","↔"},
    {"leftrightarrow","↔"},{"rightleftharpoons","⇌"},{"uparrow","↑"},{"downarrow","↓"},
    {"degree","°"},{"circ","°"},{"ell","ℓ"},{"angle","∠"},{"perp","⊥"},
    {"cdots","⋯"},{"ldots","…"},{"therefore","∴"},{"because","∵"},
    {"quad"," "},{"qquad","  "},{"limits",""},{"displaystyle",""},
    {"leftarrow","←"},{"Rightarrow","⇒"},{"Leftrightarrow","⇔"},
    {"sin","sin"},{"cos","cos"},{"tan","tan"},{"ln","ln"},{"log","log"},
    {"left",""},{"right",""}
    ,{"epsilon","ε"},{"varepsilon","ϵ"},{"zeta","ζ"},{"eta","η"},
    {"iota","ι"},{"kappa","κ"},{"nu","ν"},{"xi","ξ"},{"omicron","ο"},
    {"rho","ρ"},{"tau","τ"},{"upsilon","υ"},{"phi","φ"},{"varphi","ϕ"},
    {"chi","χ"},{"psi","ψ"},{"Gamma","Γ"},{"Theta","Θ"},{"Lambda","Λ"},
    {"Xi","Ξ"},{"Pi","Π"},{"Sigma","Σ"},{"Upsilon","Υ"},{"Phi","Φ"},{"Psi","Ψ"},
    {"vartheta","ϑ"},{"varrho","ϱ"},{"varpi","ϖ"},{"varsigma","ς"},
    {"div","÷"},{"ast","∗"},{"star","⋆"},{"bullet","•"},{"propto","∝"},
    {"sim","∼"},{"simeq","≃"},{"cong","≅"},{"ll","≪"},{"gg","≫"},
    {"neg","¬"},{"lnot","¬"},{"land","∧"},{"wedge","∧"},{"lor","∨"},{"vee","∨"},
    {"nexists","∄"},{"ni","∋"},{"owns","∋"},{"subsetneq","⊊"},{"supsetneq","⊋"},
    {"setminus","∖"},{"smallsetminus","∖"},{"varnothing","∅"},
    {"oint","∮"},{"iint","∬"},{"iiint","∭"},{"infin","∞"},
    {"parallel","∥"},{"nparallel","∦"},{"mid","∣"},{"triangle","△"},
    {"triangleq","≜"},{"sphericalangle","∢"},{"diameter","⌀"},
    {"gets","←"},{"mapsto","↦"},{"longmapsto","↦"},{"implies","⇒"},{"iff","⇔"},
    {"Leftarrow","⇐"},{"leftharpoons","⇋"},{"leftrightharpoons","⇋"},
    {"rightleftharpoons","⇌"},{"leftrightarrow","↔"},{"rightleftarrows","⇄"},
    {"langle","⟨"},{"rangle","⟩"},{"lceil","⌈"},{"rceil","⌉"},
    {"lfloor","⌊"},{"rfloor","⌋"},{"lbrace","{"},{"rbrace","}"},
    {"vert","|"},{"Vert","∥"},{"lvert","|"},{"rvert","|"},{"lVert","∥"},{"rVert","∥"},
    {"hbar","ℏ"},{"Re","ℜ"},{"Im","ℑ"},{"wp","℘"},{"aleph","ℵ"},
    {"vdots","⋮"},{"ddots","⋱"},{"dots","…"},{"prime","′"},{"doubleprime","″"},
    {"cot","cot"},{"sec","sec"},{"csc","csc"},{"arcsin","arcsin"},{"arccos","arccos"},
    {"arctan","arctan"},{"sinh","sinh"},{"cosh","cosh"},{"tanh","tanh"},
    {"lim","lim"},{"limsup","lim sup"},{"liminf","lim inf"},{"max","max"},{"min","min"},
    {"sup","sup"},{"inf","inf"},{"exp","exp"},{"det","det"},{"gcd","gcd"},
    {"bmod"," mod "},{"mod"," mod "},{"Pr","Pr"},
    {"bigl",""},{"bigr",""},{"Bigl",""},{"Bigr",""},{"big",""},{"Big",""},
    {"textstyle",""},{"scriptstyle",""},{"scriptscriptstyle",""},{"nolimits",""}
  };
  for (const auto& symbol : symbols) if (command == symbol.name) {
    out += symbol.value; at = end; return true;
  }
  if(command=="begin" || command=="end") {
    size_t next=end;std::string env;
    if(!latexGroup(text,length,next,env))return false;
    if(env!="aligned" && env!="align" && env!="equation" && env!="cases" && env!="matrix" && env!="bmatrix")return false;
    out+='\n';if(command=="begin" && env=="cases")out+="{ ";at=next;return true;
  }
  if(command=="frac" || command=="dfrac" || command=="tfrac") {
    size_t next=end;std::string a,b;
    if(!latexGroup(text,length,next,a) || !latexGroup(text,length,next,b))return false;
    auto normalize=[&](const std::string& input){return latexDisplayContent(input,depth+1);};
    a=normalize(a);b=normalize(b);
    // Compact fractions use one row; script-bearing fractions expand into full
    // height numerator/bar/denominator rows in the viewport. Nested/large fractions
    // use explicit parentheses instead of drawing outside the chat viewport.
    if(depth==0 && a.size()<=48 && b.size()<=48 && a.find('\x0e')==std::string::npos && b.find('\x0e')==std::string::npos)
      out+=std::string(1,'\x0e')+"{"+a+"}{"+b+"}";
    else out+="("+a+")/("+b+")";
    at=next;return true;
  }
  const bool wrapper = command=="boxed" || command=="fbox" || command=="mathrm" ||
      command=="mathbf" || command=="mathit" || command=="text" || command=="ce" || command=="pu" ||
      command=="operatorname" || command=="vec" || command=="bar" || command=="overline" || command=="hat" ||
      command=="mathbb" || command=="mathcal" || command=="mathsf" || command=="mathtt" || command=="boldsymbol" ||
      command=="textrm" || command=="textnormal" || command=="textbf" || command=="textit";
  const bool root = command=="sqrt";
  if (!wrapper && !root) return false;
  size_t open = end;
  while (open < length && text[open]==' ') ++open;
  std::string rootIndex;
  if(root && open<length && text[open]=='[') {
    const size_t start=++open;while(open<length && text[open]!=']')++open;
    if(open>=length || open-start>3)return false;
    rootIndex.assign(text+start,open-start);++open;
  }
  if (open >= length || text[open]!='{') return false;
  unsigned nesting = 1;
  size_t close = open+1;
  for (; close < length; ++close) {
    if (text[close]=='\\' && close+1<length && strchr("{}",text[close+1])) { ++close; continue; }
    if (text[close]=='{') ++nesting;
    if (text[close]=='}' && --nesting==0) break;
  }
  if (close >= length) { at = start; return false; }
  if (root) {
    if(rootIndex=="3")out+="³";else if(rootIndex=="4")out+="⁴";else if(!rootIndex.empty())out+="("+rootIndex+")";
    out += "√(";
  }
  std::string inner=latexDisplayContent(std::string(text+open+1,close-open-1),depth+1);
  if(command=="mathbb") {
    if(inner=="R")inner="ℝ";else if(inner=="N")inner="ℕ";else if(inner=="Z")inner="ℤ";
    else if(inner=="Q")inner="ℚ";else if(inner=="C")inner="ℂ";
  }
  if(command=="ce") {
    std::string chemistry;
    for(size_t i=0;i<inner.size();) {
      if(inner.compare(i,3,"<=>")==0){chemistry+="⇌";i+=3;continue;}
      if(inner.compare(i,2,"->")==0){chemistry+="→";i+=2;continue;}
      const char c=inner[i];
      if(c>='0' && c<='9' && i && ((inner[i-1]>='a'&&inner[i-1]<='z') || (inner[i-1]>='A'&&inner[i-1]<='Z') || inner[i-1]==')')) {
        size_t j=i;while(j<inner.size()&&inner[j]>='0'&&inner[j]<='9')++j;
        chemistry+="_{"+inner.substr(i,j-i)+"}";i=j;continue;
      }
      chemistry+=c;++i;
    }
    inner=std::move(chemistry);
  }
  out+=inner;
  if(command=="vec")out+="⃗";
  if(command=="bar" || command=="overline")out+="̄";
  if(command=="hat")out+="̂";
  if (root) out += ")";
  at = close+1;
  return true;
}

// Fenced prose/math is a common model fallback. Remove the language label,
// retaining literal code for programming languages and non-equation text.
static bool camFencedMathLine(const std::string& line) {
  if(line.find_first_of(";\"`")!=std::string::npos || line.find("://")!=std::string::npos)return false;
  const bool notation=line.find_first_of("_^=()")!=std::string::npos || line.find("\\frac")!=std::string::npos;
  return notation && (line.find('=')!=std::string::npos || line.find("\\")!=std::string::npos || line.find("f(")!=std::string::npos);
}
static bool camFractionHasScripts(const std::string& text) {
  if(text.find_first_of("_^")!=std::string::npos)return true;
  const char* scripts[]={"₀","₁","₂","₃","₄","₅","₆","₇","₈","₉","₊","₋","⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹","⁺","⁻"};
  for(const auto* script:scripts)if(text.find(script)!=std::string::npos)return true;
  return false;
}
// Convert only explicit, balanced (numerator)/(denominator) in fenced math.
// Never guess the precedence of an ungrouped a+b/c or touch program code.
static std::string camDisplayPlainFraction(const std::string& line) {
  const size_t start=line.find_first_not_of(" \t");
  if(start==std::string::npos || (line[start]!='[' && line[start]!='('))return line;
  auto closeGroup=[&](size_t at)->size_t {
    const char open=line[at],close=open=='('?')':']';unsigned depth=1;
    for(size_t i=at+1;i<line.size();++i) {
      if(line[i]==open)++depth;
      if(line[i]==close && --depth==0)return i;
    }
    return std::string::npos;
  };
  const size_t topEnd=closeGroup(start);if(topEnd==std::string::npos)return line;
  size_t slash=line.find_first_not_of(" \t",topEnd+1);if(slash==std::string::npos || line[slash]!='/')return line;
  const size_t bottom=line.find_first_not_of(" \t",slash+1);
  if(bottom==std::string::npos || line[bottom]!='(')return line;
  const size_t bottomEnd=closeGroup(bottom);if(bottomEnd==std::string::npos)return line;
  const size_t suffix=line.find_first_not_of(" \t\r",bottomEnd+1);
  if(suffix!=std::string::npos && line[suffix]!='=')return line;
  return "\\frac{"+line.substr(start+1,topEnd-start-1)+"}{"+line.substr(bottom+1,bottomEnd-bottom-1)+"}"+line.substr(bottomEnd+1);
}
template<typename Output> static Output camDisplayCodeFences(const char* input,size_t length) {
  Output out;out.reserve(length);bool fenced=false,math=false,plain=false;char marker=0;size_t fenceLength=0;
  for(size_t at=0;at<length;) {
    size_t end=at;while(end<length && input[end]!='\n')++end;
    const std::string line(input+at,end-at);size_t first=line.find_first_not_of(" \t\r");
    if(first==std::string::npos)first=line.size();
    size_t run=first;while(run<line.size() && (line[run]=='`' || line[run]=='~') && line[run]==line[first])++run;
    const bool delimiter=run-first>=3 && first<=3;
    if(delimiter && (!fenced || (line[first]==marker && run-first>=fenceLength && line.find_first_not_of(" \t\r",run)==std::string::npos))) {
      if(!fenced) {
        fenced=true;marker=line[first];fenceLength=run-first;
        std::string label=line.substr(run);size_t last=label.find_last_not_of(" \t\r");if(last==std::string::npos)label.clear();else label.resize(last+1);
        size_t start=label.find_first_not_of(" \t");if(start!=std::string::npos)label.erase(0,start);
        math=label=="math" || label=="latex" || label=="tex";
        plain=label.empty() || label=="text" || label=="plaintext";
      } else fenced=false;
    }else {
      const bool literal=fenced && !math && !(plain && camFencedMathLine(line));
      if(literal)out+='\x04';const std::string body=fenced && !literal?camDisplayPlainFraction(line):line;out+=body.c_str();if(literal)out+='\x04';
      if(end<length)out+='\n';
    }
    at=end<length?end+1:end;
  }
  return out;
}
