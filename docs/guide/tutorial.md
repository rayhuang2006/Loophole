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
register wishes : uint<2> = 3
people    alice, bob, rival

wish nothing {
}
```

第一行：一個叫 `wishes` 的暫存器，兩個位元寬（只放得下 0 到 3），初始值 3。
第二行：世界上有三個人，一開始都活著。
然後一個什麼都不做的願望。

存成 `my.wish` 跑跑看：

```bash
./wishc my.wish
```

你會看到即使願望是空的，`wishes` 還是從 3 掉到 2——那是過路費，每施一次都要付。

### 五個操作

| 寫法 | 做什麼 |
| --- | --- |
| `sub wishes, 3` | 減 3（會繞回，不會變負數） |
| `add wishes, 3` | 加 3（會繞回） |
| `widen wishes -> uint<64>` | 改位寬，值不動 |
| `kill alice` | 把 alice 的存活狀態設成 0 |
| `revive alice` | 設成 1 |

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
./wishc --dump-genie > mine.genie
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
./wishc --genie mine.genie my.wish
```

**注意這個檔案裡沒有什麼**：沒有暫存器、沒有人、沒有那五個操作。
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

把 R2 的 `surface` 改成 `ast` 存檔，再跑一次 `examples/03_tidy.wish`——
笑話 #3 當場失效。

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
| `alive(p)` | 那個人活著嗎 |
| `all p in S: ...` | S 裡的每一個都要滿足 |
| `consistent` | 精靈的承諾說得通嗎 |

`S` 可以是 `people`（宣告出來的，改不了）或任何定義（可以被玩家重綁）。
**這個選擇就是 FOOLED 存不存在的開關。**

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
./wishc --genie mine.genie --hunt examples/03_tidy.wish --max-stmts 3 --max-wishes 3
```

它會窮舉所有可能的願望，回報有幾種不同的破法。拿來比較：

```bash
# 預設的精靈
./wishc --hunt examples/03_tidy.wish --max-stmts 3 --max-wishes 3 | grep found
# 記取教訓的精靈
./wishc --genie genie/careful.genie --hunt examples/03_tidy.wish --max-stmts 3 --max-wishes 3 | grep found
```

九種對六種。差的三種正好是別名相關的那些。

**注意那個數字的意思**：`--max-stmts 3 --max-wishes 3` 是搜尋的框，
「九種」的完整意思是「**在這個框裡**有九種」。想確認框夠大，
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
