# Loophole

> A compiler that proves your genie exploit is *technically* legal.

有個工程師笑話是這樣的。精靈的戒律是不能殺人、不能讓人相愛、不能許願要更多願望。
於是工程師說：「我許願，**扣掉**我三個願望。」

精靈照做——先扣掉施展這個願望的一個，剩兩個，再扣三個，變成負一。
可是願望數存在一個無號的格子裡，沒有負這種東西，於是它繞回最大值。

「已實現。您現在還有三個願望。」

工程師笑了：**最大值是 3，那這是個兩位元的系統。**
於是他許第二個願望——把格子換成 64 位元。這完全合規，他要的是容量不是願望。
然後同一招再來一次。

`wishc` 把這件事變成真的。你寫一個願望，它證明你**完全遵守了規則**，
然後算給精靈看它的規則剛剛被你合法地拆穿。

願望不是嘴砲。「這樣就繞過了吧？」——真的嗎？寫成 `.wish`，編譯通過才算數。

```wish
wish humble {
    sub wishes, 3            # 我要「還給你」三個願望。多麼慷慨。
}
```

```
$ ./wishc examples/01_humble.wish

wish humble {
    toll:  wishes 3 -> 2
    sub    wishes, 3   (2 - 3 on uint<2> = 3)
    STATUS:  LEGAL
    I1  wishes <= 3                   ->  holds      (wishes = 3, needs <= 3)
    I2  no net gain                   ->  VIOLATED   (wishes = 3, needs <= 2)
    >> EXPLOIT: legal wish, breached I2. 合規，且拆穿。
}
```

精靈唯一的實質規矩是「不准許願要更多願望」，而你從頭到尾只做了減法。
你付了一個、還了三個，結果更有錢。無號整數的減法，只是加法換了張臉。

---

## 跑跑看

從頭手刻，無外部函式庫，只要一個 C++17 編譯器：

```bash
make
./wishc examples/01_humble.wish
make run                          # 跑過全部八個例子
```

八個例子，一個比一個歪：

| 檔案 | 招式 |
| --- | --- |
| `00_naive.wish` | 直接許願要更多願望。當場被擋 |
| `01_humble.wish` | 「我還你三個。」下溢 |
| `02_more_shelf.wish` | 「我只是想要更大的架子。」換個箱子再下溢 |
| `03_tidy.wish` | 精靈禁止 kill，那就給它取個小名 |
| `04_nobody.wish` | 沒辦法讓死人復活，那就重新定義「所有人」 |
| `05_liar.wish` | 「我保證這個願望永遠不會被實現。」 |
| `06_next_one.wish` | 不用談自己，一樣能把精靈逼進矛盾 |
| `07_the_original.wish` | 上面那個笑話，三個願望一字不差 |

每一招的完整拆解在 [笑話清單](docs/guide/jokes.md)。

---

## 兩個比較特別的功能

### `--hunt`：讓機器自己去找洞

笑話是我想出來的。但如果洞真的是規則的必然後果、不是我偷埋的，
那我就不該是唯一想得到的人：

```bash
./wishc --hunt examples/01_humble.wish
```

它把界限內每一支願望程式都跑一遍，留下「合規卻拆穿」的那些，分類，
每類印一個最小的例子。二十毫秒跑完。

第一次跑的結果有點難堪：**它找到六種，我只想到兩種。**

沒想到的那些裡，最好笑的是這個——什麼都不求，只要許夠四次，過路費自己會下溢：

```wish
wish w1 { }
wish w2 { }
wish w3 { }
wish w4 { }
```

內容全是空的。貪婪被規則擋下，謙虛拿到三個，而一無所求的人拿到無限願望。

### `--genie`：換一個精靈

精靈的規矩——禁什麼、守什麼——**沒有一條寫在程式碼裡**，
它們住在一個你可以改的檔案：

```bash
./wishc --dump-genie > mine.genie
./wishc --genie mine.genie examples/01_humble.wish
```

`genie/careful.genie` 是一個記取教訓的精靈。規矩的**字面意思一個字都沒改**，
只調整了它把守衛放在哪裡。結果上面那些笑話一半當場失效：

```
$ ./wishc --genie genie/careful.genie examples/03_tidy.wish

wish tidy {
    STATUS:  ILLEGAL — R2: wish invokes 'kill' — that deed is not done here,
                       whatever you call it
}
```

而且失效的是哪一半，`--hunt` 量得出來：預設的精靈有九種漏洞，這個只剩六種。

---

## 這是怎麼做到的

```
你的 .wish  →  能不能施？        （規則檢查）
            →  施了會怎樣？      （在精確定義的語義下跑一遍）
            →  拆穿了什麼？      （量精靈以為的和實際發生的差多少）
```

嚴謹全部落在**語義定義**：兩位元暫存器上的減法就是 mod 4 的算術，
不是精靈腦中那個「值」。exploit 不是我埋的，是這套定義的必然後果。

---

## 文件

剛接觸的話，從第一份開始讀就好：

| | |
| --- | --- |
| [背景故事](docs/guide/story.md) | 這個專案為什麼存在，那個笑話為什麼好笑 |
| [名詞解釋](docs/guide/concepts.md) | 溢位、不變量、量詞、SAT 是什麼。不用先會 |
| [笑話清單](docs/guide/jokes.md) | 每一招完整拆解 |
| [自己動手](docs/guide/tutorial.md) | 寫一個願望，寫一個精靈 |
| [操作語義](docs/SEMANTICS.md) | 精確規格。每個指令到底做什麼 |
| [設計筆記](docs/DESIGN.md) | 為什麼長成這樣，走過哪些彎路 |

---

## 進度

- **Phase 1** 整數軸：下溢，端到端的檢查器，加上窮舉搜尋器。**done**
- **Phase 2** 別名軸：可重綁的定義、人與生死。**done**
- **Phase 3** 自我指涉：承諾、精靈的元公理、命題邏輯引擎。**done**
- **Phase 4** 精靈變成可載入的資料檔。**done**
- **Phase 5** 讓別人提交願望和精靈、CI 驗證、瀏覽器 playground。

現在的語言認得 `register` / `people` / `wish` / `define` / `promise`，
五個操作 `sub` / `add` / `widen` / `kill` / `revive`，
以及承諾裡用的 `granted` / `alive` / `self` / `not` / `and` / `or` / `implies`。
語法一律 ASCII，不用打數學符號。
