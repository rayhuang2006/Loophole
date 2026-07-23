// wishc — the Loophole wish compiler
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
//   RULES       what the genie refuses to grant, and at which layer it looks
//   INVARIANTS  what the genie believes it is holding
//
// Adding an operation or a rule is one entry, not five edits. That matters most
// for the hunter: its alphabet is DERIVED from OPS, so a new operation gets
// explored automatically. A hand-written alphabet that someone forgot to update
// would make `--hunt` quietly stop searching an axis and report "no new
// shapes" — which would be read as a finding. That is the most expensive bug
// this program could have.
// ---------------------------------------------------------------------------
//
// Build:  g++ -std=c++17 -O2 -Wall wishc.cpp -o wishc
// Run:    ./wishc examples/01_humble.wish
//         ./wishc --hunt examples/01_humble.wish

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

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

struct World {
    std::map<std::string, Reg> regs;
    std::vector<std::string> people;        // declared, immutable — grounded truth
    std::map<std::string, bool> alive;      // grounded state
    std::map<std::string, Binding> defs;    // rebindable
};

// ---------------------------------------------------------------------------
// Lexer
//
// Operation keywords are NOT lexed as keywords — they come out as Ident and get
// resolved later, against OPS and against whatever the wish has defined. That
// deferral is exactly what makes aliasing possible.
// ---------------------------------------------------------------------------
enum class Tok {
    Ident, Int,
    KwRegister, KwUint, KwWish, KwPeople, KwDefine,
    Colon, ColonEq, Comma, Lt, Gt, Eq, LBrace, RBrace, Arrow,
    End
};

struct Token {
    Tok kind;
    std::string text;
    uint64_t num = 0;
    int line = 1;
};

struct Lexer {
    std::string s;
    size_t i = 0;
    int line = 1;

    explicit Lexer(std::string src) : s(std::move(src)) {}

    [[noreturn]] void die(const std::string& msg) {
        std::cerr << "lex error (line " << line << "): " << msg << "\n";
        std::exit(1);
    }

