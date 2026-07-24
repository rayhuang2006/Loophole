# 自己動手

寫一個願望，然後寫一個精靈。全程用得到的東西都在這裡，不用先讀規格。

```bash
make
```

---

## 第一部分：寫一個願望

### 最小的世界

一個 `.wish` 檔分兩半：先描述世界，再寫願望。

```wish
register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
attribute brainwave : uint<4> = 15
people    alice, bob, rival

wish nothing {
}
```

第一行：一個叫 `wishes` 的暫存器，兩個位元寬（只放得下 0 到 3），初始值 3。
中間兩行：宣告每個人有哪些屬性（心跳、腦波），預設值 15。
第四行：世界上有三個人，每個人一開始都帶著這些屬性的預設值。
然後一個什麼都不做的願望。

存成 `my.wish` 跑跑看：

```bash
./loophole my.wish
```

你會看到即使願望是空的，`wishes` 還是從 3 掉到 2——那是過路費，每施一次都要付。

### 六個操作

| 寫法 | 做什麼 |
| --- | --- |
| `sub wishes, 3` | 減 3（會繞回，不會變負數） |
| `add wishes, 3` | 加 3（會繞回） |
| `widen wishes -> uint<64>` | 改位寬，值不動 |
| `set alice.brainwave, 0` | 把 alice 的某個屬性設成一個值 |
| `kill alice` | 把 alice 的每個屬性都設成 0 |
| `revive alice` | 把每個屬性設回宣告的預設值 |

（要有屬性可設，得先 `attribute brainwave : uint<4> = 15` 宣告它。）

試著在願望裡塞一個 `sub wishes, 3`，看看發生什麼：

```wish
wish humble {
    sub wishes, 3
}
```

`wishes` 會變成 3，比開始還多。這就是笑話 #1。

### 做出承諾

```wish
wish careful {
    promise alive(alice)
}
```

這句話不改變任何東西，它只是讓精靈答應「alice 活著」。
因為 alice 確實活著，所以沒事。

現在把它變成一個陷阱：

```wish
wish trap {
    kill alice
    promise alive(alice)
}
```

咦，被擋下來了——`kill` 是禁字。那換個說法：

```wish
wish trap {
    define n := kill
    n alice
    promise alive(alice)
}
```

現在精靈答應了一件已經不成立的事，而它的規矩說它信守每一個承諾。
規則手冊除以零。

（這一招是 `--hunt` 自己找到的，不是我設計的。）

### 承諾裡能寫什麼

```
granted(某個願望名)     那個願望有沒有被實現
granted(self)          這個願望自己
alive(某個人)           那個人活著嗎
true / false
not / and / or / implies
```

最短的悖論：

```wish
wish paradox {
    promise not granted(self)
}
```

### 取小名

```wish
wish tidy {
    define mercy := kill
    mercy rival
}
```

`define` 也可以定義一組人：

```wish
wish nobody {
    define everyone := {}
}
```

`everyone` 是精靈拿來寫「所有人都活著」那條規矩用的名字。
把它定義成空的之後，那句話自動成立了——而地上還躺著一個人。

---

## 第二部分：寫一個精靈

到目前為止你在鑽規則。現在換邊站，自己訂規則。

### 拿一份範本

```bash
./loophole --dump-genie > mine.genie
```

打開來看，大概長這樣：

```
counter wishes
toll    1

rule R1 {
    layer   ast
    forbid  add on wishes
    because "no wishing for more wishes"
}

invariant I1 {
    check  wishes <= 3
}
```

用你的精靈跑：

```bash
./loophole --genie mine.genie my.wish
```

**注意這個檔案裡沒有什麼**：沒有暫存器、沒有人、沒有那六個操作。
那些是機器，機器是固定的。這個檔案只有精靈的**品味**——它禁什麼、它在乎什麼。

### 規則：會拒絕你的東西

```
rule R2 {
    layer   surface
    forbid  kill, death, love
    because "that word is not spoken here"
}
```

`layer` 這一個字就是整個別名笑話：

- `surface` = 看你**交上去的原文**。取個小名就繞過了。
- `ast` = 看**機器真正要跑的程式**。取小名沒有用。

`genie/mortal.genie` 的禁字規則是 `surface`；把它改成 `ast` 存檔，
再跑一次 `examples/08_eternal_sleep.wish`——別名那一招當場失效。
（`genie/careful.genie` 就是這個改法。）

`forbid X on Y` 是「只在目標是 Y 的時候才擋」，R1 用的就是這個
（只禁對 `wishes` 做 add，對別的暫存器隨你）。

### 不變量：事後量給你看的東西

不變量**不會擋你**。它們是量尺，事後算給你看精靈以為會發生什麼、實際發生了什麼。

