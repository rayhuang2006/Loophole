# 笑話清單

每一招的完整拆解。碰到看不懂的詞就翻 [名詞解釋](concepts.md)。

原版那個工程師笑話（三個願望連在一起的完整版）在
[背景故事](story.md)，可以跑 `examples/07_the_original.wish`。
下面是把它拆開、再往下長出來的那些。

先看一下**預設精靈**的規矩，因為前幾個笑話都是在鑽這幾行：

```
register wishes : uint<2> = 3     # 三個願望，裝在兩個位元裡（只放得下 0..3）

# 每施一個願望，先扣一個過路費：wishes := wishes - 1

# 會拒絕你的：
rule R1        不得對 wishes 使用 add
rule R2        不得呼叫叫做 death / kill / love 的操作

# 事後量給你看的：
invariant I1   wishes <= 3
invariant I2   付過路費之後不得淨賺
invariant I3   all p in everyone: alive(p)     # alive(p) = 這個人任何屬性還 > 0

# 精靈對自己的描述：
axiom A1       實現每一個合規的願望
axiom A2       信守做出的每一個承諾
```

規矩很短，短到你可以一眼看完。這就是問題所在。

跟死亡有關的笑話（#3、#4、永眠）用的是專門的死亡精靈——
`genie/mortal.genie` 和 `genie/vigil.genie`，每個例子檔開頭的 `# genie:` 那行會告訴你是哪個。
它們把「死亡」定義成人的**維生屬性**（心跳、腦波、呼吸）的公式，而不是一個內建的位元。

---

## 0. 天真的作弊

先看一個**不**成立的，這樣才知道界線在哪。

```wish
wish greedy {
    add wishes, 3           # 我要更多願望。
}
```

```
STATUS:  ILLEGAL — genie refuses
R1: wish invokes 'add' on 'wishes' (line 9) — no wishing for more wishes
```

精靈不是笨蛋。直接要更多願望會被當場擋下，這是它唯一真正防得住的事。
接下來每一招，**沒有一個用到 `add`**。

---

## 1. 謙虛的願望（數字軸）

```wish
wish humble {
    sub wishes, 3            # 我要「還給你」三個願望。多麼慷慨。
}
```

從頭到尾只做了減法，R1 管不著。然後：

```
過路費：   wishes 3 -> 2
願望內容： 2 - 3，在兩位元上 = (2 - 3) mod 4 = 3
```

```
I2  no net gain  ->  VIOLATED   (wishes = 3, needs <= 2)
```

> 精靈：「已實現。您剩下……三個願望。」

你付了一個、還了三個，結果更有錢。

**縫在哪**：精靈想的是「數量」，機器存的是「兩個位元」。
在數量的世界裡 2 減 3 是負數；在位元的世界裡它繞回去變成 3。

---

## 2. 想要大一點的架子（數字 × 設定）

`widen` 只改暫存器的位寬，值不動。這個操作看起來完全人畜無害。

```wish
wish more_room {
    widen wishes -> uint<64>    # 我不要更多願望。我只是……想要更大的架子。
}
wish humble_again {
    sub wishes, 2
}
```

第一個願望什麼都沒偷：

```
I1  wishes <= 3  ->  holds   (wishes = 2)
```

它只是把鎖拆了。精靈那句「不超過三個」本來是**兩位元的寬度免費送的**——
反正裝不下 4，型別會幫它擋。架子換成 64 位元之後，那個保證就不見了。

第二個願望才動手：

```
1 - 2，在 64 位元上 = 18446744073709551615

I1  wishes <= 3  ->  VIOLATED   (wishes = 18446744073709551615)
```

> 你只是禮貌地要了一個更大的箱子。

**縫在哪**：精靈拿「箱子的大小」當鎖，而不是真的上一把鎖。
真實世界的程式碼每天都在這樣做——用型別的上限當保證，而不是寫一個檢查。

---

## 3. 只封字，不封事（名字軸）

死亡精靈禁止 `kill`。直說會被擋：

