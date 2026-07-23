// wishc — the Loophole wish compiler (Phase 1)
//
// Pipeline:  source (.wish) -> lexer -> parser -> AST
//                           -> genie's static rule check (before granting)
//                           -> execute under fixed-width integer semantics
//                           -> invariant check (after granting)
//
// A wish is a successful EXPLOIT iff it is LEGAL (passes the genie's static
// rules) yet BREACHES one of the genie's invariants. "合規，且拆穿。"
//
// Phase 1 scope: register decls, wish blocks, `sub`/`add`/`widen`.
// Hand-rolled lexer + recursive-descent parser, no external libraries.
//
// There is also a hunter (`--hunt`): it enumerates every wish program within a
// bound and reports the ones that come out LEGAL yet BREACH. That is the point
// of this project stated mechanically — you do not have to think of the exploit
// for it to exist, because it was never put there. It follows from the
// semantics, so a machine can go and find it.
//
// Build:  g++ -std=c++17 -O2 -Wall wishc.cpp -o wishc
// Run:    ./wishc examples/01_humble.wish
//         ./wishc --hunt examples/01_humble.wish

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Fixed-width unsigned value. This is where the whole joke lives: subtraction
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

// ---------------------------------------------------------------------------
// Lexer
// ---------------------------------------------------------------------------
enum class Tok {
    Ident, Int,
    KwRegister, KwUint, KwWish, KwSub, KwAdd, KwWiden,
    Colon, Comma, Lt, Gt, Eq, LBrace, RBrace, Arrow,
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
                if (t == "register") k = Tok::KwRegister;
                else if (t == "uint") k = Tok::KwUint;
                else if (t == "wish") k = Tok::KwWish;
                else if (t == "sub")  k = Tok::KwSub;
                else if (t == "add")  k = Tok::KwAdd;
                else if (t == "widen") k = Tok::KwWiden;
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
// ---------------------------------------------------------------------------
struct Decl { std::string name; int width; uint64_t init; };

enum class Op { Sub, Add, Widen };
struct Stmt {
    Op op;
    std::string reg;
    uint64_t imm = 0;   // for Sub/Add
    int width = 0;      // for Widen
    int line = 1;
};

struct Wish { std::string name; std::vector<Stmt> body; };
struct Program { std::vector<Decl> decls; std::vector<Wish> wishes; };

// Surface form of a statement, as it would be written in a .wish file.
std::string stmtText(const Stmt& st) {
    if (st.op == Op::Widen) {
        return "widen " + st.reg + " -> uint<" + std::to_string(st.width) + ">";
    }
    return std::string(st.op == Op::Sub ? "sub " : "add ")
           + st.reg + ", " + std::to_string(st.imm);
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
        while (cur().kind == Tok::KwRegister) {
            eat(Tok::KwRegister, "'register'");
            std::string name = eat(Tok::Ident, "register name").text;
            eat(Tok::Colon, "':'");
            int w = parseWidth();
            eat(Tok::Eq, "'='");
            uint64_t init = eat(Tok::Int, "initial value").num;
            prog.decls.push_back({name, w, init});
        }
        while (cur().kind == Tok::KwWish) {
            eat(Tok::KwWish, "'wish'");
            Wish wsh;
            wsh.name = eat(Tok::Ident, "wish name").text;
            eat(Tok::LBrace, "'{'");
            while (cur().kind != Tok::RBrace) {
                wsh.body.push_back(parseStmt());
            }
            eat(Tok::RBrace, "'}'");
            prog.wishes.push_back(std::move(wsh));
        }
        if (cur().kind != Tok::End) die("trailing tokens");
        return prog;
    }

    Stmt parseStmt() {
        Stmt st; st.line = cur().line;
        if (cur().kind == Tok::KwSub || cur().kind == Tok::KwAdd) {
            st.op = (cur().kind == Tok::KwSub) ? Op::Sub : Op::Add;
            p++;
            st.reg = eat(Tok::Ident, "register name").text;
            eat(Tok::Comma, "','");
            st.imm = eat(Tok::Int, "immediate").num;
            return st;
        }
        if (cur().kind == Tok::KwWiden) {
            st.op = Op::Widen; p++;
            st.reg = eat(Tok::Ident, "register name").text;
            eat(Tok::Arrow, "'->'");
            st.width = parseWidth();
            return st;
        }
        die("expected a statement (sub / add / widen)");
    }
};

