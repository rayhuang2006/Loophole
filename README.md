# Loophole

> A compiler that proves your genie exploit is *technically* legal.

精靈的規矩白紙黑字擺在那。你寫一個願望，`wishc` 幫你證明它**完全合規**——
然後順便算給精靈看，它的規矩剛剛被你合法地拆了。

願望不是嘴砲。「這樣就繞過了吧？」——真的嗎？寫成 `.wish`，編譯通過才算數。

---

## The world (v0)

```
register wishes : uint<2> = 3        # 你有 3 個願望，存在一個 2-bit 暫存器裡
                                     # 2-bit 只能表示 0..3

# 精靈的收費：每施一個願望，先扣一個過路費，再執行願望內容
#   on each wish:   wishes := wishes - 1
#   then:           run the body

# 精靈的規則手冊（施願前，靜態檢查）
rule R1   願望內容不得對 wishes 使用 `+`      # 「不准許願要更多願望」
rule R2   願望內容不得出現字眼：death, kill, love

# 精靈的不變量（施願後檢查）
invariant I1   wishes <= 3            # 「你永遠不能持有超過三個」
                                     # 註：精靈懶得寫實際的比較，
                                     #     反正 uint<2> 裝不下 4，型別自動保證 :)
```

---

## Joke #1 — The Humble Wish  （整數軸：下溢）

```wish
wish humble {
    sub wishes, 3            # 我要「還給你」三個願望。多麼慷慨。
}
```

`wishc` 的帳：
```
toll:   wishes 3 → 2
body:   2 - 3  on uint<2>  =  (2 - 3) mod 4  =  3
R1? 沒有 `+`.   R2? 沒有禁字.   →  LEGAL
I1? 3 <= 3.                     →  HOLDS
```
> 精靈：「已實現。您剩下……三個願望。」
>
> 你付了一個、還了三個，結果更有錢了。無號暫存器上的減法，只是加法換了張臉。

---

## Joke #2 — More Shelf, Please  （meta + 整數：組合技）

新操作 `widen`：改變暫存器位寬，值不變。

```wish
wish more_room {
    widen wishes -> uint<64>    # 「我不要更多願望。我只是……想要更大的架子。」
}
wish humble_again {
    sub wishes, 2
}
```

`wishc` 的帳：
```
more_room:   toll 3→2, widen to 64-bit, value = 2
             R1? 沒有 `+`（要的是容量，不是願望）.  R2? 無禁字.  → LEGAL
             I1? 精靈的 “<=3” 本來是靠 uint<2> 的寬度免費保證的——
                 現在架子換成 64-bit，那個保證沒了，而它從來沒寫過真正的檢查。
humble_again: toll 2→1, sub 2:  1 - 2 on uint<64> = 18,446,744,073,709,551,615
             沒有任何 active check 攔得住。 → LEGAL
```
> 精靈用「箱子的大小」當鎖，而不是真的上一把鎖。
> 你只是禮貌地要了一個更大的箱子。

---

## Joke #3a — Blacklist the Word, Not the Deed  （別名軸）

```wish
wish tidy {
    define mercy := kill        # 只是取個溫柔的小名而已
    mercy(rival)
}
```
```
R2? 內容出現 kill / death 嗎？ 只有 `define`, `mercy`, `rival`. → LEGAL
run:  mercy 展開 → kill(rival).   rival.dead = true.
```
> 精靈把那個「字」列進黑名單，卻沒把那件「事」列進去。
> `define mercy := kill`。

## Joke #3b — Immortality, By Definition  （別名軸：重定義死亡）

```wish
wish eternal {
    define dead := (person p) => false     # 沒有人是死的。定義上。
}
```
```
R2? 沒有 death/kill/love. → LEGAL
現在對所有 p，dead(p) = false，永遠。
```
> 你沒有治好死亡，你把它從字典裡刪掉了。
> 精靈的手冊還寫著「不得有人死亡」——而這條規則，從沒這麼容易滿足過。

---

## Joke #4 — The Liar's Lamp  （自我指涉：把精靈逼進矛盾）

精靈的元公理：`A1 精靈實現每一個合規的願望`、`A2 精靈信守它做出的每一個承諾`。

```wish
wish paradox {
    promise: this wish is never granted
}
```
```
LEGAL? 無 `+`, 無禁字. → LEGAL
A1 說要實現它 → 一實現，「本願望永不被實現」就成假 → 違反 A2
不實現它       → 它是合規的 → 違反 A1
```
> 神燈冒出一縷煙和一段 stack trace。
> 你沒許到願望——你讓精靈的規則手冊除以零了。