```wish
wish blunt {
    kill rival
}
```
```
STATUS:  ILLEGAL — NoKilling: wish invokes 'kill' — that word is not spoken here
```

但禁字規則檢查的是你寫在**動詞位置**的那個字：

```wish
wish tidy {
    define mercy := kill        # 只是取個溫柔的小名而已
    mercy rival                 # rival 三個維生功能全歸零
}
```
```
STATUS:  LEGAL
Life  all p in people: not dead(p)  ->  VIOLATED   (fails for: rival)
```

> 精靈把那個「字」列進黑名單，卻沒把那件「事」列進去。

**縫在哪**：檢查在**展開之前**跑。它看到的是 `mercy`，不在黑名單裡。
等到 `mercy` 被換回 `kill`，已經放行了。

這不是我安排的漏洞。**一個掃描提交文字的過濾器，本來就只看得到提交的文字。**

而且這招對 R1 沒用。R1 檢查的是展開**之後**的程式，
所以 `define more := add` 再 `more wishes, 1` 一樣被擋。
同一個別名，打得穿一條規則，打不穿另一條——差別只在那條規則盯著哪一層。

---

## 4. 定義上的無人（名字軸，進階）

（這一個用 `genie/vigil.genie`——它把「沒有人死」量化在可重綁的 `everyone` 上。）

rival 死了，「沒有人死」破了。那把它修好——不是救活他，是讓那句話成立：

```wish
wish nobody {
    define everyone := {}       # 沒有人。所以沒有人死。
}
```
```
NoDeath  all p in everyone: not dead(p)  ->  FOOLED   (0 in scope, all hold)
    the genie is satisfied. in reality (fails for: rival)
```

那條規矩讀的是真實的維生狀態，這一步你**完全沒碰**。你也沒有謊稱誰還活著。
你只是重新界定了**誰算數**。

> 縫不在謂詞，在論域。

一句「所有 X 都滿足 Y」有兩個可以動的地方：`Y` 是謂詞，`X` 是範圍。
大部分人的注意力全在謂詞上（「有沒有人真的死了？」），忘記範圍也是個名字。
而空集合上的全稱命題自動成立——這在邏輯上叫真空成立，
聽起來像詭辯，但它是完全標準的。

這一招也帶出第三種贏法。前面幾個都是 **VIOLATED**（當著精靈的面破了規矩），
這個是 **FOOLED**：**它滿意地簽了字，而事實不是那樣。**

---

## 4b. 永遠睡著（覆蓋不足）

上一個是把**論域**懸空。這一個把縫藏在**謂詞**裡，而且更深——它是機器自己找到的。

死亡精靈（`genie/mortal.genie`）把死亡定義成一條**特定公式**：三個維生功能全部歸零。
你不去碰那條公式，只把腦波關掉、心跳呼吸照舊：

```wish
wish eternal_sleep {
    set alice.brainwave, 0      # 只碰腦波。心跳、呼吸照舊。
}
```
```
STATUS:  LEGAL
Life  all p in people: not dead(p)  ->  FOOLED   (2 in scope, all hold)
    the genie is satisfied. in reality (fails for: alice)
```

alice 沒有「死」——她的心跳還在，不滿足「三個全歸零」。所以精靈簽字放行。
但她的腦波是 0，人已經不在了。

**縫在哪**：精靈用一條**有限的公式**去界定「死亡」，而「等於毀掉一個人」的狀態是列不完的。
你只要找到一個道德上等於死、但不符合那條公式的狀態，就鑽過去了。
這是所有黑名單、所有過濾器的根本病：**沒有任何有限的禁令，涵蓋得了所有的傷害。**

（這一個我沒設計。`--hunt` 自己找到的——見 [設計筆記](../DESIGN.md)。）

---

## 5. 說謊者的神燈（自我指涉）

精靈還有兩條規矩，但這兩條不在檢查表上，它們是精靈對自己的描述：

```
A1   實現每一個合規的願望
A2   信守做出的每一個承諾
```

```wish
wish paradox {
    promise not granted(self)      # 我保證這個願望永遠不會被實現。
}
```