// ---------------------------------------------------------------------------
// The genie: its rules, its invariants, and the granting procedure.
// (Hardcoded for v0 — Phase 3 makes these a loadable policy.)
// ---------------------------------------------------------------------------
struct Genie {
    std::string counter = "wishes";  // the register the genie tolls
    uint64_t toll = 1;               // charged before each wish body
    uint64_t cap = 3;                // invariant I1: counter <= cap
};

struct World { std::map<std::string, Reg> regs; };

struct Outcome {
    bool legal = true;           // passed static rules?
    std::string illegal_reason;
    bool ran = false;            // body executed to completion?
    std::string unknown_reg;     // set if the body touched a register that isn't there
    bool i1_ok = true;           // capacity: counter <= cap
    bool i2_ok = true;           // monotonicity: didn't end richer than the toll allows
    uint64_t before = 0, after = 0, expected_max = 0;

    bool breach() const { return legal && ran && (!i1_ok || !i2_ok); }
};

// Static rule R1: a wish body may not `add` to the genie's counter.
// ("No wishing for more wishes.") This is the genie's ONLY real guard.
//
// Note it matches on the AST (`Op::Add`), not on tokens — so it is a semantic
// rule, not a lexical one. That distinction is going to matter a lot once
// definitions can be rebound.
bool staticCheck(const Wish& w, const Genie& g, std::string& reason) {
    for (const auto& st : w.body) {
        if (st.op == Op::Add && st.reg == g.counter) {
            reason = "R1: wish adds to '" + g.counter + "' (line "
                     + std::to_string(st.line) + ") — no wishing for more wishes";
            return false;
        }
    }
    return true;
}

// The single source of truth for what granting a wish does. The normal compile
// path and the hunter both go through here. If they didn't, a hole the hunter
// reported would not be a hole the compiler agrees with, and the whole claim
// ("the machine found this, not me") would be worth nothing.
//
// `log`, when non-null, receives the human-readable trace of the granting.
Outcome grantWish(const Wish& w, const Genie& g, World& world, std::ostream* log) {
    Outcome o;

    // 1) static rule check — genie decides whether to grant at all
    o.legal = staticCheck(w, g, o.illegal_reason);
    if (!o.legal) return o;

    // 2) grant: apply toll, then run the body.
    //
    // The toll goes through the very same wrapping subtraction as everything
    // else. Nothing anywhere checks whether you still have a wish to spend.
    Reg& C = world.regs[g.counter];
    o.before = C.val;
    C.sub(g.toll);
    if (log) {
        *log << "    toll:  " << g.counter << " " << o.before
             << " -> " << C.val << "\n";
    }

    for (const auto& st : w.body) {
        auto it = world.regs.find(st.reg);
        if (it == world.regs.end()) { o.unknown_reg = st.reg; return o; }
        Reg& R = it->second;
        uint64_t pre = R.val;
        switch (st.op) {
            case Op::Sub:
                R.sub(st.imm);
                if (log) {
                    *log << "    sub    " << st.reg << ", " << st.imm
                         << "   (" << pre << " - " << st.imm << " on uint<"
                         << R.width << "> = " << R.val << ")\n";
                }
                break;
            case Op::Add:
                R.add(st.imm);
                if (log) {
                    *log << "    add    " << st.reg << ", " << st.imm
                         << "   -> " << R.val << "\n";
                }
                break;
            case Op::Widen:
                R.widen(st.width);
                if (log) {
                    *log << "    widen  " << st.reg << " -> uint<" << st.width
                         << ">   (value preserved: " << R.val << ")\n";
                }
                break;
        }
    }
    o.ran = true;

    // 3) invariant check — after granting
    o.after = world.regs[g.counter].val;
    o.i1_ok = (o.after <= g.cap);
    // I2: after paying the toll you cannot end richer. Baseline = before-toll
    // computed WITHOUT wraparound (the genie's naive mental model).
    o.expected_max = (o.before >= g.toll) ? (o.before - g.toll) : 0;
    o.i2_ok = (o.after <= o.expected_max);
    return o;
}

