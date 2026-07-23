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

## Status

Early design / concept stage. 語言核心 (`wishc`) 規劃用 C++ 從頭刻：lexer → parser → AST →
精確操作語義 → 靜態規則檢查 → 不變量檢查。第一階段先把 Joke #1 的下溢端到端跑通。