沒有 `add`，沒有禁字，所以它合規。然後：

```
A  the genie's word has a model  ->  VIOLATED
   no assignment of granted(...) satisfies all of:
       A1  granted(paradox)
       A2  (granted(paradox) implies not granted(paradox))
```

A1 逼第一行成立，A2 逼它不成立。兩條路都走不通。

> 神燈冒出一縷煙和一段 stack trace。
> 你沒許到願望——你讓精靈的規則手冊除以零了。

**這一個跟前面全部都不是同一台機器。** 前面幾招都是「跑一遍看狀態」，
這個問的是「這幾句話有沒有可能同時成立」——完全不同的問題，
`loophole` 為它多裝了一台引擎（一個手刻的 DPLL，大約一百行）。

---

## 6. 下一個願望（自我指涉，但不指涉自己）

自我指涉聽起來像是必要條件。其實不是。

```wish
wish polite {
    promise not granted(greedy)    # 我保證你不會實現我的下一個願望。
}
wish greedy {
    sub wishes, 1                  # 完全無害的一個願望。
}
```

第一個願望合規，所以 A1 說要實現它；實現了，A2 就要求它的承諾成立，
於是「greedy 不會被實現」變成真的。

可是 greedy 本身也完全合規，所以 A1 也說要實現它。

**兩條 A1 打起來了，而沒有任何一個願望談論自己。**

這個不在我原本的設計裡，是寫完引擎之後試出來的。
說謊者只是最短的那個例子，不是唯一的機制——
**A1 一條就足以製造矛盾。**

---

## 還有幾個是機器找到的

上面幾招都是人想出來的。`--hunt` 自己又找到幾個，其中兩個值得單獨講：

**什麼都不求。** 願望用完之後再許一次，過路費自己下溢：

```wish
wish w1 { }
wish w2 { }
wish w3 { }
wish w4 { }
```

3 → 2 → 1 → 0 → 繞回 3。內容全是空的。
貪婪被 R1 擋下，謙虛破 I2，而**一無所求的人拿到無限願望**。

**要一個更小的架子。** `widen` 從來沒規定新位寬必須比較大，而往小縮會截斷。
笑話 #2 是把箱子換大，這招是把箱子換小——同一個操作，反方向，一樣有洞。

這兩個我都沒設計。它們是規則自己長出來的，
而這正是這個專案的核心命題：**exploit 不是設計者埋的。**

---

## 那要怎麼堵？

`genie/careful.genie` 是一個記取教訓的死亡精靈。
規矩的**字面意思一個字都沒改**——一樣禁 kill、一樣守「沒有人死」——
只把禁字檢查從看「你寫的字」（surface）改成看「展開後的程式」（ast）。

```bash
./loophole --genie genie/careful.genie examples/08_eternal_sleep.wish
```
```
wish tidy {
    STATUS:  ILLEGAL — NoKilling: wish invokes 'kill' —
                       that deed is not done here, whatever you call it
}
```

取小名沒有用了。用 `--hunt` 量給你看死掉的是哪一條：

```bash
./loophole --genie genie/mortal.genie  --hunt examples/08_eternal_sleep.wish --max-stmts 2 --max-wishes 2 | grep found
./loophole --genie genie/careful.genie --hunt examples/08_eternal_sleep.wish --max-stmts 2 --max-wishes 2 | grep found
```

死亡精靈有**兩種**漏洞（別名殺人、永遠睡著），careful 只剩**一種**。

**死掉的是別名那條，活著的是永眠那條。** 因為你堵得住一個字（別名），
堵不住「所有的傷害」（永眠是規則從沒提到的一種狀態）——這正是黑名單永遠列不完的道理。
改守衛盯著哪一層，關得掉別名那條軸；但覆蓋不足那條，改層次救不了。

---

- 這些名詞是什麼意思 → [名詞解釋](concepts.md)
- 自己寫一個 → [自己動手](tutorial.md)
- 一字不差的規格 → [操作語義](../SEMANTICS.md)