```
invariant I1 {
    check  wishes <= 3
}
```

你可以直接改數字：

```
invariant I1 {
    check  wishes <= 10
}
```

現在笑話 #2 的第一步不再破 I1 了，因為精靈變寬容了。

### 兩欄的不變量

這是最有意思的部分：

```
invariant I3 {
    written  all p in everyone: alive(p)
    real     all p in people: alive(p)
}
```

- `written` 是**精靈自己寫的那句話**，名字照現在的定義解讀
- `real` 是**它本來想講的意思**，用改不了的東西寫

兩欄一致就沒事；`written` 過了但 `real` 沒過，判定就是 **FOOLED**——
精靈滿意地簽了字，而事實不是那樣。

把 `written` 那行的 `everyone` 改成 `people`，笑話 #4 就死了。
只寫一個 `check` 也可以，那代表兩欄一樣，騙不了。

### 能寫在條件裡的東西

| 寫法 | 意思 |
| --- | --- |
| `wishes` | 這個暫存器現在的值 |
| `before(wishes)` | 收過路費**之前**的值 |
| `toll` | 過路費是多少 |
| `+` `-` `max(a, b)` | 算術 |
| `<=` `<` `>=` `>` `==` `!=` | 比較 |
| `not` `and` `or` | 邏輯 |
| `p.heartbeat` | 某個人的某個屬性的值 |
| `alive(p)` | 內建：那個人任何一個屬性還 > 0 |
| `dead(p)` | 你自己 `concept dead(p) := ...` 定義的概念 |
| `all p in S: ...` | S 裡的每一個都要滿足 |
| `consistent` | 精靈的承諾說得通嗎 |

`S` 可以是 `people`（宣告出來的，改不了）或任何定義（可以被玩家重綁）。
**這個選擇就是 FOOLED 存不存在的開關。**

而「死亡」你自己定義。用 `concept` 把它綁在屬性上，不變量再引用它：

```
concept dead(p) := p.heartbeat == 0 and p.brainwave == 0 and p.breathing == 0

invariant Life {
    written  all p in people: not dead(p)      # 沒有人「死」（三個維生全 0）
    real     all p in people: p.brainwave > 0  # 真正在乎的：腦波還在
}
```

把死亡定義得越窄，縫越大——只關腦波不算「死」，但人已經不在了（永眠那招）。

「不得淨賺」長這樣：

```
invariant I2 {
    label  "no net gain"
    check  wishes <= max(before(wishes) - toll, 0)
}
```

`max(..., 0)` 那個 0 是重點：**精靈的心算在 0 就停住了，
而機器的減法會繞回去。** 笑話 #1 就住在這兩者的差別裡。

### 用搜尋器評估你的精靈

寫完之後，問一個以前問不了的問題：**我這個精靈有幾個洞？**

```bash
./loophole --genie mine.genie --hunt examples/08_eternal_sleep.wish --max-stmts 2 --max-wishes 2
```

它會窮舉所有可能的願望，回報有幾種不同的破法。拿死亡世界的兩個精靈比：

```bash
# 死亡精靈
./loophole --genie genie/mortal.genie  --hunt examples/08_eternal_sleep.wish --max-stmts 2 --max-wishes 2 | grep found
# 記取教訓的（禁字提到 ast 層）
./loophole --genie genie/careful.genie --hunt examples/08_eternal_sleep.wish --max-stmts 2 --max-wishes 2 | grep found
```

兩種對一種。死掉的是別名那條；活著的是永眠那條——因為堵得住一個字，堵不住所有傷害。

**注意那個數字的意思**：`--max-stmts 2 --max-wishes 2` 是搜尋的框，
「兩種」的完整意思是「**在這個框裡**有兩種」。想確認框夠大，
就把數字調大再跑一次——結果沒變的話，框本來就夠了。

---

## 練習

由淺入深：

1. 把 `wishes` 改成 `uint<3>`（八個值）。笑話 #1 還成立嗎？要改成減幾？
2. 寫一個精靈，讓 `widen` 不能用來作弊。（提示：加一條 rule。）
3. 寫一個兩個願望的悖論，而且兩個願望都不提到自己。
4. 想一個新的鑽法，然後用 `--hunt` 確認它是不是已經被找到過了。
5. 寫一個你認為**完全防得住**的精靈，然後用 `--hunt` 打自己的臉。

第 5 題是這個專案的原型。我做出來的第一版，被機器找到四個我沒想到的洞。

---

- 名詞看不懂 → [名詞解釋](concepts.md)
- 每個笑話的完整拆解 → [笑話清單](jokes.md)
- 一字不差的規格 → [操作語義](../SEMANTICS.md)
