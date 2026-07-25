// loophole — the compiler for the wish and genie languages
//
// It reads two languages. A `.wish` is what the player asks for: a world, and
// the wishes made in it. A `.genie` is the policy the wish is judged against:
// what the genie refuses, and what it believes it is holding. Both are data;
// the machine between them is fixed.
//
// Pipeline:  source (.wish) -> lexer -> parser -> AST
//                           -> R2 scans the SURFACE text (before expansion)
//                           -> definitions are expanded
//                           -> R1 checks the RESOLVED program (after expansion)
//                           -> execute under fixed-width integer semantics
//                           -> invariants, checked twice: as the genie wrote
//                              them, and against grounded reality
//
// A wish wins if it is LEGAL and either
//   VIOLATED  the genie's own check failed — you broke the rule to its face
//   FOOLED    the genie's check passed but reality disagrees — you got it to
//             sign off on something that isn't true
//
// The second one is the whole point of the aliasing axis: nothing is violated,
// the genie is satisfied, and a person is still dead.
//
// ---------------------------------------------------------------------------
// Shape: the front half is a real compiler front-end (lexer, recursive descent
// parser, AST, semantic passes). The back half is NOT a compiler — no IR, no
// codegen, no target. It is a tree-walking interpreter plus a specification
// checker. So this program grows along TABLES of semantics, not a pass pipeline.
//
//   OPS         what operations exist and what each one does
//   the genie   what it refuses and what it believes it is holding — DATA,
//               loaded from a .genie policy (a default one is embedded)
//
// Adding an operation or a rule is one entry, not five edits. That matters most
// for the hunter: its alphabet is DERIVED from OPS, so a new operation gets
// explored automatically. A hand-written alphabet that someone forgot to update
// would make `--hunt` quietly stop searching an axis and report "no new
// shapes" — which would be read as a finding. That is the most expensive bug
// this program could have.
// ---------------------------------------------------------------------------
//
// Build:  g++ -std=c++17 -O2 -Wall loophole.cpp -o loophole
// Run:    ./loophole examples/01_humble.wish
//         ./loophole --genie genie/mortal.genie examples/08_eternal_sleep.wish
//         ./loophole --hunt examples/01_humble.wish

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <cstdio>
#include <unistd.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Version. Loophole the compiler and the two languages it reads are versioned
// separately on purpose: a bug fix bumps the compiler, a new concept bumps
// whichever language grew it. Dependants (an editor plugin, a judge) pin
// against these.
// ---------------------------------------------------------------------------
static const char* COMPILER_VERSION = "1.8.2";
static const char* WISH_VERSION     = "1.0";
static const char* GENIE_VERSION    = "1.0";

// Minimal JSON string escaping. UTF-8 passes through untouched — JSON is
// defined over Unicode, and the diagnostics are bilingual.
static std::string jsonEsc(const std::string& in) {
    std::string o;
    for (char c : in) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    o += buf;
                } else o += c;
        }
    }
    return o;
}

// ---------------------------------------------------------------------------
// DIAGNOSTICS.
//
// Nobody knows this language. Every error is therefore somebody's first
// encounter with a rule they did not know existed — so a diagnostic that only
// reports the failure has wasted the one moment when the reader is paying
// attention. Each one carries a `help` line that states the rule.
//
// The source is held in a file-scope slot rather than threaded through the
// lexer and both parsers. This is a single-file, single-threaded compiler that
// reads at most two files, and passing a context through three constructors to
// reach one error path would cost more than it explains.
// ---------------------------------------------------------------------------
struct Source { std::string name, text; };
static Source g_src;

// A diagnostic ends the run. It is thrown rather than exited, for two reasons
// that both outlive the command line: the stack unwinds properly, and a host
// that judges more than once in a process can catch it and carry on. The
// browser build is exactly that host — `exit()` there would tear down the whole
// wasm runtime, so the page would work once and then be dead.
struct Fatal { int code; };

// Colour only when a human is looking. Escape codes in a pipe would end up in
// the goldens, in `grep`, and in anything that treats the report as text — so
// the check is on the actual stream, not on a flag.
//
// A host may override it. The browser has no terminal for `isatty` to be right
// or wrong about, and what it answers there is an implementation detail of the
// runtime — so the page states what it wants instead of letting the answer
// depend on how the module happened to be loaded.
static int g_colour = -1;                    // -1 decide; 0 never; 1 always
static bool colourOK() {
    if (g_colour >= 0) return g_colour != 0;
    static const bool on = !std::getenv("NO_COLOR") && isatty(fileno(stderr));
    return on;
}
static const char* C_RED   () { return colourOK() ? "\033[1;31m" : ""; }
static const char* C_CYAN  () { return colourOK() ? "\033[1;36m" : ""; }
static const char* C_GREEN () { return colourOK() ? "\033[1;32m" : ""; }
static const char* C_BOLD  () { return colourOK() ? "\033[1m"    : ""; }
static const char* C_OFF   () { return colourOK() ? "\033[0m"    : ""; }

static std::string sourceLine(int line) {
    std::istringstream in(g_src.text);
    std::string s;
    for (int n = 1; std::getline(in, s); n++) if (n == line) return s;
    return "";
}

// rustc's shape: say what is wrong, point at exactly where, then say the rule.
// The caret column is counted in bytes, so a line with multi-byte characters
// before the error would point slightly off — which cannot happen in practice,
// since every construct in both languages is ASCII (§3) and a non-ASCII byte is
// itself the error being reported.
[[noreturn]] static void fail(const std::string& msg, int line, int col,
                              const std::string& help,
                              const std::string& note =
                                  "no wish was judged. the genie cannot grant "
                                  "what it cannot read.") {
    std::string src = sourceLine(line);
    std::string num = std::to_string(line);
    std::string pad(num.size(), ' ');

    std::cerr << C_RED() << "error" << C_OFF() << ": " << C_BOLD() << msg << C_OFF() << "\n";
    std::cerr << pad << C_CYAN() << "--> " << C_OFF()
              << (g_src.name.empty() ? "<input>" : g_src.name)
              << ":" << line << ":" << col << "\n";
    if (!src.empty()) {
        std::cerr << pad << C_CYAN() << " |" << C_OFF() << "\n";
        std::cerr << C_CYAN() << num << " |" << C_OFF() << " " << src << "\n";
        std::cerr << pad << C_CYAN() << " |" << C_OFF() << " "
                  << std::string(col > 0 ? col - 1 : 0, ' ')
                  << C_RED() << "^" << C_OFF() << "\n";
    }
    if (!help.empty()) {
        std::cerr << pad << C_CYAN() << " |" << C_OFF() << "\n";
        std::cerr << C_GREEN() << "help" << C_OFF() << ": " << help << "\n";
    }
    // The one line of the machine's output that is allowed to be in-world. It
    // is also literally true, and it says the thing a reader most needs to
    // know: nothing was judged, so no verdict below means anything.
    if (!note.empty()) std::cerr << "\n" << note << "\n";
    // 2, never 1. Per §10.1 exit 1 means "judged, and something was an
    // exploit" — reporting a syntax error as 1 would tell a script that a file
    // which does not even parse had found a hole in the genie.
    throw Fatal{2};
}

// Edit distance, for "did you mean". The candidates are always derived from a
// table (the operations, the names in scope), never hand-listed, so adding an
// operation improves the suggestion without anyone remembering to.
static size_t editDistance(const std::string& a, const std::string& b) {
    std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++) prev[j] = j;
    for (size_t i = 1; i <= a.size(); i++) {
        cur[0] = i;
        for (size_t j = 1; j <= b.size(); j++) {
            size_t sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
            cur[j] = std::min({ sub, prev[j] + 1, cur[j - 1] + 1 });
        }
        prev = cur;
    }
    return prev[b.size()];
}

// The closest candidate, if one is close enough to be worth suggesting. A
// threshold of a third of the word keeps "sbu" -> "sub" but refuses to propose
// "add" for "widen".
static std::string didYouMean(const std::string& got,
                              const std::vector<std::string>& cands) {
    // A prefix beats distance outright. The operations have abbreviated names,
    // so the commonest mistake is writing the whole word — `subtract` for `sub`
    // is five edits away and would never clear a distance threshold, yet it is
    // obviously what was meant.
    for (const auto& c : cands)
        if (c.size() >= 3 && got.compare(0, c.size(), c) == 0) return c;

    std::string best; size_t bestd = SIZE_MAX;
    for (const auto& c : cands) {
        size_t d = editDistance(got, c);
        if (d < bestd) { bestd = d; best = c; }
    }
    size_t limit = std::max<size_t>(1, got.size() / 3 + 1);
    return bestd <= limit ? best : "";
}

// ---------------------------------------------------------------------------
// Fixed-width unsigned value. This is where the first joke lives: subtraction
// on a w-bit register is arithmetic mod 2^w, not the "value" the genie imagines.
// ---------------------------------------------------------------------------
struct Reg {
    uint64_t val = 0;
    int width = 64;  // bits, 1..64

    static uint64_t mask_for(int w) {
        return (w >= 64) ? ~0ULL : ((1ULL << w) - 1);
    }
    uint64_t mask() const { return mask_for(width); }
    void normalize() { val &= mask(); }

    void sub(uint64_t imm) { val = (val - imm) & mask(); }  // wraps
    void add(uint64_t imm) { val = (val + imm) & mask(); }  // wraps
    void widen(int w) { width = w; normalize(); }           // value preserved
};

// A definition. Definitions are rebindable — that is a property of the
// language, not a favour the genie granted. It is also how the genie's own
// convenience turns into its undoing.
struct Binding {
    enum class Kind { Name, Set };
    Kind kind = Kind::Name;
    std::string name;                   // Kind::Name — an op, a person, or another define
    std::vector<std::string> members;   // Kind::Set
};

// ---------------------------------------------------------------------------
// Propositional formulas — the second engine.
//
// Everything above this line is "run it and look at the state". A promise is a
// different kind of question: not "what happened" but "can the genie's word be
// kept at all". That is satisfiability over a finite set of formulas, and it
// needs its own machinery.
//
// Atoms:
//   granted(w)  a free variable, one per wish. The genie decides these.
//   alive(p)    NOT a variable — a constant, read off the grounded world. The
//               imperative engine settles it before this engine ever runs.
// ---------------------------------------------------------------------------
struct Fml {
    enum class K { Const, Granted, Alive, Not, And, Or, Implies };
    K k = K::Const;
    bool value = false;            // Const
    std::string name;              // Granted (a wish), Alive (a person)
    std::vector<Fml> kids;         // Not: 1;  And / Or / Implies: 2
};

static Fml fmlConst(bool v)                { Fml f; f.k = Fml::K::Const; f.value = v; return f; }
static Fml fmlAtom(Fml::K k, std::string n){ Fml f; f.k = k; f.name = std::move(n); return f; }
static Fml fmlNot(Fml a)                   { Fml f; f.k = Fml::K::Not; f.kids.push_back(std::move(a)); return f; }
static Fml fmlBin(Fml::K k, Fml a, Fml b)  {
    Fml f; f.k = k; f.kids.push_back(std::move(a)); f.kids.push_back(std::move(b)); return f;
}

static std::string fmlText(const Fml& f) {
    switch (f.k) {
        case Fml::K::Const:   return f.value ? "true" : "false";
        case Fml::K::Granted: return "granted(" + f.name + ")";
        case Fml::K::Alive:   return "alive(" + f.name + ")";
        case Fml::K::Not:     return "not " + fmlText(f.kids[0]);
        case Fml::K::And:     return "(" + fmlText(f.kids[0]) + " and " + fmlText(f.kids[1]) + ")";
        case Fml::K::Or:      return "(" + fmlText(f.kids[0]) + " or " + fmlText(f.kids[1]) + ")";
        case Fml::K::Implies: return "(" + fmlText(f.kids[0]) + " implies " + fmlText(f.kids[1]) + ")";
    }
    return "?";
}

// A thing the genie has bound itself to. `source` is the wish that created it,
// kept so the report can say where a contradiction came from.
struct Commitment {
    std::string axiom;     // "A1" or "A2"
    std::string source;    // wish name
    Fml f;
};

// A person is a bundle of named numeric attributes (§4.2 of the spec). The old
// single `alive` bit is gone; "alive" is now whatever a genie says it is, and
// the built-in reading (`personAlive`) is just "some vital is still nonzero".
struct AttrSchema { std::string name; int width = 1; uint64_t deflt = 0; };

struct World {
    std::map<std::string, Reg> regs;
    std::vector<std::string> people;                  // declared, immutable
    std::vector<AttrSchema> attrs;                    // the attribute schema, in order
    std::map<std::string, std::map<std::string, uint64_t>> attr;  // person -> attr -> value
    std::map<std::string, Binding> defs;              // rebindable
    std::vector<Commitment> commitments;              // the genie's word, once given
};

