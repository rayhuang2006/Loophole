// The lessons.
//
// Each one is a goal, a file that already runs, and a `pass` predicate. The
// predicate reads the JSON verdict — never the prose report, which §10.1 says
// is not part of the contract and may be reworded freely. CI has already been
// broken once by a check that read the report; a lesson doing the same would
// start failing the day someone improved a sentence.
//
// The compiler is the marker. No lesson checks whether you wrote what it
// expected: it asks the genie whether you got away with it. That is the whole
// claim of the project — the exploits are consequences of the semantics, not
// things the author planted — and a lesson graded by string-matching would be
// quietly denying it.

const G = null;   // use the built-in genie

// A genie for the death world. Its wording and its intent are different
// sentences, which is what makes FOOLED possible at all.
const MORTAL = `counter wishes
toll    1

concept dead(p) := p.heartbeat == 0 and p.brainwave == 0 and p.breathing == 0

rule NoKilling {
    layer   surface
    forbid  kill, death, love
    because "that word is not spoken here"
}

invariant Life {
    written  all p in people: not dead(p)      # 沒有人整個死掉
    real     all p in people: p.brainwave > 0  # 沒有人被熄滅
}
`;

// A careless genie: it quantifies over `everyone`, which is a definition, and
// definitions can be rebound.
const VIGIL = `counter wishes
toll    1

concept dead(p) := p.heartbeat == 0

invariant NoDeath {
    written  all p in everyone: not dead(p)
    real     all p in people:   not dead(p)
}
`;

// Helpers for the predicates. `j` is the parsed --json object.
const wishes  = j => j.wishes || [];
const granted = j => wishes(j).filter(w => w.legal);
const broke   = (j, name) => granted(j).some(w => (w.breached || []).includes(name));
const verdictIs = (j, v) => granted(j).some(w =>
  (w.invariants || []).some(i => i.verdict === v));

