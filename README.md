# Loophole

> A compiler that proves your genie exploit is *technically* legal.

精靈的規矩白紙黑字擺在那。你寫一個願望，`wishc` 幫你證明它**完全合規**——
然後順便算給精靈看，它的規矩剛剛被你合法地拆了。

願望不是嘴砲。「這樣就繞過了吧？」——真的嗎？寫成 `.wish`，編譯通過才算數。

---

## 精靈的規矩

規矩很短，短到你可以一眼看完。這就是問題所在。

```
register wishes : uint<2> = 3     # 三個願望，裝在一個 2-bit 暫存器裡
                                  # 2-bit 只放得下 0..3

# 收費：每施一個願望，先扣一個過路費，再跑願望內容
#   wishes := wishes - 1

# 施願前檢查
rule R1        願望內容不得對 wishes 使用 add    # 「不准許願要更多願望」

# 施願後檢查
invariant I1   wishes <= 3                      # 「你不可能持有超過三個」
invariant I2   付過路費之後不得淨賺
```

`uint<2>` 那行值得多看兩秒。精靈沒有真的去比較你有沒有超過三個——
它覺得 2-bit 反正裝不下 4，型別會幫它擋。

用箱子的大小當鎖，而不是真的上一把鎖。真實世界的程式碼每天都在這樣做。

---

## Joke #1 — The Humble Wish

```wish
wish humble {
    sub wishes, 3            # 我要「還給你」三個願望。多麼慷慨。
}
```

R1 只禁止 `add`。你從頭到尾只做了減法，完全合規。然後：

```
過路費：   wishes 3 → 2
願望內容： 2 - 3，在 uint<2> 上 = (2 - 3) mod 4 = 3
```

> 精靈：「已實現。您剩下……三個願望。」
>
> 你付了一個、還了三個，結果更有錢了。無號暫存器上的減法，只是加法換了張臉。

---

## Joke #2 — More Shelf, Please

`widen` 只改暫存器的位寬，值不動。

```wish
wish more_room {
    widen wishes -> uint<64>    # 「我不要更多願望。我只是……想要更大的架子。」
}
wish humble_again {
    sub wishes, 2
}
```

第一個願望什麼都沒偷，它只是把鎖拆了：精靈的「不超過三個」本來是 `uint<2>` 免費送的，
架子換成 64-bit 之後，那個保證就不見了。第二個願望才動手：

```
過路費：   wishes 2 → 1
願望內容： 1 - 2，在 uint<64> 上 = 18,446,744,073,709,551,615
```

> 你只是禮貌地要了一個更大的箱子。

---

## 什麼叫贏

目標不是讓它報錯，是讓 `wishc` 印出這兩行：

```
STATUS:   LEGAL       — 你完全遵守了精靈的規則
BREACH:   invariant I1 violated
```

**合規，且拆穿。** 這條縫——「規則的字面」和「規則的本意」之間——就是整個遊戲。

順帶一提 I1 和 I2 的角色，因為很容易誤會：它們不是鎖，不會擋你。
真正會擋你的只有 R1。I1 和 I2 是**量尺**，事後量給你看精靈以為會發生什麼、實際發生了什麼。
所以「破壞不變量」不是失敗，那就是贏。

---

## 跑跑看

從頭手刻，無外部函式庫，只要一個 C++17 編譯器：

```bash
make            # 或： g++ -std=c++17 -O2 -Wall wishc.cpp -o wishc
./wishc examples/01_humble.wish
make run        # 跑過所有 examples/
```

`examples/` 裡有三個：`00_naive.wish`（天真的作弊，被當場擋下）、
`01_humble.wish`（下溢）、`02_more_shelf.wish`（widen 加下溢的組合技）。

Joke #1 跑出來長這樣：

```
wish humble {
    toll:  wishes 3 -> 2
    sub    wishes, 3   (2 - 3 on uint<2> = 3)
    STATUS:  LEGAL
    I1  wishes <= 3  ->  holds      (wishes = 3)
    I2  no net gain  ->  VIOLATED   (expected <= 2, actual 3)
    >> EXPLOIT: legal wish, breached I2. 合規，且拆穿。
}
```

---

## 讓機器自己去找洞

笑話是我想出來的。但如果洞真的是語義的推論、不是我偷埋的，
那我就不該是唯一想得到的人——機器應該也找得到才對。

```bash
./wishc --hunt examples/01_humble.wish
```

它把界限內每一支願望程式都跑過一遍，留下「合規卻拆穿」的那些，
再照「用了哪些操作、破了哪些不變量」分類，每一類印一個最小的例子。二十毫秒跑完。

結果有點難堪：**它找到六種，我只想到兩種。**

沒想到的那些裡，有兩個是真的新招。

**什麼都不求。** 三個願望用完之後再許一次，過路費自己下溢，計數器繞回 3：

```wish
wish w1 { }
wish w2 { }
wish w3 { }
wish w4 { }
```

內容全是空的。貪婪被 R1 擋下，謙虛破 I2，而一無所求的人拿到無限願望。

**要一個更小的架子。** `widen` 從來沒規定新位寬必須比較大，而往小縮會截斷。
笑話 #2 是把箱子換大，這招是把箱子換小——同一個操作，反方向，一樣有洞。

我把搜尋界限從 4.5 萬個候選推到 1358 萬個，形狀數停在六，最小的例子一個都沒變。
所以在這組界限內，這條軸已經被榨乾了。這不是感覺，是跑出來的。

還有一件事：`add` 一次都沒出現過。R1 是精靈唯一寫對的規則，
而它擋住的，正好是唯一沒人需要走的那條路。

---

## 它怎麼運作的

`wishc` 是一條老實的編譯器管線：

```
source (.wish) → lexer → parser → AST
              → 精靈的靜態規則檢查（施願前：合不合規）
              → 在固定位寬整數語義下執行（過路費 + 願望內容）
              → 不變量檢查（施願後：拆穿了什麼）
```

嚴謹全部落在**操作語義**。一個 w-bit 暫存器上的減法就是 mod 2^w 的算術，
不是精靈腦中那個「值」。exploit 不是我埋的，是這套語義的必然後果。

每個指令精確定義成什麼、過路費為什麼會下溢、`widen` 為什麼能縮，
都寫在 [docs/SEMANTICS.md](docs/SEMANTICS.md)。

---

## 還沒做的

- **Phase 0** — 紙上驗證笑話。**done**
- **Phase 1** — 單軸打穿：整數下溢，端到端 lexer→checker，加上窮舉搜尋器。**done**（就是現在這個 repo）
- **Phase 2** — 第二條軸：可重綁的定義（重新定義死亡）、接地本體論（people / alive）。
  過關條件是 `--hunt` 跑出我沒設計的新形狀。
- **Phase 3** — 規則和公理變成可載入的 policy、自我指涉的 Liar's Lamp、讓別人提交 `.wish`。

還有一批笑話沒實作——別名軸（`define mercy := kill`、把死亡從字典裡刪掉）
和那個會讓精靈自相矛盾的 Liar's Lamp。它們連同設計上的取捨都在
[docs/DESIGN.md](docs/DESIGN.md)。

那邊的 `.wish` 片段是**設計目標，不是能跑的語法**。
這個 repo 現在只認得 `register` / `wish` / `sub` / `add` / `widen`，五個字。

---

## 文件

- [docs/SEMANTICS.md](docs/SEMANTICS.md) — 每個指令到底做什麼的精確定義。整個專案的嚴謹都靠這份。
- [docs/DESIGN.md](docs/DESIGN.md) — 為什麼長成這樣、三條 exploit 軸、架構怎麼長大、還沒解決的問題。