std::string breachNames(const Outcome& o) {
    std::string s;
    if (!o.i1_ok) s += "I1";
    if (!o.i1_ok && !o.i2_ok) s += "+";
    if (!o.i2_ok) s += "I2";
    return s;
}

// ---------------------------------------------------------------------------
// The hunter.
//
// Enumerate every wish program inside a bound and keep the ones that are LEGAL
// yet BREACH. Two prunings make the results worth reading:
//
//   - an illegal wish is dropped rather than extended (a program containing a
//     refused wish is never more interesting than the same program without it);
//   - a breaching branch is not extended, so every exploit reported is minimal:
//     the breach happens on its last wish, and no prefix of it already breached.
//
// Reported exploits are then grouped by shape (which ops were used, which
// invariants fell), because what the author actually wants to know is not "how
// many" but "how many KINDS, and did I think of them all".
// ---------------------------------------------------------------------------
struct HuntConfig {
    int max_stmts = 3;
    int max_wishes = 4;
    uint64_t max_imm = 4;
    std::vector<int> widths{1, 2, 4, 8, 16, 32, 64};
};

struct Shape {
    std::vector<Wish> prog;
    Outcome last;
    int stmts = 0;
    long long count = 0;   // how many exploits share this shape
};

struct Hunt {
    Genie genie;
    HuntConfig cfg;
    std::vector<Stmt> alpha;
    std::map<std::string, Shape> shapes;
    long long searched = 0;
    long long found = 0;
};

std::vector<Stmt> buildAlphabet(const std::string& reg, const HuntConfig& cfg) {
    std::vector<Stmt> a;
    // imm starts at 1: `sub r, 0` and `add r, 0` are no-ops that only pad
    // programs without ever changing an outcome.
    for (uint64_t k = 1; k <= cfg.max_imm; k++) {
        Stmt s; s.reg = reg; s.imm = k;
        s.op = Op::Sub; a.push_back(s);
        s.op = Op::Add; a.push_back(s);
    }
    for (int w : cfg.widths) {
        Stmt s; s.reg = reg; s.op = Op::Widen; s.width = w;
        a.push_back(s);
    }
    return a;
}

std::string signatureOf(const std::vector<Wish>& prog, const Outcome& last) {
    bool sub = false, add = false, wid = false;
    for (const auto& w : prog) {
        for (const auto& s : w.body) {
            if (s.op == Op::Sub) sub = true;
            else if (s.op == Op::Add) add = true;
            else wid = true;
        }
    }
    std::string ops;
    if (sub) ops += "sub ";
    if (add) ops += "add ";
    if (wid) ops += "widen ";
    if (ops.empty()) ops = "(nothing) ";
    ops.pop_back();
    return ops + " | " + breachNames(last);
}