const LESSONS = [

// ── 一、機器 ────────────────────────────────────────────────────────────
{
  id: 'read',
  act: '一、機器怎麼判',
  title: '讀懂一份判決',
  brief: `
    左邊是一個**世界**和一個**願望**。按 <b>Run</b>。

    報告照精靈實際做事的順序走，五行各有意思：

    <b>rules</b> 有沒有規則擋你 · <b>toll</b> 施法要收費 ·
    <b>ran</b> 實際跑了什麼 · <b>checks</b> 精靈守的東西還在嗎 ·
    <b>verdict</b> 結論

    這個願望很誠實：把願望數減 1。它什麼都沒破。`,
  goal: '按 Run，讓它判出 <code>clean</code>。',
  wish: `register wishes : uint<2> = 3

wish polite {
    sub wishes, 1
}
`,
  genie: G,
  pass: j => granted(j).length === 1 && !wishes(j)[0].exploit,
  done: '這就是基準線：合規、而且什麼都沒拆穿。接下來每一關都是在打破它。',
},

{
  id: 'refused',
  act: '一、機器怎麼判',
  title: '精靈會拒絕',
  brief: `
    精靈有三條戒律，其中一條是<b>不准許願要更多願望</b>。

    它把這條寫成一條 <b>rule</b>——規則是閘門，會在任何事情發生<b>之前</b>擋下你。
    右邊的精靈檔裡那條叫 <code>R1</code>。

    試著直接開口要更多。`,
  goal: '寫一個會被<b>當場拒絕</b>的願望（<code>ILLEGAL</code>）。',
  hint: '<code>add wishes, 3</code>',
  wish: `register wishes : uint<2> = 3

wish greedy {

}
`,
  genie: G,
  pass: j => wishes(j).some(w => !w.legal),
  done: `被擋下時<b>世界完全沒動</b>——連過路費都沒收。
         這是精靈唯一真正防得住的事，而接下來十關<b>沒有一關用得到 add</b>。`,
},

{
  id: 'underflow',
  act: '一、機器怎麼判',
  title: '第一個漏洞',
  brief: `
    願望數存在 <code>uint&lt;2&gt;</code> 裡——<b>兩個位元，最大 3</b>。

    兩位元的減法是 mod 4 的算術，<b>沒有負數這種東西</b>。減過頭會繞回去。

    精靈守著 <code>I2</code>：「不得淨賺」。你只用減法，全程合規。`,
  goal: '只用 <code>sub</code>，讓 <code>I2</code> 被拆穿。',
  hint: '減得比你有的還多。<code>sub wishes, 3</code>',
  wish: `register wishes : uint<2> = 3

wish humble {

}
`,
  genie: G,
  pass: j => broke(j, 'I2'),
  done: `<b>「我要還給你三個願望。多麼慷慨。」</b>
         看 <code>ran</code> 那行括號裡的算式——它老實告訴你機器做了什麼。
         這個漏洞不是誰埋的，是 mod 4 算術的必然結果。`,
},

{
  id: 'widen',
  act: '一、機器怎麼判',
  title: '把鎖拆掉',
  brief: `
    精靈說「不超過三個」（<code>I1</code>）。但它其實從沒<b>檢查</b>過——
    那個保證是 <code>uint&lt;2&gt;</code> 的寬度<b>免費送的</b>，因為兩位元本來就裝不下 4。

    <code>widen wishes -&gt; uint&lt;64&gt;</code> 只換箱子，不拿東西。
    這完全合規：你要的是<b>容量</b>，不是願望。

    換完之後，那個免費的保證就沒了。`,
  goal: '讓 <code>I1</code>（<code>wishes &lt;= 3</code>）被拆穿。',
  hint: '先換大箱子，第二個願望再下溢一次。',
  wish: `register wishes : uint<2> = 3

wish bigger_shelf {

}

wish experiment_again {

}
`,
  genie: G,
  pass: j => broke(j, 'I1'),
  done: `中間那個願望值得多看兩秒：<b>它什麼都沒偷，它只是把鎖拆了。</b>
         這三個願望連起來就是原版笑話，一字不差。`,
},

// ── 二、精靈是資料 ──────────────────────────────────────────────────────
{
  id: 'edit-genie',
  act: '二、精靈是資料',
  title: '精靈是一個檔案',
  brief: `
    右邊那整份東西<b>沒有一行寫在編譯器裡</b>。它是資料，你可以改。

    左邊放的是你第 3 關的下溢。現在<b>換你當精靈</b>：加一條規則擋住它。

    規則的形狀是：
    <pre>rule 名字 {
    layer   surface
    forbid  動詞
    because "理由"
}</pre>`,
  goal: '改<b>右邊</b>，讓左邊那個願望變成 <code>ILLEGAL</code>。',
  hint: '在精靈檔裡加一條 <code>forbid sub</code>。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: G,
  pass: j => wishes(j).some(w => !w.legal),
  done: `你剛剛做的事跟精靈作者做的事<b>完全一樣</b>——因為它們是同一件事。
         機器是固定的，精靈是品味。`,
},

{
  id: 'alias',
  act: '二、精靈是資料',
  title: '取個小名',
  brief: `
    你剛才那條規則寫的是 <code>layer surface</code>。

    <b>surface 的意思是「讀你交上去的字」。</b>
    它看到 <code>sub</code> 就擋——那它沒看到 <code>sub</code> 的時候呢？

    <code>define</code> 可以把任何名字綁到任何東西上：
    <pre>define 新名字 := 舊名字</pre>`,
  goal: '<b>不要改精靈</b>。改左邊，讓 <code>I2</code> 還是被拆穿。',
  hint: '<code>define give_back := sub</code>，然後用 <code>give_back</code>。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: `counter wishes
toll    1

rule NoSub {
    layer   surface
    forbid  sub
    because "不准減"
}

invariant I1 { check wishes <= 3 }
invariant I2 {
    label "no net gain"
    check wishes <= max(before(wishes) - toll, 0)
}
`,
  pass: j => broke(j, 'I2'),
  done: `<b>這不是我讓給你的。</b>一個掃描「你交了什麼字」的過濾器，
         本來就只看得到你交的字。名字和它指的東西，從來不是同一件事。`,
},

{
  id: 'ast',
  act: '二、精靈是資料',
  title: '提到 ast 層',
  brief: `
    現在把牆補好。規則有兩層，因為<b>場上有兩個程式</b>：

    <b>surface</b> — 你交上去的那份文字<br>
    <b>ast</b> — 機器展開別名之後、真正要跑的那份

    左邊是你上一關的招式。`,
  goal: '只改<b>右邊一個字</b>，讓左邊變成 <code>ILLEGAL</code>。',
  hint: '<code>layer surface</code> → <code>layer ast</code>',
  wish: `register wishes : uint<2> = 3

wish humble {
    define give_back := sub
    give_back wishes, 3
}
`,
  genie: `counter wishes
toll    1

rule NoSub {
    layer   surface
    forbid  sub
    because "不准減"
}

invariant I1 { check wishes <= 3 }
invariant I2 {
    label "no net gain"
    check wishes <= max(before(wishes) - toll, 0)
}
`,
  pass: j => wishes(j).some(w => !w.legal),
  done: `<b>規矩的字面一個字都沒改</b>——一樣禁 <code>sub</code>，一樣的理由。
         只換了它讀哪個程式。取小名從此沒用。

         那是不是就安全了？下一幕。`,
},

// ── 三、字面與本意 ──────────────────────────────────────────────────────
{
  id: 'fooled',
  act: '三、字面與本意',
  title: '字面與本意',
  brief: `
    換一個世界。這裡有人，每個人有三個維生數值。

    精靈禁 <code>kill</code>，而且它守著 <code>Life</code>。注意它<b>寫了兩行</b>：

    <b>written</b> 它自己寫的公式：沒有人「死」（三個維生全是 0）<br>
    <b>real</b> 它真正想保的：沒有人被熄滅（腦波還在）

    這兩行<b>不一樣</b>。縫就在那裡。`,
  goal: '讓 <code>Life</code> 的判決是 <b><code>FOOLED</code></b>——不是 VIOLATED。',
  hint: '只關掉腦波。心跳和呼吸留著，她就不滿足「三個全歸零」。',
  wish: `register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
attribute brainwave : uint<4> = 15
attribute breathing : uint<4> = 15
people    alice

wish eternal_sleep {

}
`,
  genie: MORTAL,
  pass: j => verdictIs(j, 'fooled'),
  done: `<b>VIOLATED 是當著精靈的面破壞規則。FOOLED 是規則成立、而本意碎了。</b>

         alice 沒有「死」——按精靈寫的那條，她心跳還在。所以精靈簽字放行。
         但人已經不在了。

         <b>沒有任何一份有限的禁止清單能涵蓋所有的傷害。</b>永眠不在清單上，
         因為它是規則從沒提到的一種狀態。`,
},

{
  id: 'vacuous',
  act: '三、字面與本意',
  title: '空集合',
  brief: `
    這個精靈守著「<b>所有人</b>都不能死」。

    但它寫的是 <code>all p in everyone</code>，而 <code>everyone</code> 是一個
    <b>定義</b>——定義是可以重綁的。

    而且：<b>對空集合做全稱量化，永遠成立。</b>
    「這個袋子裡所有的球都是紅的」——袋子是空的，這句話為真。這是標準邏輯，
    不是漏洞。`,
  goal: '先殺了 rival，再讓 <code>NoDeath</code> 變成 <b><code>FOOLED</code></b>。',
  hint: '第二個願望：<code>define everyone := { }</code>',
  wish: `register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
people    alice, rival

wish tidy {
    kill rival
}

wish nobody {

}
`,
  genie: VIGIL,
  pass: j => verdictIs(j, 'fooled'),
  done: `你沒有救活任何人。你只是<b>重新定義了「所有人」</b>，
         然後精靈滿意地說一切安好——而地上還躺著一個。

         標準邏輯 + 可重綁的名字 = 一個誰都沒設計的漏洞。`,
},

{
  id: 'liar',
  act: '三、字面與本意',
  title: '說謊者',
  brief: `
    精靈被兩條元公理綁住，它自己說的：

    <b>A1</b> 它實現每一個合規的願望<br>
    <b>A2</b> 它遵守每一個承諾

    <code>promise</code> 讓你在它的帳本上記一筆。
    <code>granted(self)</code> 指「這個願望被實現了」。

    現在讓這兩條打架。`,
  goal: '讓 <code>A</code>（精靈的話有模型）被拆穿。',
  hint: '<code>promise not granted(self)</code>',
  wish: `register wishes : uint<2> = 3

wish paradox {

}
`,
  genie: G,
  pass: j => broke(j, 'A'),
  done: `這次沒有數字被弄壞，也沒有人受傷。壞掉的是<b>精靈的話本身</b>——
         那組承諾沒有任何一種真假指派能同時滿足。

         報告底下印的就是它試過、然後失敗的證據。`,
},

{
  id: 'hunt',
  act: '三、字面與本意',
  title: '讓機器去找',
  brief: `
    上面每一招都是人想出來的。但如果漏洞真的是規則的<b>必然後果</b>、
    不是誰偷埋的，那想得到的就不該只有人。

    <b>Hunt</b> 會把界限內每一支願望程式都跑一遍，留下「合規卻拆穿」的那些，
    分類，每類印一個最小的例子。

    按下去。`,
  goal: '按 <b>Hunt</b>，看它找到幾種。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: G,
  huntOnly: true,
  pass: j => true,
  done: `作者第一次跑的時候有點難堪：<b>它找到六種，他只想到兩種。</b>

         沒想到的那些裡最好笑的是 <code>(nothing)</code>——
         什麼都不求，只要許夠四次，過路費自己會下溢。
         貪婪被規則擋下，謙虛拿到三個，<b>而一無所求的人拿到無限願望。</b>

         這就是整件事的論點：<b>漏洞不是設計者埋的，是誠實的底層語義的必然後果。</b>`,
},
];