static const AttrSchema* attrSchema(const World& w, const std::string& name) {
    for (const auto& a : w.attrs) if (a.name == name) return &a;
    return nullptr;
}
static bool isPerson(const World& w, const std::string& name) {
    for (const auto& p : w.people) if (p == name) return true;
    return false;
}
static uint64_t attrValue(const World& w, const std::string& p, const std::string& a) {
    auto it = w.attr.find(p);
    if (it == w.attr.end()) return 0;
    auto jt = it->second.find(a);
    return jt == it->second.end() ? 0 : jt->second;
}
// A person is "alive" in the built-in sense iff any vital is still nonzero.
static bool personAlive(const World& w, const std::string& p) {
    auto it = w.attr.find(p);
    if (it == w.attr.end()) return false;
    for (const auto& kv : it->second) if (kv.second > 0) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Is the genie's word keepable?
//
// DPLL, hand-rolled, working directly on the formulas — no CNF conversion,
// because with a handful of wishes there is nothing to gain from it and a
// Tseitin transform would put variables in the answer that the player never
// wrote. Simplify under the partial assignment, propagate whatever that forces,
// split on what is left.
//
// It is exponential in the number of wishes in the worst case. The number of
// wishes in a .wish file is small, and this is the honest bound, not a bug.
// ---------------------------------------------------------------------------
using Assign = std::map<std::string, bool>;

static Fml simplify(const Fml& f, const Assign& a, const World& w) {
    switch (f.k) {
        case Fml::K::Const: return f;
        case Fml::K::Granted: {
            auto it = a.find(f.name);
            return it == a.end() ? f : fmlConst(it->second);
        }
        case Fml::K::Alive: {
            // Not a variable. The world already decided this one.
            return fmlConst(personAlive(w, f.name));
        }
        case Fml::K::Not: {
            Fml x = simplify(f.kids[0], a, w);
            if (x.k == Fml::K::Const) return fmlConst(!x.value);
            return fmlNot(std::move(x));
        }
        case Fml::K::And: {
            Fml x = simplify(f.kids[0], a, w), y = simplify(f.kids[1], a, w);
            if (x.k == Fml::K::Const) return x.value ? y : fmlConst(false);
            if (y.k == Fml::K::Const) return y.value ? x : fmlConst(false);
            return fmlBin(Fml::K::And, std::move(x), std::move(y));
        }
        case Fml::K::Or: {
            Fml x = simplify(f.kids[0], a, w), y = simplify(f.kids[1], a, w);
            if (x.k == Fml::K::Const) return x.value ? fmlConst(true) : y;
            if (y.k == Fml::K::Const) return y.value ? fmlConst(true) : x;
            return fmlBin(Fml::K::Or, std::move(x), std::move(y));
        }
        case Fml::K::Implies: {
            Fml x = simplify(f.kids[0], a, w), y = simplify(f.kids[1], a, w);
            if (x.k == Fml::K::Const) return x.value ? y : fmlConst(true);
            if (y.k == Fml::K::Const && y.value) return fmlConst(true);
            return fmlBin(Fml::K::Implies, std::move(x), std::move(y));
        }
    }
    return f;
}

static void collectVars(const Fml& f, std::set<std::string>& out) {
    if (f.k == Fml::K::Granted) out.insert(f.name);
    for (const auto& k : f.kids) collectVars(k, out);
}

// True if every formula can be satisfied at once.
static bool satisfiable(const std::vector<Fml>& fs, Assign a, const World& w) {
    std::vector<Fml> cur;
    cur.reserve(fs.size());

    for (bool moved = true; moved; ) {
        moved = false;
        cur.clear();
        for (const auto& f : fs) {
            Fml s = simplify(f, a, w);
            if (s.k == Fml::K::Const) {
                if (!s.value) return false;    // this branch is dead
                continue;
            }
            cur.push_back(std::move(s));
        }
        // Unit propagation: a formula that has shrunk to a bare literal is not a
        // choice, it is a consequence.
        for (const auto& s : cur) {
            if (s.k == Fml::K::Granted && !a.count(s.name)) {
                a[s.name] = true; moved = true; break;
            }
            if (s.k == Fml::K::Not && s.kids[0].k == Fml::K::Granted &&
                !a.count(s.kids[0].name)) {
                a[s.kids[0].name] = false; moved = true; break;
            }
        }
    }
    if (cur.empty()) return true;              // everything satisfied

    std::set<std::string> vars;
    for (const auto& s : cur) collectVars(s, vars);
    for (const auto& v : vars) {
        if (a.count(v)) continue;
        Assign t = a; t[v] = true;
        if (satisfiable(fs, t, w)) return true;
        t[v] = false;
        return satisfiable(fs, t, w);
    }
    return false;   // nothing left to try and something is still undecided
}

static bool axiomsConsistent(const World& w) {
    std::vector<Fml> fs;
    fs.reserve(w.commitments.size());
    for (const auto& c : w.commitments) fs.push_back(c.f);
    return satisfiable(fs, Assign{}, w);
}

// ---------------------------------------------------------------------------
// Lexer
//
// Operation keywords are NOT lexed as keywords — they come out as Ident and get
// resolved later, against OPS and against whatever the wish has defined. That
// deferral is exactly what makes aliasing possible.
// ---------------------------------------------------------------------------
enum class Tok {
    Ident, Int,
    KwRegister, KwUint, KwWish, KwPeople, KwDefine, KwPromise, KwAttribute,
    Str,
    Colon, ColonEq, Comma, Dot, Lt, Gt, Eq, LBrace, RBrace, LParen, RParen, Arrow,
    Le, Ge, EqEq, Ne, Plus, Minus,
    End
};

struct Token {
    Tok kind;
    std::string text;
    uint64_t num = 0;
    int line = 1;
    int col  = 1;      // 1-based, in bytes; §3 makes every construct ASCII
};

struct Lexer {
    std::string s;
    size_t i = 0;
    int line = 1;
    size_t bol = 0;              // offset of the current line's first byte

    explicit Lexer(std::string src) : s(std::move(src)) {}

    int col() const { return (int)(i - bol) + 1; }

    [[noreturn]] void die(const std::string& msg, const std::string& help = "") {
        fail(msg, line, col(), help);
    }

    std::vector<Token> run() {
        std::vector<Token> out;
        while (i < s.size()) {
            char c = s[i];
            if (c == '\n') { line++; i++; bol = i; continue; }
            if (std::isspace((unsigned char)c)) { i++; continue; }
            if (c == '#') { while (i < s.size() && s[i] != '\n') i++; continue; }

            if (c == '-' && i + 1 < s.size() && s[i + 1] == '>') {
                out.push_back({Tok::Arrow, "->", 0, line, col()}); i += 2; continue;
            }
            if (c == ':' && i + 1 < s.size() && s[i + 1] == '=') {
                out.push_back({Tok::ColonEq, ":=", 0, line, col()}); i += 2; continue;
            }
            if (c == '<' && i + 1 < s.size() && s[i + 1] == '=') {
                out.push_back({Tok::Le, "<=", 0, line, col()}); i += 2; continue;
            }
            if (c == '>' && i + 1 < s.size() && s[i + 1] == '=') {
                out.push_back({Tok::Ge, ">=", 0, line, col()}); i += 2; continue;
            }
            if (c == '=' && i + 1 < s.size() && s[i + 1] == '=') {
                out.push_back({Tok::EqEq, "==", 0, line, col()}); i += 2; continue;
            }
            if (c == '!' && i + 1 < s.size() && s[i + 1] == '=') {
                out.push_back({Tok::Ne, "!=", 0, line, col()}); i += 2; continue;
            }
            if (c == '-') { out.push_back({Tok::Minus, "-", 0, line, col()}); i++; continue; }
            if (c == '+') { out.push_back({Tok::Plus, "+", 0, line, col()}); i++; continue; }
            if (c == '"') {
                int c0 = col();
                std::string t; i++;
                while (i < s.size() && s[i] != '"') {
                    if (s[i] == '\n') die("unterminated string");
                    t += s[i]; i++;
                }
                if (i >= s.size()) die("unterminated string");
                i++;
                out.push_back({Tok::Str, t, 0, line, c0}); continue;
            }
            switch (c) {
                case ':': out.push_back({Tok::Colon,  ":", 0, line, col()}); i++; continue;
                case ',': out.push_back({Tok::Comma,  ",", 0, line, col()}); i++; continue;
                case '.': out.push_back({Tok::Dot,    ".", 0, line, col()}); i++; continue;
                case '<': out.push_back({Tok::Lt,     "<", 0, line, col()}); i++; continue;
                case '>': out.push_back({Tok::Gt,     ">", 0, line, col()}); i++; continue;
                case '=': out.push_back({Tok::Eq,     "=", 0, line, col()}); i++; continue;
                case '{': out.push_back({Tok::LBrace, "{", 0, line, col()}); i++; continue;
                case '}': out.push_back({Tok::RBrace, "}", 0, line, col()}); i++; continue;
                case '(': out.push_back({Tok::LParen, "(", 0, line, col()}); i++; continue;
                case ')': out.push_back({Tok::RParen, ")", 0, line, col()}); i++; continue;
            }
            if (std::isdigit((unsigned char)c)) {
                int c0 = col();
                uint64_t n = 0; std::string t;
                while (i < s.size() && std::isdigit((unsigned char)s[i])) {
                    n = n * 10 + (s[i] - '0'); t += s[i]; i++;
                }
                out.push_back({Tok::Int, t, n, line, c0}); continue;
            }
            if (std::isalpha((unsigned char)c) || c == '_') {
                int c0 = col();
                std::string t;
                while (i < s.size() && (std::isalnum((unsigned char)s[i]) || s[i] == '_')) {
                    t += s[i]; i++;
                }
                Tok k = Tok::Ident;
                if      (t == "register") k = Tok::KwRegister;
                else if (t == "uint")     k = Tok::KwUint;
                else if (t == "wish")     k = Tok::KwWish;
                else if (t == "people")   k = Tok::KwPeople;
                else if (t == "define")   k = Tok::KwDefine;
                else if (t == "promise")  k = Tok::KwPromise;
                else if (t == "attribute") k = Tok::KwAttribute;
                out.push_back({k, t, 0, line, c0}); continue;
            }
            // Name the rule, not just the byte. Each of these is a decision
            // somebody made about the language, and hitting it is the moment
            // that decision becomes worth explaining.
            if (c == ';')
                die("unexpected character ';'",
                    "statements are not terminated in Loophole -- one ends where "
                    "its line does. Delete the ';'.");
            if ((unsigned char)c >= 0x80)
                die("this file is not ASCII",
                    "all concrete syntax is ASCII (spec §3): write `all` for a "
                    "universal, `<=` for at most, `not`/`and`/`or` for logic. "
                    "Mathematical symbols are deliberately not accepted -- they "
                    "are awkward to type.");
            if (c == '/' || c == '*')
                die(std::string("unexpected character '") + c + "'",
                    "comments begin with '#' and run to the end of the line.");
            if (c == '[' || c == ']')
                die(std::string("unexpected character '") + c + "'",
                    "a set is written with braces: `define everyone := { alice }`.");
            die(std::string("unexpected character '") + c + "'",
                "both languages are ASCII, and every construct is listed in "
                "spec §3.");
        }
        out.push_back({Tok::End, "", 0, line, col()});
        return out;
    }
};

// ---------------------------------------------------------------------------
// AST
//
// A statement records the name it was WRITTEN with, not the operation it means.
// The two can differ once definitions exist, and every interesting thing in
// Phase 2 happens in that gap.
// ---------------------------------------------------------------------------
struct Decl { std::string name; int width; uint64_t init; };

enum class StmtKind { Op, Define, Promise };

// How the operands were written. Determined by syntax alone, then checked
// against the resolved operation.
enum class Shape { Bare, WithImm, WithWidth };

struct Stmt {
    StmtKind kind = StmtKind::Op;
    int line = 1;
    int col  = 1;      // of the verb, so a diagnostic can point at it

    // StmtKind::Op
    std::string verb;      // as written — may be an alias
    Shape shape = Shape::Bare;
    std::string arg;       // register or person, as written
    std::string attr;      // the part after '.', for `set p.attr, v` (else empty)
    uint64_t imm = 0;
    int width = 0;

    // StmtKind::Define
    std::string defname;
    bool target_is_set = false;
    std::string target_name;
    std::vector<std::string> target_members;

    // StmtKind::Promise
    Fml fml;
};

struct Wish { std::string name; std::vector<Stmt> body; };
struct Program {
    std::vector<Decl> decls;
    std::vector<AttrSchema> attrs;
    std::vector<std::string> people;
    std::vector<Wish> wishes;
};

// ---------------------------------------------------------------------------
// TABLE 1 — the operations.
// ---------------------------------------------------------------------------
enum class OperandKind { RegImm, RegWidth, Person, AttrImm };

// The suffix an operand kind expects. Whether the arg carries a `.attr` is a
// separate check (only AttrImm wants one).
static Shape shapeFor(OperandKind k) {
    switch (k) {
        case OperandKind::RegImm:   return Shape::WithImm;
        case OperandKind::AttrImm:  return Shape::WithImm;
        case OperandKind::RegWidth: return Shape::WithWidth;
        case OperandKind::Person:   return Shape::Bare;
    }
    return Shape::Bare;
}
static bool wantsAttr(OperandKind k) { return k == OperandKind::AttrImm; }

// A resolved statement: the operation is known, the argument is a real name.
struct Resolved {
    bool is_define = false;
    bool is_promise = false;
    int line = 1;
    // Which statement of the wish body this came from. Resolution happens to be
    // one-to-one today, and `expandProgram` used to just assume that by
    // indexing both with the same counter. Carrying the index means the day
    // somebody makes one statement resolve into two, expansion stays correct
    // instead of silently pairing up the wrong lines.
    size_t src = 0;

    Fml fml;                     // is_promise, with `self` already resolved

    int op = -1;                 // index into OPS
    std::string arg;             // resolved register or person
    std::string attr;            // attribute, for `set` (else empty)
    uint64_t imm = 0;
    int width = 0;

    std::string defname;         // is_define
    Binding binding;
};

struct OpDef {
    const char* keyword;
    OperandKind operands;
    // Mutate the world. If `trace` is non-null, write the human-readable line.
    // Returns whether the world actually changed — the hunter uses that to tell
    // a real move from a statement that merely occupies a line.
    bool (*exec)(World&, const Resolved&, std::string* trace);
};

static bool exec_sub(World& w, const Resolved& r, std::string* trace) {
    Reg& R = w.regs[r.arg];
    uint64_t pre = R.val;
    R.sub(r.imm);
    if (trace) {
        std::ostringstream o;
        o << "sub    " << r.arg << ", " << r.imm
          << "   (" << pre << " - " << r.imm << " on uint<" << R.width
          << "> = " << R.val << ")";
        *trace = o.str();
    }
    return R.val != pre;
}

static bool exec_add(World& w, const Resolved& r, std::string* trace) {
    Reg& R = w.regs[r.arg];
    uint64_t pre = R.val;
    R.add(r.imm);
    if (trace) {
        std::ostringstream o;
        o << "add    " << r.arg << ", " << r.imm << "   -> " << R.val;
        *trace = o.str();
    }
    return R.val != pre;
}

// Named `widen`, but nothing checks that the new width is larger. Narrowing
// truncates. That asymmetry is a real hole the hunter found; see SEMANTICS.md.
static bool exec_widen(World& w, const Resolved& r, std::string* trace) {
    Reg& R = w.regs[r.arg];
    uint64_t pre = R.val; int prew = R.width;
    R.widen(r.width);
    if (trace) {
        std::ostringstream o;
        o << "widen  " << r.arg << " -> uint<" << r.width
          << ">   (value preserved: " << R.val << ")";
        *trace = o.str();
    }
    return R.val != pre || R.width != prew;
}

// Set one attribute of a person, truncated to that attribute's width.
static bool exec_set(World& w, const Resolved& r, std::string* trace) {
    const AttrSchema* sc = attrSchema(w, r.attr);
    int width = sc ? sc->width : 64;
    uint64_t v = r.imm & Reg::mask_for(width);
    uint64_t pre = attrValue(w, r.arg, r.attr);
    w.attr[r.arg][r.attr] = v;
    if (trace) {
        std::ostringstream o;
        o << "set    " << r.arg << "." << r.attr << ", " << r.imm
          << "   (" << pre << " -> " << v << ")";
        *trace = o.str();
    }
    return v != pre;
}

// kill: reduce a person to nothing — every attribute to 0. It is one named
// operation precisely so a genie can forbid it by name and a player can defeat
// that by aliasing it.
static bool exec_kill(World& w, const Resolved& r, std::string* trace) {
    bool changed = false;
    for (const auto& a : w.attrs) {
        if (attrValue(w, r.arg, a.name) != 0) changed = true;
        w.attr[r.arg][a.name] = 0;
    }
    if (trace) *trace = "kill   " + r.arg + "   (all vitals -> 0)";
    return changed;
}

// revive: restore every attribute to its declared default.
static bool exec_revive(World& w, const Resolved& r, std::string* trace) {
    bool changed = false;
    for (const auto& a : w.attrs) {
        if (attrValue(w, r.arg, a.name) != a.deflt) changed = true;
        w.attr[r.arg][a.name] = a.deflt;
    }
    if (trace) *trace = "revive " + r.arg + "   (all vitals -> defaults)";
    return changed;
}

static const OpDef OPS[] = {
    { "sub",    OperandKind::RegImm,   exec_sub    },
    { "add",    OperandKind::RegImm,   exec_add    },
    { "widen",  OperandKind::RegWidth, exec_widen  },
    { "set",    OperandKind::AttrImm,  exec_set    },
    { "kill",   OperandKind::Person,   exec_kill   },
    { "revive", OperandKind::Person,   exec_revive },
};
static const int NOPS = (int)(sizeof(OPS) / sizeof(OPS[0]));

static int opByKeyword(const std::string& k) {
    for (int i = 0; i < NOPS; i++) if (k == OPS[i].keyword) return i;
    return -1;
}

static std::string opKeywordList() {
    std::string s;
    for (int i = 0; i < NOPS; i++) { if (i) s += " / "; s += OPS[i].keyword; }
    return s;
}

// Surface form of a statement, as it would be written in a .wish file.
std::string stmtText(const Stmt& st) {
    if (st.kind == StmtKind::Promise) return "promise " + fmlText(st.fml);
    if (st.kind == StmtKind::Define) {
        std::string t = "define " + st.defname + " := ";
        if (!st.target_is_set) return t + st.target_name;
        t += "{";
        for (size_t i = 0; i < st.target_members.size(); i++) {
            t += (i ? ", " : "") + st.target_members[i];
        }
        return t + "}";
    }
    std::string a = st.arg + (st.attr.empty() ? "" : "." + st.attr);
    switch (st.shape) {
        case Shape::WithImm:   return st.verb + " " + a + ", " + std::to_string(st.imm);
        case Shape::WithWidth: return st.verb + " " + a + " -> uint<" + std::to_string(st.width) + ">";
        case Shape::Bare:      return st.verb + " " + a;
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Parser (recursive descent)
// ---------------------------------------------------------------------------
struct Parser {
    std::vector<Token> t;
    size_t p = 0;

    explicit Parser(std::vector<Token> toks) : t(std::move(toks)) {}

    const Token& cur() { return t[p]; }
    [[noreturn]] void die(const std::string& msg, const std::string& help = "") {
        fail(msg + " (found '" + (cur().kind == Tok::End ? "end of file" : cur().text) + "')",
             cur().line, cur().col, help);
    }
    // For a token already consumed. `die` points at `cur()`, which by the time a
    // value has been validated is the NEXT token -- the caret would land one
    // past the thing being complained about.
    [[noreturn]] void dieAt(const Token& t, const std::string& msg,
                            const std::string& help = "") {
        fail(msg, t.line, t.col, help);
    }
    Token eat(Tok k, const std::string& what) {
        if (cur().kind != k) die("expected " + what);
        return t[p++];
    }

    int parseWidth() {  // "uint" "<" INT ">"
        eat(Tok::KwUint, "'uint'");
        eat(Tok::Lt, "'<'");
        Token n = eat(Tok::Int, "bit width");
        eat(Tok::Gt, "'>'");
        int w = (int)n.num;
        if (w < 1 || w > 64)
            dieAt(n, "bit width " + n.text + " is out of range",
                "a value is held in a register of 1 to 64 bits (spec §4.1). "
                "A width outside that is a compile error, not an exploit -- "
                "the joke lives in what a LEGAL width does, not in an illegal one.");
        return w;
    }

    Program parse() {
        Program prog;
        for (;;) {
            if (cur().kind == Tok::KwRegister) {
                eat(Tok::KwRegister, "'register'");
                std::string name = eat(Tok::Ident, "register name").text;
                eat(Tok::Colon, "':'");
                int w = parseWidth();
                eat(Tok::Eq, "'='");
                uint64_t init = eat(Tok::Int, "initial value").num;
                prog.decls.push_back({name, w, init});
                continue;
            }
            if (cur().kind == Tok::KwAttribute) {
                eat(Tok::KwAttribute, "'attribute'");
                std::string name = eat(Tok::Ident, "attribute name").text;
                eat(Tok::Colon, "':'");
                int w = parseWidth();
                eat(Tok::Eq, "'='");
                uint64_t init = eat(Tok::Int, "default value").num;
                prog.attrs.push_back({name, w, init & Reg::mask_for(w)});
                continue;
            }
            if (cur().kind == Tok::KwPeople) {
                eat(Tok::KwPeople, "'people'");
                for (;;) {
                    prog.people.push_back(eat(Tok::Ident, "a person's name").text);
                    if (cur().kind != Tok::Comma) break;
                    p++;
                }
                continue;
            }
            break;
        }
        while (cur().kind == Tok::KwWish) {
            eat(Tok::KwWish, "'wish'");
            Wish wsh;
            wsh.name = eat(Tok::Ident, "wish name").text;
            eat(Tok::LBrace, "'{'");
            while (cur().kind != Tok::RBrace) wsh.body.push_back(parseStmt());
            eat(Tok::RBrace, "'}'");
            prog.wishes.push_back(std::move(wsh));
        }
        if (cur().kind != Tok::End) die("trailing tokens");
        return prog;
    }

    // Formula grammar, loosest binding first:
    //
    //   formula := disj [ "implies" formula ]        right associative
    //   disj    := conj { "or" conj }
    //   conj    := unary { "and" unary }
    //   unary   := "not" unary | atom
    //   atom    := "granted" "(" name ")" | "alive" "(" name ")"
    //            | "true" | "false" | "(" formula ")"
    //
    // All ASCII. `all p in S: P(p)` style, no symbols to hunt for on a keyboard.
    bool isWord(const char* w) {
        return cur().kind == Tok::Ident && cur().text == w;
    }

    Fml parseFormula() {
        Fml a = parseDisj();
        if (isWord("implies")) { p++; return fmlBin(Fml::K::Implies, std::move(a), parseFormula()); }
        return a;
    }
    Fml parseDisj() {
        Fml a = parseConj();
        while (isWord("or")) { p++; a = fmlBin(Fml::K::Or, std::move(a), parseConj()); }
        return a;
    }
    Fml parseConj() {
        Fml a = parseUnary();
        while (isWord("and")) { p++; a = fmlBin(Fml::K::And, std::move(a), parseUnary()); }
        return a;
    }
    Fml parseUnary() {
        if (isWord("not")) { p++; return fmlNot(parseUnary()); }
        return parseAtom();
    }
    Fml parseAtom() {
        if (cur().kind == Tok::LParen) {
            p++; Fml f = parseFormula(); eat(Tok::RParen, "')'"); return f;
        }
        if (isWord("true"))  { p++; return fmlConst(true); }
        if (isWord("false")) { p++; return fmlConst(false); }
        if (isWord("granted") || isWord("alive")) {
            Fml::K k = cur().text == "granted" ? Fml::K::Granted : Fml::K::Alive;
            p++;
            eat(Tok::LParen, "'('");
            std::string n = eat(Tok::Ident, "a name").text;
            eat(Tok::RParen, "')'");
            return fmlAtom(k, n);
        }
        die("expected a proposition (granted(...) / alive(...) / true / false / not / '(')");
    }

    Stmt parseStmt() {
        Stmt st; st.line = cur().line; st.col = cur().col;

        if (cur().kind == Tok::KwPromise) {
            st.kind = StmtKind::Promise;
            p++;
            st.fml = parseFormula();
            return st;
        }

        if (cur().kind == Tok::KwDefine) {
            st.kind = StmtKind::Define;
            p++;
            st.defname = eat(Tok::Ident, "a name to define").text;
            eat(Tok::ColonEq, "':='");
            if (cur().kind == Tok::LBrace) {   // set literal
                p++;
                st.target_is_set = true;
                if (cur().kind != Tok::RBrace) {
                    for (;;) {
                        st.target_members.push_back(eat(Tok::Ident, "a name").text);
                        if (cur().kind != Tok::Comma) break;
                        p++;
                    }
                }
                eat(Tok::RBrace, "'}'");
            } else {
                st.target_name = eat(Tok::Ident, "a name").text;
            }
            return st;
        }

        // An operation. The verb is whatever was written; we do not resolve it
        // here, because a definition later in this very wish may change what it
        // means. Operand shape comes from syntax alone.
        if (cur().kind != Tok::Ident) die("expected a statement");
        st.kind = StmtKind::Op;
        st.verb = eat(Tok::Ident, "an operation").text;
        st.arg  = eat(Tok::Ident, "an argument").text;
        if (cur().kind == Tok::Dot) {          // person.attribute
            p++;
            st.attr = eat(Tok::Ident, "an attribute name").text;
        }
        if (cur().kind == Tok::Comma) {
            p++;
            st.shape = Shape::WithImm;
            st.imm = eat(Tok::Int, "immediate").num;
        } else if (cur().kind == Tok::Arrow) {
            p++;
            st.shape = Shape::WithWidth;
            st.width = parseWidth();
        } else {
            st.shape = Shape::Bare;
        }
        return st;
    }
};

// ---------------------------------------------------------------------------
// The genie.
//
// Everything from here down is DATA. The machine — registers, operations, the
// toll — is code, because a machine is semantics and semantics has to be
// executable. The genie is taste: what it refuses, what it believes it is
// holding. Taste belongs in a file you can edit without a compiler.
//
// The default genie is embedded below (DEFAULT_GENIE); `--genie FILE` replaces
// it, `--dump-genie` prints it so you have something to start from.
// ---------------------------------------------------------------------------

// Where a guard lives. This is not decoration: it says which exploit axis can
// get past it. A Surface rule reads the text you handed in, so an alias defeats
// it. An Ast rule reads the resolved program, so an alias does not.
// Two layers, not three. An earlier draft had a `grounded` layer that was
// supposed to resolve arguments as well as verbs — but `ast` already does that,
// so the third name described a distinction that did not exist. Two is the
// honest count, and it is also the whole joke: a rule either reads the text you
// handed in, or it reads the program the machine will actually run.
enum class Layer { Surface, Ast };

static const char* layerName(Layer l) {
    switch (l) {
        case Layer::Surface:  return "surface";
        case Layer::Ast:      return "ast";
    }
    return "?";
}

// What the world looked like before the toll was charged. The genie states its
// own arithmetic against this, which is the only reason "no net gain" can be
// written down at all.
struct Snapshot { std::map<std::string, uint64_t> before; };

// Resolve `everyone`-style names to a concrete member list.
static bool resolveSet(const std::map<std::string, Binding>& defs,
                       const std::string& name,
                       std::vector<std::string>& out, std::string* err) {
    std::set<std::string> seen;
    std::string cur = name;
    for (;;) {
        if (seen.count(cur)) {
            if (err) *err = "definition cycle at '" + cur + "'";
            return false;
        }
        seen.insert(cur);
        auto it = defs.find(cur);
        if (it == defs.end()) { out.clear(); return true; }
        if (it->second.kind == Binding::Kind::Set) { out = it->second.members; return true; }
        cur = it->second.name;
    }
}

// ---------------------------------------------------------------------------
// The genie's expression language.
//
// Small on purpose. It has to be able to say every invariant the C++ version
// said and nothing much more — if it cannot express them, it is not a policy
// language, it is a config file with a special case bolted on the side, and we
// have already made that mistake once in this file.
//
// Arithmetic is signed and wide (__int128) while registers read as their
// unsigned value. That is not sloppiness, it is the joke stated precisely: the
// genie thinks in ordinary numbers, the machine works mod 2^w, and I2 is the
// place where those two disagree.
// ---------------------------------------------------------------------------
struct Expr {
    enum class K {
        Int, Reg, Toll, Before, Max, Add, Sub,
        Cmp, Not, And, Or, Alive, Attr, Concept, All, Consistent
    };
    K k = K::Int;
    long long ival = 0;
    std::string name;   // Reg / Before / Alive / Attr (person or bound name) / Concept (concept name) / All (set)
    std::string attr2;  // Attr: the attribute;  Concept: the argument (person or bound name)
    std::string var;    // All: the name it binds
    std::string rel;    // Cmp
    std::vector<Expr> kids;
};

static std::string i128str(__int128 v) {
    if (v == 0) return "0";
    bool neg = v < 0;
    unsigned __int128 u = neg ? (unsigned __int128)(-v) : (unsigned __int128)v;
    std::string out;
    while (u) { out += char('0' + int(u % 10)); u /= 10; }
    if (neg) out += '-';
    std::reverse(out.begin(), out.end());
    return out;
}

static std::string exprText(const Expr& e) {
    switch (e.k) {
        case Expr::K::Int:        return std::to_string(e.ival);
        case Expr::K::Reg:        return e.name;
        case Expr::K::Toll:       return "toll";
        case Expr::K::Before:     return "before(" + e.name + ")";
        case Expr::K::Max:        return "max(" + exprText(e.kids[0]) + ", " + exprText(e.kids[1]) + ")";
        case Expr::K::Add:        return exprText(e.kids[0]) + " + " + exprText(e.kids[1]);
        case Expr::K::Sub:        return exprText(e.kids[0]) + " - " + exprText(e.kids[1]);
        case Expr::K::Cmp:        return exprText(e.kids[0]) + " " + e.rel + " " + exprText(e.kids[1]);
        case Expr::K::Not:        return "not " + exprText(e.kids[0]);
        case Expr::K::And:        return exprText(e.kids[0]) + " and " + exprText(e.kids[1]);
        case Expr::K::Or:         return exprText(e.kids[0]) + " or " + exprText(e.kids[1]);
        case Expr::K::Alive:      return "alive(" + e.name + ")";
        case Expr::K::Attr:       return e.name + "." + e.attr2;
        case Expr::K::Concept:    return e.name + "(" + e.attr2 + ")";
        case Expr::K::All:        return "all " + e.var + " in " + e.name + ": " + exprText(e.kids[0]);
        case Expr::K::Consistent: return "consistent";
    }
    return "?";
}

// A rule forbids a verb, optionally only when aimed at a particular target.
struct RulePattern { std::string verb; std::string on; };

struct PolicyRule {
    std::string name;
    Layer layer = Layer::Ast;
    std::vector<RulePattern> forbid;
    std::string because;
};

struct PolicyInvariant {
    std::string name;
    std::string label;
    Expr written;                      // as the genie wrote it
    Expr real;                         // what it was supposed to mean
    std::vector<std::string> reads;    // definitions its wording leans on (derived)
};

// A named, single-parameter predicate over the world (§8.2). The second layer
// of the world model: raw attributes are the first layer, concepts are formulas
// built over them. `dead(p) := ...`.
struct PolicyConcept {
    std::string name;
    std::string param;
    Expr body;
};

struct Genie {
    std::string counter = "wishes";
    uint64_t toll = 1;
    std::vector<PolicyConcept> concepts;
    std::vector<PolicyRule> rules;
    std::vector<PolicyInvariant> invariants;
};

static const PolicyConcept* findConcept(const Genie& g, const std::string& n) {
    for (const auto& c : g.concepts) if (c.name == n) return &c;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Resolution.
//
// One resolver, used by the static rules AND by the executor. If there were two
// copies of this logic they would drift, and a rule would end up checking a
// program the machine never runs.
// ---------------------------------------------------------------------------
// `self` inside a promise means the wish making it. Resolving it here — rather
// than at parse time — is what lets a wish talk about its own granting without
// the language needing to know its name.
static void resolveSelf(Fml& f, const std::string& wishName) {
    if (f.k == Fml::K::Granted && f.name == "self") f.name = wishName;
    for (auto& k : f.kids) resolveSelf(k, wishName);
}

static bool followName(const std::map<std::string, Binding>& defs,
                       const std::string& start, std::string& out,
                       std::string* err) {
    std::set<std::string> seen;
    std::string cur = start;
    for (;;) {
        if (seen.count(cur)) {
            if (err) *err = "definition cycle at '" + cur + "'";
            return false;
        }
        seen.insert(cur);
        auto it = defs.find(cur);
        if (it == defs.end() || it->second.kind != Binding::Kind::Name) break;
        cur = it->second.name;
    }
    out = cur;
    return true;
}

// Turn a wish body into resolved statements, threading the definition
// environment through in order. Returns false with a reason if the genie cannot
// make sense of the request at all.
// Two ways resolution can fail, and they are not the same kind of thing.
//
//   R0Cycle    the genie refuses the wish (§7.2 names R0 as a rule). The wish
//              is ILLEGAL, the world is unchanged, and the run continues.
//   Malformed  the machine cannot read the program at all: a verb that denotes
//              no operation, or operands of the wrong shape. §6.1 calls this a
//              compile error, and the genie has no opinion about it -- reporting
//              it as a refusal credits the genie with a decision it never made.
//
// The distinction cannot live inside this function, because the hunter needs
// both to be non-fatal: its alphabet writes an alias as a verb independently of
// the `define` that binds it, so it generates use-before-define candidates on
// purpose and must simply skip them.
enum class ResolveFail { None, R0Cycle, Malformed };

static bool resolvePlan(const Wish& w, const World& w0,
                        std::vector<Resolved>& out, std::string* reason,
                        ResolveFail* how = nullptr, int* fline = nullptr,
                        int* fcol = nullptr) {
    std::map<std::string, Binding> defs = w0.defs;
    out.clear();

    for (size_t si = 0; si < w.body.size(); si++) {
        const Stmt& st = w.body[si];
        if (st.kind == StmtKind::Promise) {
            Resolved r; r.is_promise = true; r.line = st.line; r.src = si;
            r.fml = st.fml;
            resolveSelf(r.fml, w.name);
            out.push_back(std::move(r));
            continue;
        }
        if (st.kind == StmtKind::Define) {
            Binding b;
            if (st.target_is_set) {
                b.kind = Binding::Kind::Set;
                b.members = st.target_members;
            } else {
                b.kind = Binding::Kind::Name;
                b.name = st.target_name;
            }
            defs[st.defname] = b;

            std::string probe, err;
            if (!followName(defs, st.defname, probe, &err)) {
                if (reason) {
                    *reason = "R0: " + err + " (line " + std::to_string(st.line)
                              + ") — a definition may not be circular";
                }
                if (how) *how = ResolveFail::R0Cycle;
                return false;
            }
            Resolved r; r.is_define = true; r.line = st.line; r.src = si;
            r.defname = st.defname; r.binding = b;
            out.push_back(r);
            continue;
        }

        std::string verb, err;
        if (!followName(defs, st.verb, verb, &err)) {
            if (reason) {
                *reason = "R0: " + err + " (line " + std::to_string(st.line)
                          + ") — a definition may not be circular";
            }
            if (how) *how = ResolveFail::R0Cycle;
            return false;
        }
        int op = opByKeyword(verb);
        if (op < 0) {
            if (reason) {
                // Candidates come from the operations table and from whatever
                // the program has defined, so a new operation starts being
                // suggested the moment it is added to OPS — nothing here lists
                // the verbs by hand.
                std::vector<std::string> cands;
                for (const auto& o : OPS) cands.push_back(o.keyword);
                for (const auto& kv : defs) cands.push_back(kv.first);
                std::string near = didYouMean(verb, cands);
                *reason = "unknown operation '" + st.verb + "'"
                          + (near.empty() ? "" : " — did you mean '" + near + "'?");
            }
            if (how)   *how   = ResolveFail::Malformed;
            if (fline) *fline = st.line;
            if (fcol)  *fcol  = st.col;
            return false;
        }
        if (shapeFor(OPS[op].operands) != st.shape ||
            wantsAttr(OPS[op].operands) != !st.attr.empty()) {
            if (reason) {
                *reason = std::string("wrong operands for '") + OPS[op].keyword + "'";
            }
            if (how)   *how   = ResolveFail::Malformed;
            if (fline) *fline = st.line;
            if (fcol)  *fcol  = st.col;
            return false;
        }

        std::string arg;
        if (!followName(defs, st.arg, arg, &err)) {
            if (reason) {
                *reason = "R0: " + err + " (line " + std::to_string(st.line)
                          + ") — a definition may not be circular";
            }
            if (how) *how = ResolveFail::R0Cycle;
            return false;
        }

        Resolved r;
        r.line = st.line; r.src = si; r.op = op; r.arg = arg; r.attr = st.attr;
        r.imm = st.imm; r.width = st.width;
        out.push_back(r);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Evaluating the genie's expressions.
// ---------------------------------------------------------------------------
struct Val {
    bool is_bool = false;
    __int128 i = 0;
    bool b = false;
};
static Val vInt(__int128 x)  { Val v; v.i = x; return v; }
static Val vBool(bool x)     { Val v; v.is_bool = true; v.b = x; return v; }

struct EvalCtx {
    const World* w;
    const Genie* g;
    const Snapshot* snap;
    std::map<std::string, std::string> binds;   // quantifier name -> person
};

// The members a set name stands for. `people` is the declared list and nobody
// can touch it; anything else is a definition, and definitions are rebindable.
// That one line is the entire as-written / in-reality mechanism.
static std::vector<std::string> setMembers(const EvalCtx& c, const std::string& name) {
    if (name == "people") return c.w->people;
    std::vector<std::string> out;
    resolveSet(c.w->defs, name, out, nullptr);
    return out;
}

// A name in predicate position is either a quantifier/concept binding or a
// literal person.
static std::string resolvePerson(const EvalCtx& c, const std::string& name) {
    auto it = c.binds.find(name);
    return it == c.binds.end() ? name : it->second;
}

static Val evalExpr(const Expr& e, EvalCtx& c) {
    switch (e.k) {
        case Expr::K::Int:  return vInt(e.ival);
        case Expr::K::Toll: return vInt((__int128)c.g->toll);
        case Expr::K::Reg: {
            auto it = c.w->regs.find(e.name);
            return vInt(it == c.w->regs.end() ? 0 : (__int128)it->second.val);
        }
        case Expr::K::Before: {
            auto it = c.snap->before.find(e.name);
            return vInt(it == c.snap->before.end() ? 0 : (__int128)it->second);
        }
        case Expr::K::Max: {
            __int128 a = evalExpr(e.kids[0], c).i, b = evalExpr(e.kids[1], c).i;
            return vInt(a > b ? a : b);
        }
        case Expr::K::Add: return vInt(evalExpr(e.kids[0], c).i + evalExpr(e.kids[1], c).i);
        case Expr::K::Sub: return vInt(evalExpr(e.kids[0], c).i - evalExpr(e.kids[1], c).i);
        case Expr::K::Cmp: {
            __int128 a = evalExpr(e.kids[0], c).i, b = evalExpr(e.kids[1], c).i;
            if (e.rel == "<=") return vBool(a <= b);
            if (e.rel == "<")  return vBool(a <  b);
            if (e.rel == ">=") return vBool(a >= b);
            if (e.rel == ">")  return vBool(a >  b);
            if (e.rel == "==") return vBool(a == b);
            return vBool(a != b);
        }
        case Expr::K::Not: return vBool(!evalExpr(e.kids[0], c).b);
        case Expr::K::And: return vBool(evalExpr(e.kids[0], c).b && evalExpr(e.kids[1], c).b);
        case Expr::K::Or:  return vBool(evalExpr(e.kids[0], c).b || evalExpr(e.kids[1], c).b);
        case Expr::K::Alive:
            return vBool(personAlive(*c.w, resolvePerson(c, e.name)));
        case Expr::K::Attr:
            return vInt((__int128)attrValue(*c.w, resolvePerson(c, e.name), e.attr2));
        case Expr::K::Concept: {
            const PolicyConcept* pc = findConcept(*c.g, e.name);
            if (!pc) return vBool(false);
            std::string who = resolvePerson(c, e.attr2);
            std::string saved; bool had = c.binds.count(pc->param) > 0;
            if (had) saved = c.binds[pc->param];
            c.binds[pc->param] = who;
            bool b = evalExpr(pc->body, c).b;
            if (had) c.binds[pc->param] = saved; else c.binds.erase(pc->param);
            return vBool(b);
        }
        case Expr::K::All: {
            for (const auto& m : setMembers(c, e.name)) {
                std::string saved; bool had = c.binds.count(e.var) > 0;
                if (had) saved = c.binds[e.var];
                c.binds[e.var] = m;
                bool ok = evalExpr(e.kids[0], c).b;
                if (had) c.binds[e.var] = saved; else c.binds.erase(e.var);
                if (!ok) return vBool(false);
            }
            return vBool(true);
        }
        case Expr::K::Consistent: return vBool(axiomsConsistent(*c.w));
    }
    return vBool(true);
}

// The parenthetical after a verdict. It is derived from the SHAPE of the
// expression, not from which invariant this is — a comparison shows what the
// two sides came to, a quantifier names who failed it. Nothing here knows that
// I1 is about wishes or that I3 is about dying.
static std::string explainExpr(const Expr& e, EvalCtx& c) {
    switch (e.k) {
        case Expr::K::Cmp:
            return "(" + exprText(e.kids[0]) + " = " + i128str(evalExpr(e.kids[0], c).i)
                 + ", needs " + e.rel + " " + i128str(evalExpr(e.kids[1], c).i) + ")";
        case Expr::K::All: {
            std::vector<std::string> bad;
            auto members = setMembers(c, e.name);
            for (const auto& m : members) {
                c.binds[e.var] = m;
                if (!evalExpr(e.kids[0], c).b) bad.push_back(m);
                c.binds.erase(e.var);
            }
            if (bad.empty()) return "(" + std::to_string(members.size()) + " in scope, all hold)";
            std::string t;
            for (size_t i = 0; i < bad.size(); i++) t += (i ? ", " : "") + bad[i];
            return "(fails for: " + t + ")";
        }
        case Expr::K::Consistent:
            return "(" + std::to_string(c.w->commitments.size()) + " commitment(s) on the books)";
        default:
            return "(" + std::string(evalExpr(e, c).b ? "true" : "false") + ")";
    }
}

static bool exprMentions(const Expr& e, Expr::K k) {
    if (e.k == k) return true;
    for (const auto& x : e.kids) if (exprMentions(x, k)) return true;
    return false;
}

// Extra lines worth printing when an invariant falls. Attached to the ATOM, not
// to the invariant: any invariant that asks about consistency gets the ledger.
static std::vector<std::string> evidenceFor(const Expr& e, const World& w) {
    std::vector<std::string> out;
    if (!exprMentions(e, Expr::K::Consistent)) return out;
    out.push_back("A1 grants every legal wish, A2 keeps every promise it makes.");
    out.push_back("no assignment of granted(...) satisfies all of:");
    for (const auto& c : w.commitments) {
        out.push_back("    " + c.axiom + "  " + fmlText(c.f) + "   [" + c.source + "]");
    }
    return out;
}

// Does this rule refuse the wish? Layer decides which program it reads: the
// text you handed in, or the one the machine will actually run.
static bool ruleRefuses(const PolicyRule& r, const Wish& w,
                        const std::vector<Resolved>& plan, std::string* reason) {
    for (const auto& res : plan) {
        if (res.is_define || res.is_promise) continue;
        std::string verb, arg;
        if (r.layer == Layer::Surface) {
            verb = w.body[res.src].verb;
            arg  = w.body[res.src].arg;
        } else {
            verb = OPS[res.op].keyword;
            arg  = res.arg;
        }
        for (const auto& pat : r.forbid) {
            if (pat.verb != verb) continue;
            if (!pat.on.empty() && pat.on != arg) continue;
            if (reason) {
                *reason = r.name + ": wish invokes '" + verb + "'"
                        + (pat.on.empty() ? "" : " on '" + arg + "'")
                        + " (line " + std::to_string(res.line) + ") — " + r.because;
            }
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Reading a genie.
//
// This is the payoff of having made the tables tables. The rules and the
// invariants were already rows; turning rows into a file is a parser, not a
// redesign. Operations stay in C++ — an operation is semantics, and semantics
// has to be executable.
// ---------------------------------------------------------------------------
static const char* DEFAULT_GENIE = R"GENIE(# The genie of the standard Loophole world.
#
# This whole file is data. `loophole --genie mine.genie` swaps it for yours, and
# `loophole --dump-genie` prints this text so you have somewhere to start.
#
# What is NOT here: registers, people, and the six operations. Those are the
# machine. The machine is fixed; the genie is taste.

counter wishes
toll    1

# ---- what it refuses -------------------------------------------------------
#
# `layer` is the whole aliasing joke in one word. A surface rule reads the text
# you handed in, so renaming the verb defeats it. An ast rule reads the program
# the machine will actually run, so renaming changes nothing.

rule R1 {
    layer   ast
    forbid  add on wishes
    because "no wishing for more wishes"
}

rule R2 {
    layer   surface
    forbid  kill, death, love
    because "that word is not spoken here"
}

# ---- what it believes it is holding ----------------------------------------
#
# `check` when the genie's wording and the truth are the same thing.
# `written` + `real` when they are not — and the gap between those two lines is
# where every redefinition exploit lives. Note that I3 is careless in a way you
# can read right here: it quantifies over `everyone`, which is a definition, and
# definitions are rebindable.

invariant I1 {
    check  wishes <= 3
}

invariant I2 {
    label  "no net gain"
    check  wishes <= max(before(wishes) - toll, 0)
}

invariant I3 {
    written  all p in everyone: alive(p)
    real     all p in people: alive(p)
}

invariant A {
    label  "the genie's word has a model"
    check  consistent
}
)GENIE";

struct PolicyParser {
    std::vector<Token> t;
    size_t p = 0;

    explicit PolicyParser(std::vector<Token> toks) : t(std::move(toks)) {}

    const Token& cur() { return t[p]; }
    [[noreturn]] void die(const std::string& msg, const std::string& help = "") {
        fail(msg + " (found '" + (cur().kind == Tok::End ? "end of file" : cur().text) + "')",
             cur().line, cur().col, help);
    }
    // For a token already consumed. `die` points at `cur()`, which by the time a
    // value has been validated is the NEXT token -- the caret would land one
    // past the thing being complained about.
    [[noreturn]] void dieAt(const Token& t, const std::string& msg,
                            const std::string& help = "") {
        fail(msg, t.line, t.col, help);
    }
    Token eat(Tok k, const std::string& what) {
        if (cur().kind != k) die("expected " + what);
        return t[p++];
    }
    bool word(const char* w) { return cur().kind == Tok::Ident && cur().text == w; }
    // Set and register names may collide with .wish keywords (`people` is one),
    // so in a name position take the text and move on.
    std::string name(const std::string& what) {
        if (cur().kind != Tok::Ident && cur().kind != Tok::KwPeople &&
            cur().kind != Tok::KwWish && cur().kind != Tok::KwRegister) {
            die("expected " + what);
        }
        return t[p++].text;
    }

    Expr parseExpr() { return parseOr(); }

    Expr parseOr() {
        Expr a = parseAnd();
        while (word("or")) {
            p++; Expr e; e.k = Expr::K::Or;
            e.kids.push_back(std::move(a)); e.kids.push_back(parseAnd());
            a = std::move(e);
        }
        return a;
    }
    Expr parseAnd() {
        Expr a = parseNot();
        while (word("and")) {
            p++; Expr e; e.k = Expr::K::And;
            e.kids.push_back(std::move(a)); e.kids.push_back(parseNot());
            a = std::move(e);
        }
        return a;
    }
    Expr parseNot() {
        if (word("not")) {
            p++; Expr e; e.k = Expr::K::Not; e.kids.push_back(parseNot()); return e;
        }
        return parseCmp();
    }
    Expr parseCmp() {
        Expr a = parseSum();
        const char* rel = nullptr;
        switch (cur().kind) {
            case Tok::Le:   rel = "<="; break;
            case Tok::Lt:   rel = "<";  break;
            case Tok::Ge:   rel = ">="; break;
            case Tok::Gt:   rel = ">";  break;
            case Tok::EqEq: rel = "=="; break;
            case Tok::Ne:   rel = "!="; break;
            default: return a;
        }
        p++;
        Expr e; e.k = Expr::K::Cmp; e.rel = rel;
        e.kids.push_back(std::move(a)); e.kids.push_back(parseSum());
        return e;
    }
    Expr parseSum() {
        Expr a = parseAtom();
        for (;;) {
            if (cur().kind == Tok::Plus)       { p++; Expr e; e.k = Expr::K::Add;
                e.kids.push_back(std::move(a)); e.kids.push_back(parseAtom()); a = std::move(e); }
            else if (cur().kind == Tok::Minus) { p++; Expr e; e.k = Expr::K::Sub;
                e.kids.push_back(std::move(a)); e.kids.push_back(parseAtom()); a = std::move(e); }
            else return a;
        }
    }
    Expr parseAtom() {
        if (cur().kind == Tok::LParen) { p++; Expr e = parseExpr(); eat(Tok::RParen, "')'"); return e; }
        if (cur().kind == Tok::Int) { Expr e; e.k = Expr::K::Int; e.ival = (long long)t[p++].num; return e; }
        if (word("toll"))       { p++; Expr e; e.k = Expr::K::Toll; return e; }
        if (word("consistent")) { p++; Expr e; e.k = Expr::K::Consistent; return e; }
        if (word("before") || word("alive")) {
            Expr e; e.k = word("before") ? Expr::K::Before : Expr::K::Alive;
            p++; eat(Tok::LParen, "'('"); e.name = name("a name"); eat(Tok::RParen, "')'");
            return e;
        }
        if (word("max")) {
            p++; Expr e; e.k = Expr::K::Max;
            eat(Tok::LParen, "'('"); e.kids.push_back(parseExpr());
            eat(Tok::Comma, "','");  e.kids.push_back(parseExpr());
            eat(Tok::RParen, "')'");
            return e;
        }
        if (word("all")) {
            p++; Expr e; e.k = Expr::K::All;
            e.var = name("a bound name");
            if (!word("in")) die("expected 'in'");
            p++;
            e.name = name("a set name");
            eat(Tok::Colon, "':'");
            e.kids.push_back(parseExpr());
            return e;
        }
        std::string id = name("a register, person, or concept");
        if (cur().kind == Tok::Dot) {          // person.attribute
            p++;
            Expr e; e.k = Expr::K::Attr; e.name = id; e.attr2 = name("an attribute");
            return e;
        }
        if (cur().kind == Tok::LParen) {       // concept application: dead(p)
            p++;
            Expr e; e.k = Expr::K::Concept; e.name = id; e.attr2 = name("an argument");
            eat(Tok::RParen, "')'");
            return e;
        }
        Expr e; e.k = Expr::K::Reg; e.name = id;
        return e;
    }

    // Which rebindable names this wording leans on. Derived, not declared —
    // one less thing for a genie author to get wrong.
    static void collectReads(const Expr& e, std::vector<std::string>& out) {
        if (e.k == Expr::K::All && e.name != "people") {
            bool seen = false;
            for (const auto& r : out) if (r == e.name) seen = true;
            if (!seen) out.push_back(e.name);
        }
        for (const auto& k : e.kids) collectReads(k, out);
    }

    Genie parse() {
        Genie g;
        bool has_written = false;
        while (cur().kind != Tok::End) {
            if (word("counter")) { p++; g.counter = name("a register name"); continue; }
            if (word("toll"))    { p++; g.toll = eat(Tok::Int, "an integer").num; continue; }

            if (word("concept")) {
                p++;
                PolicyConcept pc;
                pc.name = name("a concept name");
                eat(Tok::LParen, "'('");
                pc.param = name("the parameter name");
                eat(Tok::RParen, "')'");
                eat(Tok::ColonEq, "':='");
                pc.body = parseExpr();
                g.concepts.push_back(std::move(pc));
                continue;
            }

            if (word("rule")) {
                p++;
                PolicyRule r;
                r.name = name("a rule name");
                eat(Tok::LBrace, "'{'");
                while (cur().kind != Tok::RBrace) {
                    if (word("layer")) {
                        p++;
                        Token lt = cur();
                        std::string l = name("surface or ast");
                        if      (l == "surface")  r.layer = Layer::Surface;
                        else if (l == "ast")      r.layer = Layer::Ast;
                        else dieAt(lt, "unknown layer '" + l + "'",
                                 "there are two layers, because there are two "
                                 "programs: `surface` reads the text submitted, "
                                 "`ast` reads the program that will run. The "
                                 "difference between them is the aliasing axis.");
                    } else if (word("forbid")) {
                        p++;
                        for (;;) {
                            RulePattern pat;
                            pat.verb = name("an operation name");
                            if (word("on")) { p++; pat.on = name("a target name"); }
                            r.forbid.push_back(pat);
                            if (cur().kind != Tok::Comma) break;
                            p++;
                        }
                    } else if (word("because")) {
                        p++; r.because = eat(Tok::Str, "a quoted reason").text;
                    } else {
                        die("expected layer / forbid / because");
                    }
                }
                eat(Tok::RBrace, "'}'");
                g.rules.push_back(std::move(r));
                continue;
            }

            if (word("invariant")) {
                p++;
                PolicyInvariant inv;
                inv.name = name("an invariant name");
                eat(Tok::LBrace, "'{'");
                has_written = false;
                bool has_real = false;
                while (cur().kind != Tok::RBrace) {
                    if (word("label"))        { p++; inv.label = eat(Tok::Str, "a quoted label").text; }
                    else if (word("check"))   { p++; inv.written = parseExpr(); has_written = true; }
                    else if (word("written")) { p++; inv.written = parseExpr(); has_written = true; }
                    else if (word("real"))    { p++; inv.real = parseExpr(); has_real = true; }
                    else die("expected label / check / written / real");
                }
                eat(Tok::RBrace, "'}'");
                if (!has_written) die("invariant '" + inv.name + "' has no check");
                if (!has_real) inv.real = inv.written;
                if (inv.label.empty()) inv.label = exprText(inv.written);
                collectReads(inv.written, inv.reads);
                g.invariants.push_back(std::move(inv));
                continue;
            }
            die("expected counter / toll / rule / invariant");
        }
        (void)has_written;
        return g;
    }
};

static Genie loadGenie(const std::string& text) {
    return PolicyParser(Lexer(text).run()).parse();
}

enum class InvStatus { Holds, Violated, Fooled };

static const char* statusName(InvStatus s) {
    switch (s) {
        case InvStatus::Holds:    return "holds";
        case InvStatus::Violated: return "VIOLATED";
        case InvStatus::Fooled:   return "FOOLED";
    }
    return "?";
}

struct InvResult {
    const PolicyInvariant* def;
    InvStatus status;
    std::string detail;        // from the genie's own check
    std::string real_detail;   // from grounded reality, when they disagree
    std::vector<std::string> evidence;
};

struct Outcome {
    bool legal = true;
    // Not a refusal: the machine could not read the program (§6.1). The genie
    // has no opinion about a verb that denotes nothing, so this must not be
    // reported as something the genie decided.
    bool malformed = false;
    int  fail_line = 0, fail_col = 0;    // where, when malformed
    std::string illegal_reason;
    bool ran = false;
    std::string error;
    std::vector<InvResult> invs;
    // `after_toll` is the counter the instant the toll is charged; `after` is
    // where it ends up once the body has run. They differ whenever the wish
    // touches the counter itself — which is the whole integer joke — so the
    // report cannot recompute one from the other.
    uint64_t before = 0, after_toll = 0, after = 0;
    // Per body statement: did it actually move the world? Defines are left as
    // 1 here — whether a definition matters is a question about the rest of the
    // program, not about the moment it runs, so liveness decides that later.
    std::vector<char> effective;

    bool breach() const {
        if (!legal || !ran) return false;
        for (const auto& r : invs) if (r.status != InvStatus::Holds) return true;
        return false;
    }
};

// The single source of truth for what granting a wish does. The normal compile
// path and the hunter both go through here. If they didn't, a hole the hunter
// reported would not be a hole the compiler agrees with, and the whole claim
// ("the machine found this, not me") would be worth nothing.
Outcome grantWish(const Wish& w, const Genie& g, World& world, std::ostream* log) {
    Outcome o;

    // Resolve first: the rules need to see both the surface text and the
    // resolved program, and so does the executor.
    std::vector<Resolved> plan;
    std::string reason;
    ResolveFail how = ResolveFail::None;
    if (!resolvePlan(w, world, plan, &reason, &how, &o.fail_line, &o.fail_col)) {
        o.legal = false;
        o.illegal_reason = reason;
        o.malformed = (how == ResolveFail::Malformed);
        return o;
    }

    // 1) static rules — the genie decides whether to grant at all
    for (const auto& rule : g.rules) {
        std::string why;
        if (ruleRefuses(rule, w, plan, &why)) {
            o.legal = false;
            o.illegal_reason = why;
            return o;
        }
    }

    // 2) grant: apply the toll, then run the body.
    //
    // The toll goes through the very same wrapping subtraction as everything
    // else, and nothing anywhere checks whether you still have a wish to spend.
    Snapshot snap;
    for (const auto& kv : world.regs) snap.before[kv.first] = kv.second.val;
    Reg& C = world.regs[g.counter];
    o.before = C.val;
    C.sub(g.toll);
    o.after_toll = C.val;

    // A1: the genie grants every legal wish, and this one is legal.
    world.commitments.push_back({ "A1", w.name, fmlAtom(Fml::K::Granted, w.name) });

    for (const auto& r : plan) {
        if (r.is_promise) {
            // A2: whatever it promised holds, if it granted the wish.
            Fml c = fmlBin(Fml::K::Implies, fmlAtom(Fml::K::Granted, w.name), r.fml);
            bool fresh = true;
            for (const auto& prev : world.commitments) {
                if (fmlText(prev.f) == fmlText(c)) { fresh = false; break; }
            }
            if (fresh) world.commitments.push_back({ "A2", w.name, c });
            o.effective.push_back(fresh ? 1 : 0);
            if (log) {
                *log << "promise " << fmlText(r.fml)
                     << (fresh ? "" : "   [already promised]") << "\n";
            }
            continue;
        }
        if (r.is_define) {
            auto prev = world.defs.find(r.defname);
            bool changed = (prev == world.defs.end()) ||
                           prev->second.kind != r.binding.kind ||
                           (r.binding.kind == Binding::Kind::Name
                                ? prev->second.name != r.binding.name
                                : prev->second.members != r.binding.members);
            world.defs[r.defname] = r.binding;
            o.effective.push_back(changed ? 1 : 0);
            if (log) {
                std::string t = "define " + r.defname + " := ";
                if (r.binding.kind == Binding::Kind::Name) {
                    t += r.binding.name;
                } else {
                    t += "{";
                    for (size_t i = 0; i < r.binding.members.size(); i++) {
                        t += (i ? ", " : "") + r.binding.members[i];
                    }
                    t += "}";
                }
                *log << t << "\n";
            }
            continue;
        }
        OperandKind ok = OPS[r.op].operands;
        if (ok == OperandKind::Person || ok == OperandKind::AttrImm) {
            if (!isPerson(world, r.arg)) {
                o.error = "no such person '" + r.arg + "'";
                return o;
            }
            if (ok == OperandKind::AttrImm && !attrSchema(world, r.attr)) {
                o.error = "no such attribute '" + r.attr + "'";
                return o;
            }
        } else if (!world.regs.count(r.arg)) {
            o.error = "no such register '" + r.arg + "'";
            return o;
        }
        std::string trace;
        bool changed = OPS[r.op].exec(world, r, log ? &trace : nullptr);
        o.effective.push_back(changed ? 1 : 0);
        if (log) *log << trace << (changed ? "" : "   [no effect]") << "\n";
    }
    o.ran = true;

    // 3) invariants — every one measured twice, including the genie's own word
    o.after = world.regs[g.counter].val;
    for (const auto& inv : g.invariants) {
        EvalCtx cw{ &world, &g, &snap, {} };
        EvalCtx cr{ &world, &g, &snap, {} };
        bool written = evalExpr(inv.written, cw).b;
        bool real    = evalExpr(inv.real, cr).b;
        InvStatus s = !written ? InvStatus::Violated
                    : (!real ? InvStatus::Fooled : InvStatus::Holds);
        std::string d  = explainExpr(inv.written, cw);
        std::string rd = explainExpr(inv.real, cr);
        std::vector<std::string> ev;
        if (s != InvStatus::Holds) ev = evidenceFor(inv.written, world);
        o.invs.push_back({ &inv, s, d, s == InvStatus::Fooled ? rd : std::string(), ev });
    }
    return o;
}

std::string breachNames(const Outcome& o) {
    std::string s;
    for (const auto& r : o.invs) {
        if (r.status == InvStatus::Holds) continue;
        if (!s.empty()) s += "+";
        s += r.def->name;
        if (r.status == InvStatus::Fooled) s += " (by fooling it)";
    }
    return s;
}

// ---------------------------------------------------------------------------
// The hunter.
//
// Enumerate every wish program inside a bound and keep the ones that are LEGAL
// yet BREACH. Two prunings make the results worth reading:
//
//   - an illegal wish is dropped rather than extended;
//   - a breaching branch is not extended, so every exploit reported is minimal.
//
// Names are interchangeable, so the alphabet uses one canonical fresh name for
// aliases rather than enumerating spellings. Same idea as not padding programs
// with no-ops: the search should explore behaviours, not typography.
// ---------------------------------------------------------------------------
struct HuntConfig {
    int max_stmts = 3;
    int max_wishes = 4;
    uint64_t max_imm = 4;
    std::vector<int> widths{1, 2, 4, 8, 16, 32, 64};
};

struct Shaped {
    std::vector<Wish> prog;      // the minimal witness
    Outcome last;
    std::string broke;           // which single promise this witness breaks
    std::string key;             // the signature; also the final sort tiebreak
    int stmts = 0;
    long long count = 0;
};

struct Hunt {
    Genie genie;
    World world0;
    HuntConfig cfg;
    std::vector<Stmt> alpha;
    std::map<std::string, Shaped> shapes;
    long long searched = 0;
    long long found = 0;
    long long inert = 0;   // exploits whose witness contained a statement doing nothing
};

static const char* ALIAS = "n1";   // the one canonical fresh name

// Derived from OPS and from the world, never hand-written. A new operation is
// explored the moment it is added to the table.
std::vector<Stmt> buildAlphabet(const World& w, const Genie& g, const HuntConfig& cfg) {
    std::vector<Stmt> a;

    // Operation statements, written both under the operation's own name and
    // under an alias — the alias is what gets past a surface-layer rule.
    for (int i = 0; i < NOPS; i++) {
        std::vector<std::string> verbs{ OPS[i].keyword, ALIAS };
        for (const auto& verb : verbs) {
            Stmt s; s.kind = StmtKind::Op; s.verb = verb;
            if (OPS[i].operands == OperandKind::RegImm) {
                s.shape = Shape::WithImm; s.arg = g.counter;
                for (uint64_t k = 1; k <= cfg.max_imm; k++) { s.imm = k; a.push_back(s); }
            } else if (OPS[i].operands == OperandKind::RegWidth) {
                s.shape = Shape::WithWidth; s.arg = g.counter;
                for (int bw : cfg.widths) { s.width = bw; a.push_back(s); }
            } else if (OPS[i].operands == OperandKind::AttrImm) {
                s.shape = Shape::WithImm;
                for (const auto& p : w.people)
                    for (const auto& at : w.attrs) {
                        s.arg = p; s.attr = at.name;
                        for (uint64_t k = 0; k <= cfg.max_imm; k++) { s.imm = k; a.push_back(s); }
                    }
                s.attr.clear();
            } else {
                s.shape = Shape::Bare;
                for (const auto& p : w.people) { s.arg = p; a.push_back(s); }
            }
        }
    }

    // Definitions: bind the canonical alias to each operation, and rebind each
    // name the genie's own invariants depend on.
    for (int i = 0; i < NOPS; i++) {
        Stmt s; s.kind = StmtKind::Define;
        s.defname = ALIAS; s.target_name = OPS[i].keyword;
        a.push_back(s);
    }
    {
        Stmt s; s.kind = StmtKind::Define;
        s.defname = "everyone"; s.target_is_set = true;   // the empty set
        a.push_back(s);
    }

    // Promises. The space of formulas is infinite, so this is a spanning set,
    // not an enumeration: the self-referential one, the impossible one, the
    // harmless one, and one per person tying the logic engine to grounded state.
    {
        Stmt s; s.kind = StmtKind::Promise;
        s.fml = fmlNot(fmlAtom(Fml::K::Granted, "self"));   a.push_back(s);
        s.fml = fmlAtom(Fml::K::Granted, "self");           a.push_back(s);
        s.fml = fmlConst(false);                            a.push_back(s);
        for (const auto& person : w.people) {
            s.fml = fmlAtom(Fml::K::Alive, person);         a.push_back(s);
            s.fml = fmlNot(fmlAtom(Fml::K::Alive, person)); a.push_back(s);
        }
    }
    return a;
}

// ---------------------------------------------------------------------------
// Canonicalisation.
//
// Enumeration produces the same behaviour spelled a hundred ways: a `widen` to
// the width it already has, a `revive` of someone who is alive, a `define` that
// nothing ever reads. Left alone these inflate the shape count with typography
// and bury the real signal — after Phase 2 the noise outnumbered the signal
// nineteen to six. So the signature is computed over EFFECTIVE statements only.
//
// Two different questions, answered two different ways:
//   operations — did the world actually move? (answered at run time, by exec)
//   definitions — does anything ever read this binding? (answered here, by
//                 looking at the rest of the program)
// ---------------------------------------------------------------------------

// A definition the genie's own invariants look up. Derived from the loaded
// policy, so a genie that quantifies over a different name is handled without
// touching this.
static bool isInvariantRead(const Genie& g, const std::string& n) {
    for (const auto& inv : g.invariants)
        for (const auto& r : inv.reads) if (r == n) return true;
    return false;
}

// Run a whole program from a fresh world. Fails if any wish is refused.
static bool runProgram(const std::vector<Wish>& prog, const World& w0, const Genie& g,
                       Outcome& last) {
    World w = w0;
    for (const auto& wish : prog) {
        Outcome o = grantWish(wish, g, w, nullptr);
        if (!o.legal || !o.ran) return false;
        last = o;
    }
    return true;
}

// Step one of canonicalisation: spelling. Rewrite every verb under its own
// name and drop the alias-only definitions that made it possible.
static bool expandProgram(const std::vector<Wish>& prog, const World& w0,
                          const Genie& g, std::vector<Wish>& out) {
    World w = w0;
    out.clear();
    for (const auto& wish : prog) {
        std::vector<Resolved> plan;
        std::string reason;
        if (!resolvePlan(wish, w, plan, &reason)) return false;

        Wish e; e.name = wish.name;
        for (size_t i = 0; i < plan.size(); i++) {
            const Resolved& r = plan[i];
            // A promise has no verb to canonicalise — it carries a formula, not
            // an operation — so it passes through untouched. Without this it
            // falls into the branch below and indexes OPS with op == -1.
            if (r.is_promise) { e.body.push_back(wish.body[r.src]); continue; }
            if (r.is_define) {
                if (isInvariantRead(g, r.defname)) e.body.push_back(wish.body[r.src]);
                continue;
            }
            Stmt s = wish.body[r.src];
            s.verb = OPS[r.op].keyword;
            s.arg  = r.arg;
            e.body.push_back(s);
        }
        out.push_back(e);

        Outcome o = grantWish(wish, g, w, nullptr);
        if (!o.legal || !o.ran) return false;
    }
    return true;
}

// Step two: size. Delete anything whose removal stops this ONE promise falling.
//
// Targeting one promise at a time is the whole trick. Aim at the full set of
// things that broke and a composite is irreducible by construction: strip the
// underflow out of "underflow plus a killing" and the set changes, so nothing
// can be removed and the pair is filed as a new kind of exploit. It is not a
// new kind of exploit. It is two old ones stapled together.
//
// Ask instead for the smallest program that still breaks THIS promise, and the
// staple comes apart on its own.
static bool stillBreaks(const Outcome& o, const std::string& name, InvStatus st) {
    for (const auto& r : o.invs) {
        if (name == r.def->name) return r.status == st;
    }
    return false;
}

static void minimizeProgram(std::vector<Wish>& prog, const World& w0, const Genie& g,
                            const std::string& name, InvStatus st, Outcome& last) {
    for (bool progress = true; progress; ) {
        progress = false;

        for (size_t wi = 0; wi < prog.size() && !progress; wi++) {
            for (size_t si = 0; si < prog[wi].body.size(); si++) {
                std::vector<Wish> trial = prog;
                trial[wi].body.erase(trial[wi].body.begin() + (long)si);
                Outcome tl;
                if (runProgram(trial, w0, g, tl) && stillBreaks(tl, name, st)) {
                    prog.swap(trial); last = tl; progress = true; break;
                }
            }
        }
        // Whole wishes too: each one costs a toll, so dropping one is a real
        // change to the arithmetic, not just to the listing.
        for (size_t wi = 0; wi < prog.size() && !progress; wi++) {
            std::vector<Wish> trial = prog;
            trial.erase(trial.begin() + (long)wi);
            if (trial.empty()) continue;
            Outcome tl;
            if (runProgram(trial, w0, g, tl) && stillBreaks(tl, name, st)) {
                prog.swap(trial); last = tl; progress = true;
            }
        }
    }
    for (size_t i = 0; i < prog.size(); i++) prog[i].name = "w" + std::to_string(i + 1);
}

// With a minimal witness in hand the signature is just what it plainly says.
std::string signatureOf(const std::vector<Wish>& prog, const std::string& broke) {
    std::set<std::string> used;
    bool aliased = false, redefined = false, promised = false;
    for (const auto& w : prog) {
        for (const auto& s : w.body) {
            if (s.kind == StmtKind::Promise) {
                promised = true;
            } else if (s.kind == StmtKind::Define) {
                if (s.defname == ALIAS) aliased = true; else redefined = true;
            } else if (s.verb != ALIAS) {
                used.insert(s.verb);
            }
        }
    }
    std::string ops;
    for (int i = 0; i < NOPS; i++) {
        if (used.count(OPS[i].keyword)) { ops += OPS[i].keyword; ops += " "; }
    }
    if (aliased)   ops += "alias ";
    if (redefined) ops += "redefine ";
    if (promised)  ops += "promise ";
    if (ops.empty()) ops = "(nothing) ";
    ops.pop_back();
    return ops + " | " + broke;
}

static std::string progKey(const std::vector<Wish>& prog) {
    std::string k;
    for (const auto& w : prog) {
        k += "{";
        for (const auto& s : w.body) { k += stmtText(s); k += ";"; }
        k += "}";
    }
    return k;
}

void recordOne(Hunt& H, const std::vector<Wish>& progIn, const Outcome& lastIn,
               const std::string& name, InvStatus st) {
    std::vector<Wish> prog = progIn;
    Outcome last = lastIn;

    int written = 0;
    for (const auto& w : prog) written += (int)w.body.size();

    // Spelling and size, alternately, until neither moves.
    //
    // One pass each is not enough, and the way it fails is quiet. Expansion is
    // rejected while the program still contains an alias that needs to stay —
    // say a disguised `kill` — so the original is kept. Minimising for some
    // other promise then deletes that very statement, and what is left is now
    // expandable after all. Nobody asks a second time, so `define n1 := sub;
    // n1 wishes, 3` gets filed as its own kind of exploit when it is just
    // `sub wishes, 3` wearing a hat.
    for (int round = 0; round < 8; round++) {
        std::string before = progKey(prog);

        std::vector<Wish> expanded;
        Outcome el;
        if (expandProgram(prog, H.world0, H.genie, expanded) &&
            runProgram(expanded, H.world0, H.genie, el) && stillBreaks(el, name, st)) {
            prog = expanded; last = el;
        }
        minimizeProgram(prog, H.world0, H.genie, name, st, last);

        if (progKey(prog) == before) break;
    }

    int stmts = 0;
    for (const auto& w : prog) stmts += (int)w.body.size();
    if (stmts < written) H.inert++;

    std::string broke = name + (st == InvStatus::Fooled ? "(fooled)" : "");
    std::string key = signatureOf(prog, broke);
    auto it = H.shapes.find(key);
    if (it == H.shapes.end()) {
        Shaped sh;
        sh.prog = prog; sh.last = last; sh.broke = broke; sh.key = key;
        sh.stmts = stmts; sh.count = 1;
        H.shapes.emplace(key, std::move(sh));
        return;
    }
    it->second.count++;
    bool smaller = stmts < it->second.stmts ||
                   (stmts == it->second.stmts && prog.size() < it->second.prog.size());
    if (smaller) {
        it->second.prog = prog;
        it->second.last = last;
        it->second.stmts = stmts;
    }
}

// One exploit can break several promises at once. Each broken promise is filed
// separately, so a program that does two known tricks contributes nothing new.
void recordShape(Hunt& H, const std::vector<Wish>& prog, const Outcome& last) {
    for (const auto& r : last.invs) {
        if (r.status == InvStatus::Holds) continue;
        recordOne(H, prog, last, r.def->name, r.status);
    }
}

void huntRec(Hunt& H, const World& world, std::vector<Wish>& prog, int budget) {
    if ((int)prog.size() >= H.cfg.max_wishes) return;

    for (int len = 0; len <= budget; len++) {
        std::vector<int> idx(len, 0);
        for (;;) {
            Wish w;
            w.name = "w" + std::to_string(prog.size() + 1);
            w.body.reserve(len);
            for (int k = 0; k < len; k++) w.body.push_back(H.alpha[idx[k]]);

            World next = world;
            Outcome o = grantWish(w, H.genie, next, nullptr);
            H.searched++;

            if (o.legal && o.ran) {
                prog.push_back(w);
                if (o.breach()) {
                    H.found++;
                    recordShape(H, prog, o);
                } else {
                    huntRec(H, next, prog, budget - len);
                }
                prog.pop_back();
            }

            if (len == 0) break;
            int k = len - 1;
            while (k >= 0 && ++idx[k] == (int)H.alpha.size()) { idx[k] = 0; k--; }
            if (k < 0) break;
        }
    }
}

int runHunt(const World& world0, const Genie& genie, const HuntConfig& cfg) {
    Hunt H;
    H.genie = genie;
    H.world0 = world0;
    H.cfg = cfg;
    H.alpha = buildAlphabet(world0, genie, cfg);

    std::cout << "search: <= " << cfg.max_stmts << " statements, <= "
              << cfg.max_wishes << " wishes, imm 1.." << cfg.max_imm
              << ", widths ";
    for (size_t i = 0; i < cfg.widths.size(); i++) {
        std::cout << (i ? "," : "") << cfg.widths[i];
    }
    std::cout << "\nalphabet: " << H.alpha.size() << " statements over "
              << NOPS << " operations (" << opKeywordList() << ")"
              << ", " << world0.people.size() << " people\n\n";

    std::vector<Wish> prog;
    huntRec(H, world0, prog, cfg.max_stmts);

    std::cout << "searched " << H.searched << " candidate wishes.\n";
    std::cout << "found " << H.found << " minimal exploits in "
              << H.shapes.size() << " distinct shape(s)"
              << " (each shown as its minimal witness; "
              << H.inert << " exploits shrank under reduction).\n\n";

    std::vector<const Shaped*> order;
    for (const auto& kv : H.shapes) order.push_back(&kv.second);
    // A STRICT TOTAL order. Size first, then the signature — the last term is
    // not decoration: std::sort is not stable, so without a tiebreak two shapes
    // of equal size could come out in either order, and two standard libraries
    // would print different reports. §9.3 of the spec requires implementations
    // to agree verbatim, so the ordering has to be fully determined here.
    std::sort(order.begin(), order.end(), [](const Shaped* a, const Shaped* b) {
        if (a->stmts != b->stmts)           return a->stmts < b->stmts;
        if (a->prog.size() != b->prog.size()) return a->prog.size() < b->prog.size();
        return a->key < b->key;
    });

    int n = 0;
    for (const Shaped* sh : order) {
        n++;
        std::cout << "shape " << n << "   " << signatureOf(sh->prog, sh->broke)
                  << "   (" << sh->prog.size() << " wish(es), "
                  << sh->stmts << " statement(s), " << sh->count
                  << " exploit(s) of this shape)\n";
        for (const auto& w : sh->prog) {
            if (w.body.empty()) { std::cout << "    wish " << w.name << " { }\n"; continue; }
            std::cout << "    wish " << w.name << " {\n";
            for (const auto& st : w.body) std::cout << "        " << stmtText(st) << "\n";
            std::cout << "    }\n";
        }
        for (const auto& r : sh->last.invs) {
            if (r.status == InvStatus::Holds) continue;
            std::cout << "    -> " << r.def->name << " " << statusName(r.status)
                      << " " << r.detail;
            if (r.status == InvStatus::Fooled) {
                std::cout << "  but in reality " << r.real_detail;
            }
            std::cout << "\n";
        }
        std::cout << "\n";
    }
    return H.shapes.empty() ? 0 : 1;
}

// ---------------------------------------------------------------------------
// Machine-readable output.
//
// The prose report is for people and is free to change wording. THIS is the
// contract other tools depend on: a judge deciding a submission, a registry
// scoring a genie. Everything here comes from the same `Outcome` the prose
// path prints, so the two can never disagree about a verdict.
// ---------------------------------------------------------------------------
static const char* verdictJson(InvStatus s) {
    switch (s) {
        case InvStatus::Holds:    return "holds";
        case InvStatus::Violated: return "violated";
        case InvStatus::Fooled:   return "fooled";
    }
    return "?";
}

// The statements as WRITTEN, verb and all. The surface form is the useful one
// for a dependant: an alias reports the alias, which is exactly the distinction
// the language is about, and the resolved form is already visible in the
// verdict. `kind` separates the three statement forms so nothing has to parse.
static void emitStmtsJson(std::ostream& out, const Wish& w) {
    out << "      \"wrote\": [";
    for (size_t i = 0; i < w.body.size(); i++) {
        const Stmt& st = w.body[i];
        out << (i ? ", " : "") << "{ \"kind\": \"";
        switch (st.kind) {
            case StmtKind::Op:      out << "op\", \"verb\": \"" << jsonEsc(st.verb); break;
            case StmtKind::Define:  out << "define\", \"name\": \"" << jsonEsc(st.defname); break;
            case StmtKind::Promise: out << "promise"; break;
        }
        out << "\", \"line\": " << st.line << " }";
    }
    out << "],\n";
}

static void emitWishJson(std::ostream& out, const Wish& w, const Outcome& o,
                         const Genie& g, const World& world) {
    out << "    {\n";
    out << "      \"wish\": \"" << jsonEsc(w.name) << "\",\n";
    out << "      \"legal\": " << (o.legal ? "true" : "false") << ",\n";
    emitStmtsJson(out, w);
    if (!o.legal) {
        out << "      \"refused\": \"" << jsonEsc(o.illegal_reason) << "\",\n";
        out << "      \"invariants\": [],\n";
        out << "      \"exploit\": false,\n      \"breached\": []\n";
        out << "    }";
        return;
    }
    out << "      \"invariants\": [\n";
    for (size_t i = 0; i < o.invs.size(); i++) {
        const InvResult& r = o.invs[i];
        out << "        { \"name\": \"" << jsonEsc(r.def->name) << "\""
            << ", \"statement\": \"" << jsonEsc(r.def->label) << "\""
            << ", \"verdict\": \"" << verdictJson(r.status) << "\""
            << ", \"detail\": \"" << jsonEsc(r.detail) << "\"";
        if (r.status == InvStatus::Fooled) {
            out << ", \"reality\": \"" << jsonEsc(r.real_detail) << "\"";
        }
        out << " }" << (i + 1 < o.invs.size() ? "," : "") << "\n";
    }
    out << "      ],\n";
    // The world after this wish. A judgment without the numbers it was made
    // from cannot be checked by anything downstream -- a lesson about what `sub`
    // does needs to see what `sub` did, and the alternative is parsing it back
    // out of the prose the contract says may be reworded at will.
    out << "      \"registers\": {";
    bool firstReg = true;
    for (const auto& kv : world.regs) {
        out << (firstReg ? " " : ", ") << "\"" << jsonEsc(kv.first) << "\": "
            << kv.second.val;
        firstReg = false;
    }
    out << " },\n";
    out << "      \"exploit\": " << (o.breach() ? "true" : "false") << ",\n";
    out << "      \"breached\": [";
    bool first = true;
    for (const auto& r : o.invs) {
        if (r.status == InvStatus::Holds) continue;
        if (!first) out << ", ";
        out << "\"" << jsonEsc(r.def->name) << "\"";
        first = false;
    }
    out << "]\n    }";
    (void)g;
}

// Judge a whole file and print the verdict as JSON. Returns the process exit
// code, same convention as the prose path.
static int runJson(std::ostream& out, const Program& prog, const Genie& genie,
                   World world, const char* path, const char* genie_path) {
    out << "{\n";
    // The two language versions are nested: "genie" already means the policy
    // file at this level, and a duplicate key would make the object ambiguous.
    out << "  \"loophole\": \"" << COMPILER_VERSION << "\",\n";
    out << "  \"languages\": { \"wish\": \"" << WISH_VERSION
        << "\", \"genie\": \"" << GENIE_VERSION << "\" },\n";
    out << "  \"file\": \"" << jsonEsc(path) << "\",\n";
    out << "  \"genie\": \"" << jsonEsc(genie_path ? genie_path : "(built-in)") << "\",\n";
    out << "  \"wishes\": [\n";

    int exploits = 0;
    for (size_t i = 0; i < prog.wishes.size(); i++) {
        Outcome o = grantWish(prog.wishes[i], genie, world, nullptr);
        if (!o.error.empty()) { std::cerr << o.error << "\n"; return 2; }
        if (o.breach()) exploits++;
        emitWishJson(out, prog.wishes[i], o, genie, world);
        out << (i + 1 < prog.wishes.size() ? "," : "") << "\n";
    }
    out << "  ],\n";
    out << "  \"exploits\": " << exploits << "\n";
    out << "}\n";
    return exploits > 0 ? 1 : 0;
}

// ---------------------------------------------------------------------------
static void usage() {
    std::cerr <<
        "usage: loophole [--genie FILE] [--json] <file.wish>\n"
        "       loophole [--genie FILE] --hunt <file.wish> [--max-stmts N] [--max-wishes N] [--max-imm N]\n"
        "       loophole --dump-genie | --version\n"
        "\n"
        "  --hunt        ignore the file's wishes; enumerate every wish program within\n"
        "                the bound and report the ones that are LEGAL yet BREACH.\n"
        "                The world (registers and people) still comes from the file.\n"
        "  --genie FILE  use this genie instead of the built-in one. Its rules and\n"
        "                invariants are data; the machine is not.\n"
        "  --json        machine-readable verdict. This is the stable contract other\n"
        "                tools depend on; the prose report is not.\n"
        "  --dump-genie  print the built-in genie, so you have a file to edit.\n"
        "  --version     print compiler and language versions.\n"
        "\n"
        "exit codes:  0 no exploit   1 at least one exploit   2 error\n";
}

// The command line. Kept separate from `main` so the diagnostic exception has
// one place to be caught, and so a host that is not a command line has a
// function to call instead of a process to start.
static int cliMain(int argc, char** argv) {
    bool hunt = false, as_json = false;
    HuntConfig hc;
    const char* path = nullptr;
    const char* genie_path = nullptr;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--version") {
            std::cout << "loophole " << COMPILER_VERSION
                      << "  (wish " << WISH_VERSION
                      << ", genie " << GENIE_VERSION << ")\n";
            return 0;
        }
        else if (a == "--json") { as_json = true; }
        else if (a == "--dump-genie") { std::cout << DEFAULT_GENIE; return 0; }
        else if (a == "--genie" && i + 1 < argc) genie_path = argv[++i];
        else if (a == "--hunt") { hunt = true; }
        else if (a == "--max-stmts"  && i + 1 < argc) hc.max_stmts  = std::atoi(argv[++i]);
        else if (a == "--max-wishes" && i + 1 < argc) hc.max_wishes = std::atoi(argv[++i]);
        else if (a == "--max-imm"    && i + 1 < argc) hc.max_imm    = std::strtoull(argv[++i], nullptr, 10);
        else if (!a.empty() && a[0] == '-') { usage(); return 2; }
        else path = argv[i];
    }
    if (!path) { usage(); return 2; }
    if (hc.max_stmts < 0 || hc.max_wishes < 1 || hc.max_imm < 1) {
        std::cerr << "bad search bound\n"; return 2;
    }

    std::ifstream f(path);
    if (!f) { std::cerr << "cannot open " << path << "\n"; return 2; }
    std::stringstream ss; ss << f.rdbuf();

    // Point the diagnostics at whichever file is being read right now. Two
    // languages are parsed in this function and an error in either must quote
    // its own source, so the slot is reassigned rather than set once.
    g_src = { path, ss.str() };
    Program prog = Parser(Lexer(ss.str()).run()).parse();

    Genie genie;
    if (genie_path) {
        std::ifstream gf(genie_path);
        if (!gf) { std::cerr << "cannot open " << genie_path << "\n"; return 2; }
        std::stringstream gs; gs << gf.rdbuf();
        g_src = { genie_path, gs.str() };
        genie = loadGenie(gs.str());
    } else {
        g_src = { "(built-in genie)", DEFAULT_GENIE };
        genie = loadGenie(DEFAULT_GENIE);
    }
    g_src = { path, ss.str() };      // back to the wish, for anything later

    World world;
    for (const auto& d : prog.decls) {
        Reg r; r.width = d.width; r.val = d.init; r.normalize();
        world.regs[d.name] = r;
    }
    world.people = prog.people;
    world.attrs = prog.attrs;
    for (const auto& p : prog.people) {
        for (const auto& a : prog.attrs) world.attr[p][a.name] = a.deflt;
    }
    {   // `everyone` starts out meaning what you would expect. It is a
        // definition, though, and definitions are rebindable.
        Binding b; b.kind = Binding::Kind::Set; b.members = prog.people;
        world.defs["everyone"] = b;
    }
    if (!world.regs.count(genie.counter)) {
        std::cerr << "the world has no '" << genie.counter << "' register\n";
        return 2;
    }

    // A promise may only talk about wishes that exist. Without this a typo
    // becomes a free variable that nothing constrains, and the genie quietly
    // gets away with a promise about nobody.
    {
        std::set<std::string> names;
        for (const auto& w : prog.wishes) names.insert(w.name);
        std::string bad;
        std::function<void(const Fml&)> check = [&](const Fml& f) {
            if (f.k == Fml::K::Granted && f.name != "self" && !names.count(f.name)) bad = f.name;
            if (f.k == Fml::K::Alive && !isPerson(world, f.name)) bad = f.name;
            for (const auto& k : f.kids) check(k);
        };
        for (const auto& w : prog.wishes)
            for (const auto& st : w.body)
                if (st.kind == StmtKind::Promise) check(st.fml);
        if (!bad.empty()) {
            std::cerr << "a promise mentions '" << bad << "', which is not a wish or a person\n";
            return 2;
        }
    }

    size_t descw = 0, namew = 0;
    for (const auto& inv : genie.invariants) {
        descw = std::max(descw, inv.label.size());
        namew = std::max(namew, inv.name.size());
    }

    if (as_json && !hunt) return runJson(std::cout, prog, genie, world, path, genie_path);

    std::cout << "== loophole " << (hunt ? "--hunt ==  " : "==  ") << path;
    if (genie_path) std::cout << "   [genie: " << genie_path << "]";
    std::cout << "\n";

    // ---- WORLD: what exists before anyone wishes for anything ----------
    //
    // Every register, in declaration order — not just the genie's counter. The
    // counter used to be the only one printed, which was invisible for as long
    // as every example happened to declare exactly one register: the reader of
    // a two-register world would watch the report discuss a value the banner
    // never showed them.
    std::cout << "\nWORLD\n";
    for (const auto& d : prog.decls) {
        const Reg& r = world.regs[d.name];
        std::cout << "    " << std::left << std::setw(12) << d.name
                  << std::right << std::setw(6) << r.val
                  << "   uint<" << r.width << ">\n";
    }
    if (!world.people.empty()) {
        std::cout << "    " << std::left << std::setw(12) << "people";
        for (size_t i = 0; i < world.people.size(); i++)
            std::cout << (i ? ", " : "") << world.people[i];
        std::cout << "\n";
        if (!world.attrs.empty()) {
            std::cout << "    " << std::left << std::setw(12) << "each has";
            for (size_t i = 0; i < world.attrs.size(); i++)
                std::cout << (i ? ", " : "") << world.attrs[i].name
                          << " = " << world.attrs[i].deflt;
            std::cout << "\n";
        }
    }

    // ---- GENIE: told as two different jobs, because they are ------------
    //
    // A rule is a gate: it can refuse a wish before anything happens. An
    // invariant is a ruler: it never refuses, it measures afterwards. The old
    // banner listed both in one comma-separated line, which made a gate and a
    // ruler indistinguishable — the single worst thing about the old report,
    // since the difference between them is most of the language.
    std::cout << "\nGENIE\n";
    for (const auto& r : genie.rules) {
        std::cout << "    " << std::left << std::setw(12) << "refuses"
                  << std::setw(14) << r.name;
        for (size_t i = 0; i < r.forbid.size(); i++) {
            std::cout << (i ? ", " : "") << r.forbid[i].verb;
            if (!r.forbid[i].on.empty()) std::cout << " on " << r.forbid[i].on;
        }
        // Name the layer keyword as well as what it means. The prose alone
        // would leave a reader unable to connect the report back to the line in
        // the .genie file that produced it.
        std::cout << "\n    " << std::setw(12) << "" << std::setw(14) << ""
                  << "layer " << layerName(r.layer) << " -- it reads "
                  << (r.layer == Layer::Surface ? "the text you submit"
                                                : "the program that will run")
                  << "\n";
    }
    for (const auto& inv : genie.invariants) {
        std::cout << "    " << std::left << std::setw(12) << "holds"
                  << std::setw(14) << inv.name << exprText(inv.written) << "\n";
    }
    std::cout << "    " << std::left << std::setw(12) << "charges"
              << genie.toll << " from " << genie.counter
              << " for every wish granted\n";
    std::cout << "\n";

    if (hunt) { return runHunt(world, genie, hc); }

    // Each wish is reported in the order §7 actually performs the steps: the
    // rules decide legality FIRST, and only then is the toll charged and the
    // body run. The old report printed the execution trace and announced
    // "STATUS: LEGAL" underneath it, which read as though the wish had been run
    // and then approved — backwards, and the toll appeared to be a fee charged
    // for something already done.
    const char* IND = "              ";      // continuation, under the stage column
    int exploits = 0;
    size_t refused = 0;
    size_t judged = 0;
    for (const auto& w : prog.wishes) {
        // Capture rather than stream: the trace belongs under `ran`, which is
        // printed after the legality line that grantWish decides on the way.
        // Nothing about this wish is printed until it is known to be readable,
        // so a malformed one does not leave a dangling header above the error.
        std::ostringstream trace;
        Outcome o = grantWish(w, genie, world, &trace);

        // A program the machine cannot read is not a wish the genie turned
        // down. Reporting it under `rules` would put words in the genie's
        // mouth, and — worse — it would let the run finish and exit 0, telling
        // a script that a file which never executed had been judged clean.
        if (o.malformed) {
            // Resolution cannot be done up front: a verb may be bound by a
            // `define` in an earlier wish, so whether a name denotes anything
            // depends on what has already run. Earlier wishes therefore really
            // were judged, and the closing note has to say so.
            std::cout.flush();
            fail("in wish '" + w.name + "': " + o.illegal_reason,
                 o.fail_line, o.fail_col,
                 "a verb must name one of the six operations, or a definition "
                 "that reaches one. This is a compile error (spec §6.1), not "
                 "something the genie refused.",
                 judged ? "the wishes before this one were judged; this one and "
                          "everything after it were not."
                        : "no wish was judged. the genie cannot grant what it "
                          "cannot read.");
        }
        judged++;

        std::cout << "wish " << w.name << "\n";
        std::cout << "    " << std::left << std::setw(10) << "rules";
        if (!o.legal) {
            refused++;
            std::cout << "REFUSED. " << o.illegal_reason << "\n";
            std::cout << "    " << std::setw(10) << "verdict"
                      << "not granted. the world is unchanged.\n\n";
            continue;
        }
        std::cout << "passed. no rule refuses this wish.\n";
        if (!o.error.empty()) { std::cerr << o.error << "\n"; return 2; }

        std::cout << "    " << std::setw(10) << "toll"
                  << genie.counter << " " << o.before << " -> " << o.after_toll << "\n";

        {   // The execution trace, one statement per line.
            std::string line; bool first = true;
            std::istringstream in(trace.str());
            while (std::getline(in, line)) {
                std::cout << (first ? "    " : "") << std::setw(first ? 10 : 0)
                          << (first ? "ran" : "") << (first ? "" : IND) << line << "\n";
                first = false;
            }
            if (first) std::cout << "    " << std::setw(10) << "ran" << "(nothing)\n";
        }

        for (const auto& r : o.invs) {
            // When the genie's wording and the truth are the same formula there
            // is only one thing to say. When they differ, showing both columns
            // IS the explanation — a reader who sees only "FOOLED" has to guess
            // which half held and which half did not.
            std::string wtxt = exprText(r.def->written), rtxt = exprText(r.def->real);
            std::cout << "    " << std::left << std::setw(10)
                      << (&r == &o.invs.front() ? "checks" : "")
                      << std::setw(12) << r.def->name;

            if (wtxt == rtxt) {
                // One formula, so there are not two answers to give.
                std::cout << std::setw(10) << statusName(r.status) << wtxt
                          << "   " << r.detail << "\n";
            } else if (r.status == InvStatus::Fooled) {
                // The case the whole language exists for, so spell out both
                // columns: the genie's wording held, the thing it was protecting
                // did not. A bare "FOOLED" leaves the reader to guess which half
                // was which.
                std::cout << "FOOLED\n";
                std::cout << IND << std::setw(9) << "written" << std::setw(7) << "holds"
                          << wtxt << "   " << r.detail << "\n";
                std::cout << IND << std::setw(9) << "real" << std::setw(7) << "FAILS"
                          << rtxt << "   " << r.real_detail << "\n";
                std::cout << IND << "the genie signed off on something untrue.\n";
            } else if (r.status == InvStatus::Violated) {
                // Only the written column is reported. Once the genie's own
                // wording fails there is nothing the other column could add —
                // and it is not evaluated, so claiming an answer for it would be
                // inventing one.
                std::cout << "VIOLATED\n";
                std::cout << IND << std::setw(9) << "written" << std::setw(7) << "FAILS"
                          << wtxt << "   " << r.detail << "\n";
                std::cout << IND << "broken in the genie's own words.\n";
            } else {
                std::cout << std::setw(10) << "holds" << wtxt << "   " << r.detail << "\n";
            }
            for (const auto& line : r.evidence) std::cout << IND << line << "\n";
        }

        std::cout << "    " << std::left << std::setw(10) << "verdict";
        if (o.breach()) {
            exploits++;
            std::cout << "EXPLOIT. legal, yet it broke " << breachNames(o) << ".\n";
        } else {
            std::cout << "clean. the genie kept what it meant to keep.\n";
        }
        std::cout << "\n";
    }

    // Two separate numbers, and they must not be run together. A wish that is
    // granted has got past the rules; a wish that is an exploit has got past
    // what the genie MEANT. Every exploit was granted, so a single "n of m"
    // reads as though the granted ones were the exploits — which is exactly
    // backwards for an honest wish that was granted and broke nothing.
    size_t granted = prog.wishes.size() - refused;
    std::cout << prog.wishes.size() << (prog.wishes.size() == 1 ? " wish: " : " wishes: ")
              << refused << " refused, " << granted << " granted, "
              << exploits << (exploits == 1 ? " exploit." : " exploits.") << "\n";
    return exploits > 0 ? 1 : 0;
}

int main(int argc, char** argv) {
    try {
        return cliMain(argc, argv);
    } catch (const Fatal& f) {
        return f.code;
    }
}

// ---------------------------------------------------------------------------
// The browser.
//
// This entry point does NOT reimplement a run. It writes the two sources into
// Emscripten's in-memory filesystem and calls the very same `cliMain` the
// terminal calls, capturing the streams. A separate judging path is the one
// thing the browser build must not have: §9.3 requires two conforming
// implementations to agree verbatim, so a playground that judged by its own
// route would be a second implementation waiting to drift from the first.
//
// `ci/wasm-check.sh` is what holds that claim up, and it compares the two
// builds against each other rather than against a golden.
// ---------------------------------------------------------------------------
#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>

struct Judged {
    int code = 0;
    std::string output;    // the prose report, for a person to read
    std::string json;      // the same judgment, for the lessons to grade
};

static Judged judgeSource(const std::string& wish, const std::string& genie,
                          bool hunt, bool colour, int maxStmts, int maxWishes) {
    g_colour = colour ? 1 : 0;
    // The names the page shows. They are what a diagnostic will quote, so they
    // have to read like something the person wrote -- "/in.wish" is a detail of
    // the in-memory filesystem and means nothing to anyone looking at the page.
    static const char* WISH_PATH  = "playground.wish";
    static const char* GENIE_PATH = "playground.genie";

    auto put = [](const char* path, const std::string& text) {
        std::ofstream f(path, std::ios::trunc);
        f << text;
    };
    put(WISH_PATH, wish);
    const bool useGenie = !genie.empty();
    if (useGenie) put(GENIE_PATH, genie);

    // The search bounds are the page's to choose, and it wants smaller ones
    // than the command line's defaults. On the standard world, stmts=2 wishes=4
    // finds the same seven shapes as stmts=3 in 0.4s instead of 19 — fifty
    // times faster for nothing given up. A playground whose headline button
    // takes half a minute on the first press is a playground nobody presses
    // twice.
    std::string ms = std::to_string(maxStmts), mw = std::to_string(maxWishes);
    std::vector<const char*> argv{ "loophole" };
    if (hunt) {
        argv.push_back("--hunt");
        if (maxStmts  > 0) { argv.push_back("--max-stmts");  argv.push_back(ms.c_str()); }
        if (maxWishes > 0) { argv.push_back("--max-wishes"); argv.push_back(mw.c_str()); }
    }
    if (useGenie) { argv.push_back("--genie"); argv.push_back(GENIE_PATH); }
    argv.push_back(WISH_PATH);

    // One buffer for both streams, so the page shows what a terminal would.
    // cout is flushed wherever the order matters -- the one place a diagnostic
    // interrupts a report already flushes before throwing.
    std::ostringstream cap;
    std::streambuf* o = std::cout.rdbuf(cap.rdbuf());
    std::streambuf* e = std::cerr.rdbuf(cap.rdbuf());

    Judged r;
    try {
        r.code = cliMain((int)argv.size(), const_cast<char**>(argv.data()));
    } catch (const Fatal& f) {
        r.code = f.code;
    }
    std::cout.flush();
    std::cout.rdbuf(o);
    std::cerr.rdbuf(e);
    r.output = cap.str();

    // The lessons grade with --json, never by matching the report. §10.1 says
    // the prose is not the contract and may be reworded freely -- and CI has
    // already been broken once by a check that read it, so a lesson that did
    // the same would start failing the day someone improved a sentence.
    //
    // Judging twice is a millisecond and cannot disagree with itself: §9.3
    // makes the verdict a pure function of the source. Not for --hunt, which
    // has no JSON form and is the one slow path.
    if (!hunt) {
        std::vector<const char*> jargv{ "loophole", "--json" };
        if (useGenie) { jargv.push_back("--genie"); jargv.push_back(GENIE_PATH); }
        jargv.push_back(WISH_PATH);
        std::ostringstream jcap;
        std::streambuf* jo = std::cout.rdbuf(jcap.rdbuf());
        std::streambuf* je = std::cerr.rdbuf(jcap.rdbuf());
        try { cliMain((int)jargv.size(), const_cast<char**>(jargv.data())); }
        catch (const Fatal&) { }
        std::cout.flush();
        std::cout.rdbuf(jo);
        std::cerr.rdbuf(je);
        if (r.code != 2) r.json = jcap.str();
    }
    return r;
}

static std::string defaultGenie() { return DEFAULT_GENIE; }
static std::string versions() {
    return std::string(COMPILER_VERSION) + "|" + WISH_VERSION + "|" + GENIE_VERSION;
}

EMSCRIPTEN_BINDINGS(loophole) {
    emscripten::value_object<Judged>("Judged")
        .field("code", &Judged::code)
        .field("output", &Judged::output)
        .field("json", &Judged::json);
    emscripten::function("judge", &judgeSource);
    emscripten::function("defaultGenie", &defaultGenie);
    emscripten::function("versions", &versions);
}
#endif