void recordShape(Hunt& H, const std::vector<Wish>& prog, const Outcome& last) {
    int stmts = 0;
    for (const auto& w : prog) stmts += (int)w.body.size();

    std::string key = signatureOf(prog, last);
    auto it = H.shapes.find(key);
    if (it == H.shapes.end()) {
        Shape sh; sh.prog = prog; sh.last = last; sh.stmts = stmts; sh.count = 1;
        H.shapes.emplace(key, std::move(sh));
        return;
    }
    it->second.count++;
    // keep the smallest witness: fewest statements, then fewest wishes
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
    H.cfg = cfg;
    H.alpha = buildAlphabet(genie.counter, cfg);

    std::cout << "search: <= " << cfg.max_stmts << " statements, <= "
              << cfg.max_wishes << " wishes, imm 1.." << cfg.max_imm
              << ", widths ";
    for (size_t i = 0; i < cfg.widths.size(); i++) {
        std::cout << (i ? "," : "") << cfg.widths[i];
    }
    std::cout << "\n\n";

    std::vector<Wish> prog;
    huntRec(H, world0, prog, cfg.max_stmts);

    std::cout << "searched " << H.searched << " candidate wishes.\n";
    std::cout << "found " << H.found << " minimal exploits in "
              << H.shapes.size() << " distinct shape(s).\n\n";

    // smallest witness first
    std::vector<const Shape*> order;
    for (const auto& kv : H.shapes) order.push_back(&kv.second);
    for (size_t i = 0; i + 1 < order.size(); i++) {
        for (size_t j = i + 1; j < order.size(); j++) {
            bool less = order[j]->stmts < order[i]->stmts ||
                        (order[j]->stmts == order[i]->stmts &&
                         order[j]->prog.size() < order[i]->prog.size());
            if (less) std::swap(order[i], order[j]);
        }
    }

    int n = 0;
    for (const Shape* sh : order) {
        n++;
        std::string key = signatureOf(sh->prog, sh->last);
        std::cout << "shape " << n << "   " << key
                  << "   (" << sh->prog.size() << " wish(es), "
                  << sh->stmts << " statement(s), " << sh->count
                  << " exploit(s) of this shape)\n";
        for (const auto& w : sh->prog) {
            if (w.body.empty()) {
                std::cout << "    wish " << w.name << " { }\n";
                continue;
            }
            std::cout << "    wish " << w.name << " {\n";
            for (const auto& st : w.body) {
                std::cout << "        " << stmtText(st) << "\n";
            }
            std::cout << "    }\n";
        }
        std::cout << "    -> " << genie.counter << " = " << sh->last.after
                  << ",  I1 " << (sh->last.i1_ok ? "holds" : "VIOLATED")
                  << ",  I2 " << (sh->last.i2_ok ? "holds" : "VIOLATED")
                  << "   (I2 expected <= " << sh->last.expected_max << ")\n\n";
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
        "           The world (register decls) still comes from the file.\n";
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

    // world state
    World world;
    for (const auto& d : prog.decls) {
        Reg r; r.width = d.width; r.val = d.init; r.normalize();
        world.regs[d.name] = r;
    }
    if (!world.regs.count(genie.counter)) {
        std::cerr << "the world has no '" << genie.counter << "' register\n";
        return 2;
    }

    std::cout << "== Loophole / wishc " << (hunt ? "--hunt ==  " : "==  ") << path << "\n";
    std::cout << "world: " << genie.counter << " = " << world.regs[genie.counter].val
              << "  (uint<" << world.regs[genie.counter].width << ">)\n";
    std::cout << "genie: toll=" << genie.toll
              << ", I1(" << genie.counter << " <= " << genie.cap << "), "
              << "I2(no net gain past the toll)\n\n";

    if (hunt) {
        runHunt(world, genie, hc);
        return 0;
    }

    int exploits = 0;
    for (const auto& w : prog.wishes) {
        std::cout << "wish " << w.name << " {\n";

        Outcome o = grantWish(w, genie, world, &std::cout);

        if (!o.legal) {
            std::cout << "    STATUS:  ILLEGAL — genie refuses\n";
            std::cout << "    " << o.illegal_reason << "\n}\n\n";
            continue;
        }
        if (!o.unknown_reg.empty()) {
            std::cerr << "unknown register '" << o.unknown_reg << "'\n";
            return 2;
        }

        std::cout << "    STATUS:  LEGAL\n";
        std::cout << "    I1  " << genie.counter << " <= " << genie.cap
                  << "  ->  " << (o.i1_ok ? "holds" : "VIOLATED")
                  << "   (" << genie.counter << " = " << o.after << ")\n";
        std::cout << "    I2  no net gain      ->  "
                  << (o.i2_ok ? "holds" : "VIOLATED")
                  << "   (expected <= " << o.expected_max
                  << ", actual " << o.after << ")\n";

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