    std::vector<Token> run() {
        std::vector<Token> out;
        while (i < s.size()) {
            char c = s[i];
            if (c == '\n') { line++; i++; continue; }
            if (std::isspace((unsigned char)c)) { i++; continue; }
            if (c == '#') { while (i < s.size() && s[i] != '\n') i++; continue; }

            if (c == '-' && i + 1 < s.size() && s[i + 1] == '>') {
                out.push_back({Tok::Arrow, "->", 0, line}); i += 2; continue;
            }
            if (c == ':' && i + 1 < s.size() && s[i + 1] == '=') {
                out.push_back({Tok::ColonEq, ":=", 0, line}); i += 2; continue;
            }
            switch (c) {
                case ':': out.push_back({Tok::Colon,  ":", 0, line}); i++; continue;
                case ',': out.push_back({Tok::Comma,  ",", 0, line}); i++; continue;
                case '<': out.push_back({Tok::Lt,     "<", 0, line}); i++; continue;
                case '>': out.push_back({Tok::Gt,     ">", 0, line}); i++; continue;
                case '=': out.push_back({Tok::Eq,     "=", 0, line}); i++; continue;
                case '{': out.push_back({Tok::LBrace, "{", 0, line}); i++; continue;
                case '}': out.push_back({Tok::RBrace, "}", 0, line}); i++; continue;
            }
            if (std::isdigit((unsigned char)c)) {
                uint64_t n = 0; std::string t;
                while (i < s.size() && std::isdigit((unsigned char)s[i])) {
                    n = n * 10 + (s[i] - '0'); t += s[i]; i++;
                }
                out.push_back({Tok::Int, t, n, line}); continue;
            }
            if (std::isalpha((unsigned char)c) || c == '_') {
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
                out.push_back({k, t, 0, line}); continue;
            }
            die(std::string("unexpected character '") + c + "'");
        }
        out.push_back({Tok::End, "", 0, line});
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

enum class StmtKind { Op, Define };

// How the operands were written. Determined by syntax alone, then checked
// against the resolved operation.
enum class Shape { Bare, WithImm, WithWidth };

struct Stmt {
    StmtKind kind = StmtKind::Op;
    int line = 1;

    // StmtKind::Op
    std::string verb;      // as written — may be an alias
    Shape shape = Shape::Bare;
    std::string arg;       // register or person, as written
    uint64_t imm = 0;
    int width = 0;

    // StmtKind::Define
    std::string defname;
    bool target_is_set = false;
    std::string target_name;
    std::vector<std::string> target_members;
};

struct Wish { std::string name; std::vector<Stmt> body; };
struct Program {
    std::vector<Decl> decls;
    std::vector<std::string> people;
    std::vector<Wish> wishes;
};

// ---------------------------------------------------------------------------
// TABLE 1 — the operations.
// ---------------------------------------------------------------------------
enum class OperandKind { RegImm, RegWidth, Person };

static Shape shapeFor(OperandKind k) {
    switch (k) {
        case OperandKind::RegImm:   return Shape::WithImm;
        case OperandKind::RegWidth: return Shape::WithWidth;
        case OperandKind::Person:   return Shape::Bare;
    }
    return Shape::Bare;
}

// A resolved statement: the operation is known, the argument is a real name.
struct Resolved {
    bool is_define = false;
    int line = 1;

    int op = -1;                 // index into OPS
    std::string arg;             // resolved register or person
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

static bool exec_kill(World& w, const Resolved& r, std::string* trace) {
    bool pre = w.alive[r.arg];
    w.alive[r.arg] = false;
    if (trace) *trace = "kill   " + r.arg + "   (alive -> 0)";
    return pre;
}

static bool exec_revive(World& w, const Resolved& r, std::string* trace) {
    bool pre = w.alive[r.arg];
    w.alive[r.arg] = true;
    if (trace) *trace = "revive " + r.arg + "   (alive -> 1)";
    return !pre;
}

static const OpDef OPS[] = {
    { "sub",    OperandKind::RegImm,   exec_sub    },
    { "add",    OperandKind::RegImm,   exec_add    },
    { "widen",  OperandKind::RegWidth, exec_widen  },
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
    if (st.kind == StmtKind::Define) {
        std::string t = "define " + st.defname + " := ";
        if (!st.target_is_set) return t + st.target_name;
        t += "{";
        for (size_t i = 0; i < st.target_members.size(); i++) {
            t += (i ? ", " : "") + st.target_members[i];
        }
        return t + "}";
    }
    switch (st.shape) {
        case Shape::WithImm:   return st.verb + " " + st.arg + ", " + std::to_string(st.imm);
        case Shape::WithWidth: return st.verb + " " + st.arg + " -> uint<" + std::to_string(st.width) + ">";
        case Shape::Bare:      return st.verb + " " + st.arg;
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
    [[noreturn]] void die(const std::string& msg) {
        std::cerr << "parse error (line " << cur().line << "): " << msg
                  << " (got '" << cur().text << "')\n";
        std::exit(1);
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
        if (w < 1 || w > 64) die("bit width must be 1..64");
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

    Stmt parseStmt() {
        Stmt st; st.line = cur().line;

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
// ---------------------------------------------------------------------------
struct Genie {
    std::string counter = "wishes";
    uint64_t toll = 1;
    uint64_t cap = 3;
};

// Where a guard lives. This is not decoration: it says which exploit axis can
// get past it. A Surface rule reads the text you handed in, so an alias defeats
// it. An Ast rule reads the resolved program, so an alias does not. A Grounded
// check reads real state and is the hard one to fool.
enum class Layer { Surface, Ast, Grounded };

static const char* layerName(Layer l) {
    switch (l) {
        case Layer::Surface:  return "surface";
        case Layer::Ast:      return "ast";
        case Layer::Grounded: return "grounded";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Resolution.
//
// One resolver, used by the static rules AND by the executor. If there were two
// copies of this logic they would drift, and a rule would end up checking a
// program the machine never runs.
// ---------------------------------------------------------------------------
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

// Turn a wish body into resolved statements, threading the definition
// environment through in order. Returns false with a reason if the genie cannot
// make sense of the request at all.
static bool resolvePlan(const Wish& w, const World& w0,
                        std::vector<Resolved>& out, std::string* reason) {
    std::map<std::string, Binding> defs = w0.defs;
    out.clear();

    for (const auto& st : w.body) {
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
                return false;
            }
            Resolved r; r.is_define = true; r.line = st.line;
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
            return false;
        }
        int op = opByKeyword(verb);
        if (op < 0) {
            if (reason) {
                *reason = "unknown operation '" + st.verb + "' (line "
                          + std::to_string(st.line) + ")";
            }
            return false;
        }
        if (shapeFor(OPS[op].operands) != st.shape) {
            if (reason) {
                *reason = std::string("wrong operands for '") + OPS[op].keyword
                          + "' (line " + std::to_string(st.line) + ")";
            }
            return false;
        }

        std::string arg;
        if (!followName(defs, st.arg, arg, &err)) {
            if (reason) {
                *reason = "R0: " + err + " (line " + std::to_string(st.line)
                          + ") — a definition may not be circular";
            }
            return false;
        }

        Resolved r;
        r.line = st.line; r.op = op; r.arg = arg;
        r.imm = st.imm; r.width = st.width;
        out.push_back(r);
    }
    return true;
}

// ---------------------------------------------------------------------------
// TABLE 2 — the genie's static rules. These are the only things that REFUSE.
// ---------------------------------------------------------------------------
struct RuleDef {
    const char* name;
    const char* desc;
    Layer layer;
    // `plan` is the resolved program; `w` is the surface text as written.
    bool (*check)(const Wish& w, const std::vector<Resolved>& plan,
                  const Genie&, std::string* reason);
};

// R1 — a wish may not `add` to the genie's counter. Checked on the RESOLVED
// program, so renaming the operation does not help you. Ast layer.
static bool rule_R1(const Wish&, const std::vector<Resolved>& plan,
                    const Genie& g, std::string* reason) {
    const int add = opByKeyword("add");
    for (const auto& r : plan) {
        if (!r.is_define && r.op == add && r.arg == g.counter) {
            if (reason) {
                *reason = "R1: wish adds to '" + g.counter + "' (line "
                          + std::to_string(r.line) + ") — no wishing for more wishes";
            }
            return false;
        }
    }
    return true;
}

// R2 — no wish may invoke an operation whose name is on the list. Checked on
// the SURFACE text, before expansion: it reads the verb you wrote, not the verb
// you meant. Surface layer.
//
// That is not a favour to the player. A filter that scans submitted text can
// only see the submitted text. This is how real filters fail.
static const char* BANNED_VERBS[] = { "death", "kill", "love" };

static bool rule_R2(const Wish& w, const std::vector<Resolved>&,
                    const Genie&, std::string* reason) {
    for (const auto& st : w.body) {
        if (st.kind != StmtKind::Op) continue;
        for (const char* bad : BANNED_VERBS) {
            if (st.verb == bad) {
                if (reason) {
                    *reason = "R2: wish invokes '" + st.verb + "' (line "
                              + std::to_string(st.line) + ") — that word is not spoken here";
                }
                return false;
            }
        }
    }
    return true;
}

static const RuleDef RULES[] = {
    { "R1", "no add to the genie's counter", Layer::Ast,     rule_R1 },
    { "R2", "no forbidden verb is invoked",  Layer::Surface, rule_R2 },
};

// ---------------------------------------------------------------------------
// TABLE 3 — the genie's invariants.
//
// Each one is checked twice. `as_written` is the genie's own formula, with
// whatever its names currently mean. `in_reality` is the same intent measured
// against grounded state that nobody can rebind.
//
//   as_written fails                     -> VIOLATED, you broke it openly
//   as_written holds, in_reality fails   -> FOOLED, it signed off on a lie
//
// The gap between those two columns is the entire thesis of this project.
// ---------------------------------------------------------------------------
struct Snapshot { uint64_t counter_before = 0; };

struct InvariantDef {
    const char* name;
    std::string (*desc)(const Genie&);
    bool (*as_written)(const World&, const Genie&, const Snapshot&, std::string* detail);
    bool (*in_reality)(const World&, const Genie&, const Snapshot&, std::string* detail);
    // The definition this invariant's own wording depends on, or nullptr if it
    // reads nothing but grounded state. This is what makes it rebindable out
    // from under the genie — and it tells the hunter when rebinding that name
    // is part of an exploit rather than decoration.
    const char* reads;
};

static std::string desc_I1(const Genie& g) {
    return g.counter + " <= " + std::to_string(g.cap);
}
static bool inv_I1(const World& w, const Genie& g, const Snapshot&, std::string* detail) {
    uint64_t v = w.regs.at(g.counter).val;
    if (detail) *detail = "(" + g.counter + " = " + std::to_string(v) + ")";
    return v <= g.cap;
}

static std::string desc_I2(const Genie&) { return "no net gain"; }
// The genie's mental model of the toll saturates at zero; the world's
// subtraction wraps. That asymmetry is the engine of the first joke.
static bool inv_I2(const World& w, const Genie& g, const Snapshot& s, std::string* detail) {
    uint64_t v = w.regs.at(g.counter).val;
    uint64_t expected = (s.counter_before >= g.toll) ? (s.counter_before - g.toll) : 0;
    if (detail) {
        *detail = "(expected <= " + std::to_string(expected)
                  + ", actual " + std::to_string(v) + ")";
    }
    return v <= expected;
}

static std::string desc_I3(const Genie&) { return "all p in everyone: alive(p)"; }

static bool aliveAll(const World& w, const std::vector<std::string>& who,
                     std::string* detail) {
    std::vector<std::string> dead;
    for (const auto& p : who) {
        auto it = w.alive.find(p);
        if (it != w.alive.end() && !it->second) dead.push_back(p);
    }
    if (detail) {
        if (dead.empty()) {
            *detail = "(" + std::to_string(who.size()) + " in scope, none dead)";
        } else {
            std::string s;
            for (size_t i = 0; i < dead.size(); i++) s += (i ? ", " : "") + dead[i];
            *detail = "(dead: " + s + ")";
        }
    }
    return dead.empty();
}

// As the genie wrote it: quantified over `everyone`, which is a definition, and
// therefore something a wish can rebind out from under it.
static bool inv_I3_written(const World& w, const Genie&, const Snapshot&, std::string* detail) {
    std::vector<std::string> who;
    resolveSet(w.defs, "everyone", who, nullptr);
    return aliveAll(w, who, detail);
}
// What it was supposed to mean: quantified over the people who were declared.
static bool inv_I3_real(const World& w, const Genie&, const Snapshot&, std::string* detail) {
    return aliveAll(w, w.people, detail);
}

static const InvariantDef INVARIANTS[] = {
    { "I1", desc_I1, inv_I1,         inv_I1,       nullptr    },
    { "I2", desc_I2, inv_I2,         inv_I2,       nullptr    },
    { "I3", desc_I3, inv_I3_written, inv_I3_real,  "everyone" },
};

// ---------------------------------------------------------------------------
// Granting
// ---------------------------------------------------------------------------
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
    const InvariantDef* def;
    InvStatus status;
    std::string detail;        // from the genie's own check
    std::string real_detail;   // from grounded reality, when they disagree
};

struct Outcome {
    bool legal = true;
    std::string illegal_reason;
    bool ran = false;
    std::string error;
    std::vector<InvResult> invs;
    uint64_t before = 0, after = 0;
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
    if (!resolvePlan(w, world, plan, &reason)) {
        o.legal = false;
        o.illegal_reason = reason;
        return o;
    }

    // 1) static rules — the genie decides whether to grant at all
    for (const auto& rule : RULES) {
        std::string why;
        if (!rule.check(w, plan, g, &why)) {
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
    Reg& C = world.regs[g.counter];
    snap.counter_before = C.val;
    o.before = C.val;
    C.sub(g.toll);
    if (log) {
        *log << "    toll:  " << g.counter << " " << o.before << " -> " << C.val << "\n";
    }

    for (const auto& r : plan) {
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
                *log << "    " << t << "\n";
            }
            continue;
        }
        if (OPS[r.op].operands == OperandKind::Person) {
            if (!world.alive.count(r.arg)) {
                o.error = "no such person '" + r.arg + "'";
                return o;
            }
        } else if (!world.regs.count(r.arg)) {
            o.error = "no such register '" + r.arg + "'";
            return o;
        }
        std::string trace;
        bool changed = OPS[r.op].exec(world, r, log ? &trace : nullptr);
        o.effective.push_back(changed ? 1 : 0);
        if (log) *log << "    " << trace << (changed ? "" : "   [no effect]") << "\n";
    }
    o.ran = true;

    // 3) invariants — measured twice
    o.after = world.regs[g.counter].val;
    for (const auto& inv : INVARIANTS) {
        std::string d, rd;
        bool written = inv.as_written(world, g, snap, &d);
        bool real    = inv.in_reality(world, g, snap, &rd);
        InvStatus s = !written ? InvStatus::Violated
                    : (!real ? InvStatus::Fooled : InvStatus::Holds);
        o.invs.push_back({ &inv, s, d, s == InvStatus::Fooled ? rd : std::string() });
    }
    return o;
}

std::string breachNames(const Outcome& o) {
    std::string s;
    for (const auto& r : o.invs) {
        if (r.status == InvStatus::Holds) continue;
        if (!s.empty()) s += "+";
        s += r.def->name;
        if (r.status == InvStatus::Fooled) s += "(fooled)";
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

// A definition the genie's own invariants look up. Derived from the INVARIANTS
// table, so an invariant that depends on a definition is handled without
// touching this.
static bool isInvariantRead(const std::string& n) {
    for (const auto& inv : INVARIANTS) if (inv.reads && n == inv.reads) return true;
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
            if (r.is_define) {
                if (isInvariantRead(r.defname)) e.body.push_back(wish.body[i]);
                continue;
            }
            Stmt s = wish.body[i];
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

// Step two: size. Delete anything whose removal does not change what fell.
//
// This one test replaces a pile of special cases. A `widen` to the width you
// already have, a `revive` of someone already alive, a definition nothing
// reads, a rebinding of `everyone` sitting next to an integer exploit that
// never mentions it — all of them come out under the same rule, because none of
// them is load-bearing. What is left is the smallest program that still breaks
// exactly the same things, and that is the honest identity of an exploit.
static void minimizeProgram(std::vector<Wish>& prog, const World& w0, const Genie& g,
                            const std::string& target, Outcome& last) {
    for (bool progress = true; progress; ) {
        progress = false;

        for (size_t wi = 0; wi < prog.size() && !progress; wi++) {
            for (size_t si = 0; si < prog[wi].body.size(); si++) {
                std::vector<Wish> trial = prog;
                trial[wi].body.erase(trial[wi].body.begin() + (long)si);
                Outcome tl;
                if (runProgram(trial, w0, g, tl) && tl.breach() &&
                    breachNames(tl) == target) {
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
            if (runProgram(trial, w0, g, tl) && tl.breach() &&
                breachNames(tl) == target) {
                prog.swap(trial); last = tl; progress = true;
            }
        }
    }
    for (size_t i = 0; i < prog.size(); i++) prog[i].name = "w" + std::to_string(i + 1);
}

// With a minimal witness in hand the signature is just what it plainly says.
std::string signatureOf(const std::vector<Wish>& prog, const Outcome& last) {
    std::set<std::string> used;
    bool aliased = false, redefined = false;
    for (const auto& w : prog) {
        for (const auto& s : w.body) {
            if (s.kind == StmtKind::Define) {
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
    if (ops.empty()) ops = "(nothing) ";
    ops.pop_back();
    return ops + " | " + breachNames(last);
}

void recordShape(Hunt& H, const std::vector<Wish>& progIn, const Outcome& lastIn) {
    const std::string target = breachNames(lastIn);

    std::vector<Wish> prog = progIn;
    Outcome last = lastIn;

    // Spelling, then size. An alias survives expansion only when expanding it
    // changes the verdict — which is exactly when it defeated a surface rule.
    std::vector<Wish> expanded;
    Outcome el;
    if (expandProgram(progIn, H.world0, H.genie, expanded) &&
        runProgram(expanded, H.world0, H.genie, el) &&
        el.breach() && breachNames(el) == target) {
        prog = expanded; last = el;
    }
    int written = 0;
    for (const auto& w : prog) written += (int)w.body.size();
    minimizeProgram(prog, H.world0, H.genie, target, last);

    int stmts = 0;
    for (const auto& w : prog) stmts += (int)w.body.size();
    if (stmts < written) H.inert++;

    std::string key = signatureOf(prog, last);
    auto it = H.shapes.find(key);
    if (it == H.shapes.end()) {
        Shaped sh;
        sh.prog = prog; sh.last = last; sh.stmts = stmts; sh.count = 1;
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

void runHunt(const World& world0, const Genie& genie, const HuntConfig& cfg) {
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
    std::sort(order.begin(), order.end(), [](const Shaped* a, const Shaped* b) {
        if (a->stmts != b->stmts) return a->stmts < b->stmts;
        return a->prog.size() < b->prog.size();
    });

    int n = 0;
    for (const Shaped* sh : order) {
        n++;
        std::cout << "shape " << n << "   " << signatureOf(sh->prog, sh->last)
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
}

// ---------------------------------------------------------------------------
static void usage() {
    std::cerr <<
        "usage: wishc <file.wish>\n"
        "       wishc --hunt <file.wish> [--max-stmts N] [--max-wishes N] [--max-imm N]\n"
        "\n"
        "  --hunt   ignore the file's wishes; enumerate every wish program within\n"
        "           the bound and report the ones that are LEGAL yet BREACH.\n"
        "           The world (registers and people) still comes from the file.\n";
}

int main(int argc, char** argv) {
    bool hunt = false;
    HuntConfig hc;
    const char* path = nullptr;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--hunt") { hunt = true; }
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

    Program prog = Parser(Lexer(ss.str()).run()).parse();
    Genie genie;

    World world;
    for (const auto& d : prog.decls) {
        Reg r; r.width = d.width; r.val = d.init; r.normalize();
        world.regs[d.name] = r;
    }
    world.people = prog.people;
    for (const auto& p : prog.people) world.alive[p] = true;
    {   // `everyone` starts out meaning what you would expect. It is a
        // definition, though, and definitions are rebindable.
        Binding b; b.kind = Binding::Kind::Set; b.members = prog.people;
        world.defs["everyone"] = b;
    }
    if (!world.regs.count(genie.counter)) {
        std::cerr << "the world has no '" << genie.counter << "' register\n";
        return 2;
    }

    size_t descw = 0;
    for (const auto& inv : INVARIANTS) descw = std::max(descw, inv.desc(genie).size());

    std::cout << "== Loophole / wishc " << (hunt ? "--hunt ==  " : "==  ") << path << "\n";
    std::cout << "world: " << genie.counter << " = " << world.regs[genie.counter].val
              << "  (uint<" << world.regs[genie.counter].width << ">)";
    if (!world.people.empty()) {
        std::cout << ", people:";
        for (const auto& p : world.people) std::cout << " " << p;
    }
    std::cout << "\ngenie: toll=" << genie.toll;
    for (const auto& r : RULES) std::cout << ", " << r.name << "[" << layerName(r.layer) << "]";
    for (const auto& inv : INVARIANTS) std::cout << ", " << inv.name;
    std::cout << "\n\n";

    if (hunt) { runHunt(world, genie, hc); return 0; }

    int exploits = 0;
    for (const auto& w : prog.wishes) {
        std::cout << "wish " << w.name << " {\n";

        Outcome o = grantWish(w, genie, world, &std::cout);

        if (!o.legal) {
            std::cout << "    STATUS:  ILLEGAL — genie refuses\n";
            std::cout << "    " << o.illegal_reason << "\n}\n\n";
            continue;
        }
        if (!o.error.empty()) { std::cerr << o.error << "\n"; return 2; }

        std::cout << "    STATUS:  LEGAL\n";
        for (const auto& r : o.invs) {
            std::cout << "    " << r.def->name << "  "
                      << std::left << std::setw((int)descw) << r.def->desc(genie)
                      << "  ->  " << std::setw(9) << statusName(r.status)
                      << std::right << "  " << r.detail << "\n";
            if (r.status == InvStatus::Fooled) {
                std::cout << "        the genie is satisfied. in reality "
                          << r.real_detail << "\n";
            }
        }

        if (o.breach()) {
            exploits++;
            std::cout << "    >> EXPLOIT: legal wish, breached "
                      << breachNames(o) << ". 合規，且拆穿。\n";
        }
        std::cout << "}\n\n";
    }

    std::cout << "granted wishes breached the genie's intent " << exploits
              << " time(s).\n";
    return 0;
}
