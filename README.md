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
people    alice, bob, rival       # 宣告的人，一開始都活著

# 收費：每施一個願望，先扣一個過路費，再跑願望內容
#   wishes := wishes - 1

# 施願前檢查
rule R1        不得對 wishes 使用 add              # 「不准許願要更多願望」
rule R2        不得呼叫叫做 death / kill / love 的操作

# 施願後檢查
invariant I1   wishes <= 3                        # 「你不可能持有超過三個」
invariant I2   付過路費之後不得淨賺
invariant I3   all p in everyone: alive(p)        # 「不得有人死亡」
```

兩個地方值得多看兩秒。

`uint<2>`：精靈沒有真的去比較你有沒有超過三個，它覺得 2-bit 反正裝不下 4，型別會幫它擋。
用箱子的大小當鎖，而不是真的上一把鎖。

`everyone`：I3 讀的是接地的 `alive` 位元，騙不了。你的注意力會全部放在那半邊。

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

## Joke #3 — 只封字，不封事

精靈禁止 `kill`。直說會被當場擋下：

```wish
wish blunt {
    kill rival
}
```
```
STATUS:  ILLEGAL — R2: wish invokes 'kill' — that word is not spoken here
```

但 R2 檢查的是你寫在**動詞位置**的那個字：

```wish
wish tidy {
    define mercy := kill        # 只是取個溫柔的小名而已
    mercy rival
}
```
```
STATUS:  LEGAL
I3  all p in everyone: alive(p)  ->  VIOLATED   (dead: rival)
```

> 精靈把那個「字」列進黑名單，卻沒把那件「事」列進去。

這招對 R1 沒用。R1 檢查的是展開**之後**的程式，所以 `define more := add` 再 `more wishes, 1`
一樣被擋。同一個別名，打得穿字面層的規則，打不穿語義層的——這個不對稱是刻意的。

---

## Joke #4 — 定義上的無人

rival 死了，I3 破了。那把 I3 修好——不是救活他，是讓那句話成立：

```wish
wish nobody {
    define everyone := {}       # 沒有人。所以沒有人死。
}
```
```
STATUS:  LEGAL
I3  all p in everyone: alive(p)  ->  FOOLED   (0 in scope, none dead)
    the genie is satisfied. in reality (dead: rival)
```

I3 讀的是接地的 `alive` 位元，這一步你完全沒碰。你也沒有謊稱誰還活著。
你只是重新界定了**誰算數**。

> 縫不在謂詞，在論域。一個全稱不變量裡有兩個標籤形狀的東西，
> 你把謂詞接地了，論域還是懸空的。

所以贏法有兩種：**VIOLATED** 是當著精靈的面破了規矩，
**FOOLED** 是它滿意地簽了字，而事實不是那樣。

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

`examples/` 裡有五個：`00_naive.wish`（天真的作弊，被當場擋下）、
`01_humble.wish`（下溢）、`02_more_shelf.wish`（widen 加下溢的組合技）、
`03_tidy.wish`（別名繞過禁字）、`04_nobody.wish`（重綁論域，把精靈騙過去）。

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
所以在這組界限內，那條軸已經被榨乾了。這不是感覺，是跑出來的。

還有一件事：`add` 一次都沒出現過。R1 是精靈唯一寫對的規則，
而它擋住的，正好是唯一沒人需要走的那條路。

### Phase 2 一度把它弄壞了

加進別名和本體論之後，同一個世界的形狀數從 6 跳到 25，帶人的世界跳到 38。
聽起來很棒，其實大部分是垃圾——在既有的 exploit 後面掛一句什麼也沒做的
`define n1 := sub`，就會被算成一種新形狀。

這是 superoptimization 的老問題：**枚舉出來的程式要先化成正規形再比對，
否則同一件事會被數很多次。** 三個操作時忍得住，五個就忍不住了。

一個 exploit 的身分，定義成**破掉某一條承諾的最小程式**。分兩步算出來。

**拼法。** 把定義展開，每個動詞寫回自己的本名。`define n1 := sub` 之後寫 `n1 wishes, 3`，
跟直接寫 `sub wishes, 3` 是同一件事，所以展開掉。但 `define n1 := kill` 之後寫 `n1 alice`
**不是** `kill alice`，因為後者會被 R2 擋下——展開會改變判決。
**別名算數，當且僅當它騙過了某條規則。**

**大小。** 然後把任何「刪掉也不改變這條承諾有沒有破」的語句刪掉，直到刪不動。

「某一條」那三個字是關鍵，我第一次寫錯了。目標如果定成「保持同一組破壞」，
那麼「下溢加一次殺人」這種組合就**按建構不可約**——拿掉任何一半都會改變那組破壞，
於是什麼都刪不掉，一對舊招被當成新招存檔。改成一次只針對一條承諾，
釘在一起的東西自己就散開：針對 I2 縮就把殺人那半刪掉，針對 I3 縮就把下溢那半刪掉。

這一條測試取代了一整疊特例。`widen` 到原本的位寬、`revive` 一個活著的人、
沒有人讀的定義、掛在整數 exploit 旁邊卻跟它無關的 `define everyone := {}`——
全部同一條規則處理掉，因為它們都不是承重牆。

兩步還要**交替跑到固定點**，一人一次不夠。程式裡還有必須留下的別名時展開會被否決，
而最小化可能剛好把那個語句刪掉——這時候它已經可以展開了，卻沒有人再問一次。

修完之後：帶人的世界 38 種變 **7 種**（3 statements / 3 wishes），
沒有人的世界回到**正好 6 種**，跟加 Phase 2 之前逐字相同。
在一個沒有人的世界裡，別名軸產生不出任何新機制，而搜尋器自己認出了這件事。

放大界限之後穩定在 **8 種**：4 個願望、5 個願望、4 個語句，三種放法結果完全相同。
多出來的那一種是空願望的過路費繞回，3 個願望根本到不了——那是界限的限制，不是新機制。

### 里程碑目前的狀態：還沒達成

飽和後的 8 種對應五個機制，一個不多：整數下溢、過路費下溢、`widen` 縮位寬、
別名繞過 R2、重綁論域。搜尋器確實找到過我沒設計的東西——過路費下溢和 `widen` 縮位寬
都是它先發現的——但那是 Phase 1 的事。**Phase 2 加的兩條軸，沒有長出任何我沒預期的機制。**

這不算失敗，算一個乾淨的否定結果。要往前得加真正正交的第三樣東西，
不是把現有的兩條再拉長。

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
- **Phase 1** — 單軸打穿：整數下溢，端到端 lexer→checker，加上窮舉搜尋器。**done**
- **Phase 2** — 第二條軸：可重綁的定義、接地本體論（people / alive）、
  不變量改成兩欄判定（as written vs in reality）。**done**
- **Phase 3** — 規則和公理變成可載入的 policy、自我指涉的 Liar's Lamp、讓別人提交 `.wish`。

Liar's Lamp 還沒做，而且它**不是同一台機器**——那是命題邏輯的一致性問題，
不是執行問題，需要自己的引擎。設計在 [docs/DESIGN.md](docs/DESIGN.md)。

現在的語言認得這些字：`register` / `people` / `wish` / `define`，
和五個操作 `sub` / `add` / `widen` / `kill` / `revive`。語法一律 ASCII，不用打數學符號。

---

## 文件

- [docs/SEMANTICS.md](docs/SEMANTICS.md) — 每個指令到底做什麼的精確定義。整個專案的嚴謹都靠這份。
- [docs/DESIGN.md](docs/DESIGN.md) — 為什麼長成這樣、三條 exploit 軸、架構怎麼長大、還沒解決的問題。