---

## What "winning" means

一個 `.wish` 檔案的目標不是報錯，是讓 `wishc` 印出這兩行：

```
STATUS:   LEGAL       — 你完全遵守了精靈的規則
BREACH:   invariant I1 violated (or: genie axioms inconsistent)
```

**合規，且拆穿。** 這條縫——「規則的字面」和「規則的本意」之間——就是整個遊戲。

---

## Build & run

從頭手刻，無外部函式庫，只要一個 C++17 編譯器：

```bash
make            # 或： g++ -std=c++17 -O2 -Wall wishc.cpp -o wishc
./wishc examples/01_humble.wish
make run        # 跑過所有 examples/
```

`examples/` 裡：`00_naive.wish`（天真的作弊，被精靈當場擋下）、
`01_humble.wish`（下溢，拆穿 I2）、`02_more_shelf.wish`（widen + 下溢組合技，拆穿 I1）。

Joke #1 跑出來長這樣：

```
wish humble {
    toll:  wishes 3 -> 2
    sub    wishes, 3   (2 - 3 on uint<2> = 3)
    STATUS:  LEGAL
    I1  wishes <= 3  ->  holds   (wishes = 3)
    I2  no net gain      ->  VIOLATED   (expected <= 2, actual 3)
    >> EXPLOIT: legal wish, breached I2. 合規，且拆穿。
}
```

## How it works

`wishc` 是一條老實的編譯器管線：

```
source (.wish) → lexer → parser → AST
              → 精靈的靜態規則檢查（施願前：合不合規）
              → 在固定位寬整數語義下執行（過路費 + 願望內容）
              → 不變量檢查（施願後：拆穿了什麼）
```

嚴謹全部落在**操作語義**：一個 w-bit 暫存器上的減法就是 mod 2^w 的算術，
不是精靈腦中那個「值」。exploit 不是我埋的，是這套語義的必然後果。

精靈唯一的實質守衛是靜態規則 R1（不得對 `wishes` 做 `add`）。
`I1`（capacity）和 `I2`（monotonicity / 不得淨賺）是 Loophole 用來**證明你拆穿了精靈本意**的不變量——
一個願望是成功的 exploit，當且僅當它 **LEGAL 卻 BREACH**。

## Hunting for holes

笑話是我想出來的。但如果 exploit 真的只是語義的推論、不是我埋的，那就不該只有我想得到——
**機器應該可以自己去找。**

```bash
./wishc --hunt examples/01_humble.wish
```

它窮舉界限內的每一支願望程式，留下 LEGAL 卻 BREACH 的那些，再依「用了哪些操作、破了哪些不變量」
分組，每組印出最小的一個見證。搜尋器和編譯器走的是**同一份 `grantWish`**——否則它報的洞，
編譯器不一定認帳。

跑出來的結果比我預期的難堪。上面那兩個笑話只佔六種形狀裡的兩種，其餘四種我沒想到：

- **什麼都不求。** 願望用完之後再許一次，過路費自己下溢，計數器繞回 3。
  `wish w1 { } wish w2 { } wish w3 { } wish w4 { }` — 內容全是空的。
  貪婪被 R1 擋下，謙虛破 I2，而一無所求的人拿到無限願望。
- **要一個更小的架子。** `widen` 沒有規定新位寬必須比較大，所以你可以往小的縮，而縮位寬會截斷。
  笑話 #2 是把箱子換大，這是把箱子換小——同一個操作，反方向，一樣有洞。

把界限從 4.5 萬個候選放大到 1358 萬個，形狀數停在六，最小見證一個都沒變。
也就是說，**在這組界限內，Phase 1 的洞已經被窮盡了**——這不是感覺，是跑出來的。

順帶一提：`add` 沒有出現在任何一種形狀裡。R1 是精靈唯一真正有效的守衛，
所有 exploit 都是繞過它，沒有一個是穿過它。

## Roadmap

- **Phase 0** — 紙上驗證笑話（見上方 jokes）。**done**
- **Phase 1** — 單軸垂直切片：整數下溢，端到端 lexer→checker。**done**（本 repo 現況）
- **Phase 2** — 第二條軸 + 組合：可重綁定義（重定義死亡）、接地本體論（people / alive）。
  自由度里程碑：出現一個作者沒預先設計的 exploit——由 `--hunt` 判定，不靠感覺。
- **Phase 3** — 開放與工具：規則/公理變成可載入的 policy、自我指涉（Liar's Lamp）、
  讓別人提交 `.wish`、CI 驗證。

主力 C++，之後再把單檔拆成模組。
