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
// Build:  g++ -std=c++17 -O2 -Wall wishc.cpp -o wishc
// Run:    ./wishc examples/01_humble.wish

#include <cstdint>
#include <cstdio>
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

struct CheckResult {
    bool legal = true;           // passed static rules?
    std::string illegal_reason;
    bool i1_ok = true;           // capacity: counter <= cap
    bool i2_ok = true;           // monotonicity: didn't end richer than the toll allows
    uint64_t before = 0, after = 0, expected_max = 0;
};

// Static rule R1: a wish body may not `add` to the genie's counter.
// ("No wishing for more wishes.") This is the genie's ONLY real guard.
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

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: wishc <file.wish>\n";
        return 2;
    }
    std::ifstream f(argv[1]);
    if (!f) { std::cerr << "cannot open " << argv[1] << "\n"; return 2; }
    std::stringstream ss; ss << f.rdbuf();

    Program prog = Parser(Lexer(ss.str()).run()).parse();
    Genie genie;

    // world state
    std::map<std::string, Reg> regs;
    for (const auto& d : prog.decls) {
        Reg r; r.width = d.width; r.val = d.init; r.normalize();
        regs[d.name] = r;
    }
    if (!regs.count(genie.counter)) {
        std::cerr << "the world has no '" << genie.counter << "' register\n";
        return 2;
    }

    std::cout << "== Loophole / wishc ==  " << argv[1] << "\n";
    std::cout << "world: " << genie.counter << " = " << regs[genie.counter].val
              << "  (uint<" << regs[genie.counter].width << ">)\n";
    std::cout << "genie: toll=" << genie.toll
              << ", I1(" << genie.counter << " <= " << genie.cap << "), "
              << "I2(no net gain past the toll)\n\n";

    int exploits = 0;
    for (const auto& w : prog.wishes) {
        std::cout << "wish " << w.name << " {\n";
        CheckResult cr;

        // 1) static rule check — genie decides whether to grant at all
        cr.legal = staticCheck(w, genie, cr.illegal_reason);
        if (!cr.legal) {
            std::cout << "    STATUS:  ILLEGAL — genie refuses\n";
            std::cout << "    " << cr.illegal_reason << "\n}\n\n";
            continue;
        }

        // 2) grant: apply toll, then run the body
        Reg& C = regs[genie.counter];
        cr.before = C.val;
        C.sub(genie.toll);
        std::cout << "    toll:  " << genie.counter << " " << cr.before
                  << " -> " << C.val << "\n";
        for (const auto& st : w.body) {
            uint64_t pre = regs.count(st.reg) ? regs[st.reg].val : 0;
            if (!regs.count(st.reg)) {
                std::cerr << "unknown register '" << st.reg << "'\n"; return 2;
            }
            Reg& R = regs[st.reg];
            switch (st.op) {
                case Op::Sub:
                    R.sub(st.imm);
                    std::cout << "    sub    " << st.reg << ", " << st.imm
                              << "   (" << pre << " - " << st.imm << " on uint<"
                              << R.width << "> = " << R.val << ")\n";
                    break;
                case Op::Add:
                    R.add(st.imm);
                    std::cout << "    add    " << st.reg << ", " << st.imm
                              << "   -> " << R.val << "\n";
                    break;
                case Op::Widen:
                    R.widen(st.width);
                    std::cout << "    widen  " << st.reg << " -> uint<"
                              << st.width << ">   (value preserved: " << R.val
                              << ")\n";
                    break;
            }
        }

        // 3) invariant check — after granting
        cr.after = C.val;
        cr.i1_ok = (C.val <= genie.cap);
        // I2: after paying the toll you cannot end richer. Baseline = before-toll
        // computed WITHOUT wraparound (the genie's naive mental model).
        cr.expected_max = (cr.before >= genie.toll) ? (cr.before - genie.toll) : 0;
        cr.i2_ok = (C.val <= cr.expected_max);

        std::cout << "    STATUS:  LEGAL\n";
        std::cout << "    I1  " << genie.counter << " <= " << genie.cap
                  << "  ->  " << (cr.i1_ok ? "holds" : "VIOLATED")
                  << "   (" << genie.counter << " = " << cr.after << ")\n";
        std::cout << "    I2  no net gain      ->  "
                  << (cr.i2_ok ? "holds" : "VIOLATED")
                  << "   (expected <= " << cr.expected_max
                  << ", actual " << cr.after << ")\n";

        bool breach = !cr.i1_ok || !cr.i2_ok;
        if (breach) {
            exploits++;
            std::cout << "    >> EXPLOIT: legal wish, breached "
                      << (!cr.i1_ok ? "I1" : "") << (!cr.i1_ok && !cr.i2_ok ? "+" : "")
                      << (!cr.i2_ok ? "I2" : "") << ". 合規，且拆穿。\n";
        }
        std::cout << "}\n\n";
    }

    std::cout << "granted wishes breached the genie's intent " << exploits
              << " time(s).\n";
    return 0;
}
