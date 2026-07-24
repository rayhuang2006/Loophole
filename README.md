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

`wishc` 把這個笑話變成一個可以編譯的東西。上面那三個願望不是比喻，是一個檔案：

```wish
wish experiment      { sub   wishes, 3          }
wish bigger_shelf    { widen wishes -> uint<64> }
wish experiment_again{ sub   wishes, 2          }
```

```
$ ./wishc examples/07_the_original.wish

wish experiment {
    toll:  wishes 3 -> 2
    sub    wishes, 3   (2 - 3 on uint<2> = 3)
    STATUS:  LEGAL
    I2  no net gain  ->  VIOLATED   (wishes = 3, needs <= 2)
}

wish bigger_shelf {
    widen  wishes -> uint<64>   (value preserved: 2)
    I1  wishes <= 3  ->  holds          ← 這一步什麼規矩都沒破
}

wish experiment_again {
    sub    wishes, 2   (1 - 2 on uint<64> = 18446744073709551615)
    I1  wishes <= 3  ->  VIOLATED
}
```

中間那個願望值得多看兩秒：**它什麼都沒偷，它只是把鎖拆了。**
精靈那句「不超過三個」本來是兩位元的寬度免費送的——反正裝不下 4。
換成 64 位元之後保證就沒了，而精靈從來沒寫過真正的檢查。

願望不是嘴砲。「這樣就繞過了吧？」——真的嗎？寫成 `.wish`，編譯通過才算數。

---

## 跑跑看

從頭手刻，無外部函式庫，只要一個 C++17 編譯器：

```bash
make
./wishc examples/01_humble.wish
make run                          # 跑過全部例子（自動帶對的精靈）
```

一個比一個歪：

| 檔案 | 招式 |
| --- | --- |
| `00_naive.wish` | 直接許願要更多願望。當場被擋 |
| `01_humble.wish` | 「我還你三個。」下溢 |
| `02_more_shelf.wish` | 「我只是想要更大的架子。」換個箱子再下溢 |
| `07_the_original.wish` | 上面那個，原版笑話三個願望一字不差 |
| `05_liar.wish` | 「我保證這個願望永遠不會被實現。」 |
| `06_next_one.wish` | 不用談自己，一樣能把精靈逼進矛盾 |
| `04_nobody.wish` | 沒辦法讓死人復活，那就重新定義「所有人」（配 `genie/vigil.genie`） |
| `08_eternal_sleep.wish` | 精靈只禁「死」，那就讓他永遠睡著（配 `genie/mortal.genie`） |

有些例子需要特定的精靈——檔案開頭的 `# genie:` 那行會告訴 `make run` 要載入哪一個。
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

`genie/careful.genie` 是一個記取教訓的死亡精靈。規矩的**字面意思一個字都沒改**——
一樣禁 kill，一樣守「沒有人死」——只把禁字檢查從「看你寫的字」提到「看展開後的程式」：

```
$ ./wishc --genie genie/careful.genie examples/08_eternal_sleep.wish

wish tidy {
    STATUS:  ILLEGAL — NoKilling: wish invokes 'kill' —
                       that deed is not done here, whatever you call it
}
```

取小名沒有用了。而且死掉的是哪條漏洞，`--hunt` 量得出來——
`genie/mortal.genie` 有兩種漏洞（別名殺人、永遠睡著），`careful` 只剩一種：

**別名那條死了，永眠那條活著。** 因為你堵得住一個字，堵不住「所有的傷害」——
永眠不在精靈禁的字裡，它是規則從沒提到的一種狀態。

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
| [語言規格 1.0](docs/spec/loophole-1.0.md) | 正式白皮書（英文），模仿 C++/Python 標準的寫法 |
| [操作語義](docs/SEMANTICS.md) | 精確語義的中文說明。每個指令到底做什麼 |
| [設計筆記](docs/DESIGN.md) | 為什麼長成這樣，走過哪些彎路 |
| [願景與全家桶](docs/VISION.md) | 這個專案想長成什麼樣、三個子專案的規劃 |

---

## 進度

- **Phase 1** 整數軸：下溢，端到端的檢查器，加上窮舉搜尋器。**done**
- **Phase 2** 別名軸：可重綁的定義、人與生死。**done**
- **Phase 3** 自我指涉：承諾、精靈的元公理、命題邏輯引擎。**done**
- **Phase 4** 精靈變成可載入的資料檔。**done**
- **v1.0** 兩層世界模型：人的屬性 + 精靈用 `concept` 定義的死亡。**done**（規格見 [語言規格 1.0](docs/spec/loophole-1.0.md)）
- **接下來** 讓別人提交願望和精靈、CI 驗證、瀏覽器 playground。

現在的語言認得 `register` / `attribute` / `people` / `wish` / `define` / `promise`，
六個操作 `sub` / `add` / `widen` / `set` / `kill` / `revive`，
精靈政策裡的 `concept`，以及承諾裡用的 `granted` / `alive` / `self` / `not` / `and` / `or` / `implies`。
語法一律 ASCII，不用打數學符號。
