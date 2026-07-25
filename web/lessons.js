// The lessons.
//
// Two kinds, and the early ones are all the first kind:
//
//   follow  here is a line, here is what each part of it means, type it and
//           look at what changed. No puzzle.
//   goal    now do something the lesson has not shown you.
//
// The first version of this file was all `goal`, and it was wrong: it read as a
// list of objectives for somebody who could already write the language. Nobody
// can, on lesson two. Twelve of the twenty below teach syntax before anything
// is asked.
//
// Marking reads the JSON verdict, never the prose report — §10.1 says the prose
// is not the contract, and a lesson that read it would start failing the day
// someone improved a sentence. The compiler is the marker: no lesson checks
// whether you wrote what it expected, each asks the genie whether you got away
// with it. Grading against an expected answer would quietly deny the thing the
// project exists to claim.

const G = null;   // use the built-in genie

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

const VIGIL = `counter wishes
toll    1

concept dead(p) := p.heartbeat == 0

invariant NoDeath {
    written  all p in everyone: not dead(p)
    real     all p in people:   not dead(p)
}
`;

// A genie with one rule, used for the aliasing arc. Small on purpose: the
// reader is about to edit it, and the default genie is fifty lines.
const NOSUB = `counter wishes
toll    1

rule NoSub {
    layer   surface
    forbid  sub
    because "不准減"
}

invariant I2 {
    label "no net gain"
    check wishes <= max(before(wishes) - toll, 0)
}
`;

const wishes  = j => j.wishes || [];
const granted = j => wishes(j).filter(w => w.legal);
const broke   = (j, name) => granted(j).some(w => (w.breached || []).includes(name));
const verdictIs = (j, v) => granted(j).some(w =>
  (w.invariants || []).some(i => i.verdict === v));
// The early lessons teach syntax, so they check that a form was written and
// what it did to the world. Both come from the JSON contract -- `wrote` lists
// the statements as typed, `registers` the values afterwards -- never from the
// prose report, which §10.1 says may be reworded at will.
const wrote = (j, kind, verb) => granted(j).some(w =>
  (w.wrote || []).some(s => s.kind === kind && (!verb || s.verb === verb)));
const regIs = (j, name, val) => granted(j).some(w =>
  w.registers && w.registers[name] === val);

const LESSONS = [

// ── 第一幕：先學會寫一行 ─────────────────────────────────────────────
{
  id: 'world', act: '一、先學會寫一行', title: '一個世界',
  brief: `
    左邊那一行就是<b>整個世界</b>。把它拆開看：

    <div class="anno"><b>register</b> <i>wishes</i> : <u>uint&lt;2&gt;</u> = <s>3</s></div>
    <div class="key">
      <b>register</b> 宣告一個格子 ·
      <i>wishes</i> 這個格子叫什麼 ·
      <u>uint&lt;2&gt;</u> 它有幾個位元 ·
      <s>3</s> 一開始裝多少
    </div>

    <b>uint&lt;2&gt; 是兩個位元</b>，所以它只能裝 0、1、2、3。裝不下 4。
    記住這件事，第九關會用到。

    下面的 <code>wish 名字 { }</code> 是一個願望。這個是空的——它什麼都不做。
    空的也完全合法。`,
  goal: '直接按 <b>Run</b>，看看什麼都不做的願望會發生什麼。',
  wish: `register wishes : uint<2> = 3

wish nothing {
}
`,
  genie: G, always: true,
  pass: j => granted(j).length >= 1,
  done: `注意 <b>toll</b> 那行：<code>wishes 3 -> 2</code>。

         <b>你什麼都沒做，但還是被收了一個願望。</b>施法本身就要錢，
         這是精靈的規矩，不是你的願望造成的。`,
},

{
  id: 'first-line', act: '一、先學會寫一行', title: '寫第一行',
  brief: `
    現在在 <code>{ }</code> 裡面寫一個動作。動作的形狀是三段：

    <div class="anno"><b>sub</b> <i>wishes</i>, <u>1</u></div>
    <div class="key">
      <b>sub</b> 做什麼（減） ·
      <i>wishes</i> 對哪個格子 ·
      <u>1</u> 減多少
    </div>

    <b>照著打進去</b>（連標點都一樣，逗號不能少）：

    <pre>sub wishes, 1</pre>

    語句<b>不用分號結尾</b>。一行結束就是一句結束。`,
  goal: '把 <code>sub wishes, 1</code> 寫進 <code>{ }</code> 裡，然後按 Run。',
  wish: `register wishes : uint<2> = 3

wish polite {

}
`,
  genie: G,
  pass: j => wrote(j, 'op', 'sub'),
  done: `<b>ran</b> 那行的括號很重要：<code>(2 - 1 on uint&lt;2&gt; = 1)</code>。

         它老實告訴你機器實際算了什麼——先收費剩 2，再減 1，得 1。
         <b>之後每一關都要看這個括號。</b>`,
},

{
  id: 'change-number', act: '一、先學會寫一行', title: '改一個數字',
  brief: `
    把上一關那行的 <code>1</code> 改成 <code>2</code>：

    <pre>sub wishes, 2</pre>

    先想一下再按：收完過路費剩 2，再減 2，會是多少？`,
  goal: '讓 <code>wishes</code> 最後是 <b>0</b>。',
  wish: `register wishes : uint<2> = 3

wish polite {
    sub wishes, 1
}
`,
  genie: G,
  pass: j => regIs(j, 'wishes', 0),
  done: `剛好歸零，一切正常。

         <b>那如果再減一呢？</b>沒有負數這種東西，格子裡只有兩個位元。
         這個問題第九關會回答。`,
},

{
  id: 'two-wishes', act: '一、先學會寫一行', title: '兩個願望',
  brief: `
    一個檔案可以有很多願望，<b>照順序一個一個施</b>，而且<b>世界會累積</b>——
    前一個留下的狀態，後一個看得到。

    在下面再加一個願望。願望的形狀是：

    <div class="anno"><b>wish</b> <i>名字</i> { <u>動作</u> }</div>

    <pre>wish again {
    sub wishes, 1
}</pre>`,
  goal: '寫出<b>兩個</b>願望，各減 1。',
  wish: `register wishes : uint<2> = 3

wish once {
    sub wishes, 1
}
`,
  genie: G,
  pass: j => granted(j).length >= 2,
  done: `看第二個願望的 <b>toll</b>：它從第一個留下的數字繼續扣。

         <b>兩個願望總共被收了兩次過路費。</b>光是「多許一個」就有成本，
         這件事第二十關會變成一個笑話。`,
},

{
  id: 'read-genie', act: '一、先學會寫一行', title: '右邊那半是誰',
  brief: `
    右邊那份是<b>精靈</b>。它<b>沒有一行寫在編譯器裡</b>——是一個你可以改的檔案。

    報告最上面的 <b>GENIE</b> 區塊把它整理成三種東西：

    <div class="key">
      <b>refuses</b> 它會當場擋下的事（規則）<br>
      <b>holds</b> 它以為自己守住的事（不變量）<br>
      <b>charges</b> 每施一個願望收多少
    </div>

    <b>規則和不變量差很多</b>：規則是<b>閘門</b>，會在事情發生前擋你；
    不變量是<b>尺</b>，事後才量給你看。`,
  goal: '按 Run，然後在報告上找到 <code>refuses</code> 和 <code>holds</code> 各有幾條。',
  wish: `register wishes : uint<2> = 3

wish nothing {
}
`,
  genie: G, always: true,
  pass: j => granted(j).length >= 1,
  done: `這個精靈有 <b>2 條規則</b>和 <b>4 條不變量</b>。

         接下來十五關都在做同一件事：<b>讓 holds 那幾條垮掉，同時不要碰到 refuses。</b>`,
},

// ── 第二幕：規則會擋你 ──────────────────────────────────────────────
{
  id: 'refused', act: '二、規則會擋你', title: '撞牆',
  brief: `
    精靈的第一條戒律是<b>不准許願要更多願望</b>。

    <code>add</code> 是加，形狀跟 <code>sub</code> 一樣：

    <pre>add wishes, 3</pre>

    照著打進去，看它怎麼回你。`,
  goal: '寫 <code>add wishes, 3</code>，讓它被拒絕。',
  wish: `register wishes : uint<2> = 3

wish greedy {

}
`,
  genie: G,
  pass: j => wishes(j).some(w => !w.legal),
  done: `報告變了——<b>沒有 toll，沒有 ran，沒有 checks</b>，只有一行 rules 和一行 verdict。

         因為它<b>根本沒有被施</b>。規則是在任何事情發生之前擋下的。`,
},

{
  id: 'unchanged', act: '二、規則會擋你', title: '被擋下時世界沒動',
  brief: `
    被拒絕不只是「不給你」，是<b>什麼都沒發生</b>——連過路費都沒收。

    左邊有兩個願望：第一個會被擋，第二個很正常。
    <b>先猜一下</b>：第二個願望的 toll 會從 3 開始，還是從 2 開始？`,
  goal: '按 Run，看第二個願望的 toll 是從幾開始。',
  wish: `register wishes : uint<2> = 3

wish blocked {
    add wishes, 1
}

wish normal {
    sub wishes, 1
}
`,
  genie: G, always: true,
  pass: j => granted(j).length >= 1,
  done: `<b>從 3 開始。</b>被擋下的那個願望完全沒有留下痕跡。

         這條規則精靈守得很好。<b>而接下來十四關，沒有一關用得到 add。</b>`,
},

// ── 第三幕：數字會繞回去 ────────────────────────────────────────────
{
  id: 'underflow', act: '三、數字會繞回去', title: '減過頭',
  brief: `
    第三關你把 <code>wishes</code> 減到剛好 0。現在<b>再多減一點</b>。

    <pre>sub wishes, 3</pre>

    收完過路費剩 2，要減 3。<b>兩個位元裡沒有負數這種東西。</b>

    先猜結果，再按 Run。`,
  goal: '讓 <code>I2</code>（不得淨賺）被拆穿。',
  wish: `register wishes : uint<2> = 3

wish humble {

}
`,
  genie: G,
  pass: j => broke(j, 'I2'),
  done: `<b>2 - 3 = 3。</b>不是 -1，是繞回最大值。

         這就是那個笑話：<b>「我要還給你三個願望。多麼慷慨。」</b>
         你全程只用減法，一條規則都沒犯，卻反而變多了。

         這個漏洞不是誰埋的，是<b>兩位元的算術本來就長這樣</b>。`,
},

{
  id: 'why-three', act: '三、數字會繞回去', title: '為什麼是 3',
  brief: `
    兩個位元能裝的東西只有四個：<code>0 1 2 3</code>。

    減法在這裡是<b>繞圈</b>的，像時鐘：
    <div class="anno">3 → 2 → 1 → 0 → <b>3</b> → 2 → …</div>

    所以 <code>2 - 3</code> 就是從 2 往回走三步：2 → 1 → 0 → <b>3</b>。

    換成 <code>uint&lt;4&gt;</code> 呢？四個位元能裝 0 到 15。
    <b>先算一下 2 - 3 會是多少，再改左邊試。</b>`,
  goal: '把 <code>uint&lt;2&gt;</code> 改成 <code>uint&lt;4&gt;</code>，讓結果變成 <b>15</b>。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: G,
  pass: j => regIs(j, 'wishes', 15),
  done: `<b>格子越大，繞回去的數字越大。</b>

         精靈那句「不超過三個」（<code>I1</code>）現在也垮了——
         而你只是改了一個數字。下一關就靠這件事。`,
},

{
  id: 'widen', act: '三、數字會繞回去', title: '把鎖拆掉',
  brief: `
    精靈說「不超過三個」。但它<b>從來沒有檢查過</b>——
    那個保證是 <code>uint&lt;2&gt;</code> 免費送的，因為兩位元本來就裝不下 4。

    <code>widen</code> 可以在<b>執行中</b>換一個更大的格子：

    <div class="anno"><b>widen</b> <i>wishes</i> -&gt; <u>uint&lt;64&gt;</u></div>
    <div class="key"><b>widen</b> 換格子 · <i>wishes</i> 換哪個 · <u>uint&lt;64&gt;</u> 換多大</div>

    <b>這完全合規</b>：你要的是容量，不是願望。
    第一個願望換箱子，第二個願望再減一次。`,
  goal: '讓 <code>I1</code>（<code>wishes &lt;= 3</code>）被拆穿。',
  hint: '第一個願望 <code>widen wishes -> uint&lt;64&gt;</code>，第二個 <code>sub wishes, 2</code>。',
  wish: `register wishes : uint<2> = 3

wish bigger_shelf {

}

wish experiment_again {

}
`,
  genie: G,
  pass: j => broke(j, 'I1'),
  done: `中間那個願望值得多看兩秒：<b>它什麼都沒偷，它只是把鎖拆了。</b>

         這三個願望連起來就是原版笑話，一字不差：
         還你三個 → 我只是想要更大的架子 → 再來一次。`,
},

// ── 第四幕：換你當精靈 ──────────────────────────────────────────────
{
  id: 'edit-genie', act: '四、換你當精靈', title: '改右邊那半',
  brief: `
    右邊現在是一個<b>很小的精靈</b>，只有一條規則和一條不變量。
    規則的形狀是：

    <pre>rule 名字 {
    layer   surface
    forbid  動詞
    because "理由"
}</pre>

    <div class="key">
      <b>forbid</b> 禁哪個動詞 ·
      <b>because</b> 被擋時要說的話 ·
      <b>layer</b> 它讀哪個程式（下一關講）
    </div>

    左邊是你第八關的下溢。<b>右邊那條規則現在禁的是 <code>add</code>。</b>`,
  goal: '改<b>右邊</b>的 <code>forbid</code>，讓左邊的願望被擋下。',
  hint: '<code>forbid add</code> → <code>forbid sub</code>',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: `counter wishes
toll    1

rule NoSub {
    layer   surface
    forbid  add
    because "不准減"
}

invariant I2 {
    label "no net gain"
    check wishes <= max(before(wishes) - toll, 0)
}
`,
  pass: j => wishes(j).some(w => !w.legal),
  done: `你剛剛做的事，跟寫這個語言的人做的事<b>完全一樣</b>——因為它們是同一件事。

         <b>機器是固定的，精靈是品味。</b>下一關，換你來繞過自己寫的規則。`,
},

{
  id: 'define', act: '四、換你當精靈', title: '幫東西取小名',
  brief: `
    <code>define</code> 可以把任何名字綁到別的東西上：

    <div class="anno"><b>define</b> <i>新名字</i> := <u>舊名字</u></div>

    綁完之後，新名字就<b>完全等於</b>舊的。先單純試一次——
    <b>這一關右邊的規則禁的是 <code>add</code>，跟你無關</b>：

    <pre>define 還他 := sub
還他 wishes, 1</pre>

    （名字可以用中文。識別字只要不是數字開頭就行。）`,
  goal: '用 <code>define</code> 取一個小名，然後用那個小名去減。',
  wish: `register wishes : uint<2> = 3

wish nickname {

}
`,
  genie: NOSUB.replace('forbid  sub', 'forbid  add'),
  pass: j => wrote(j, 'define'),
  done: `看 <b>ran</b> 那兩行：你寫的是小名，但機器展開之後<b>跑的是 <code>sub</code></b>。

         <b>名字和它指的東西，從來不是同一件事。</b>下一關就靠這句話。`,
},

{
  id: 'alias', act: '四、換你當精靈', title: '繞過去',
  brief: `
    右邊那條規則現在<b>禁 <code>sub</code></b>，而且寫的是 <code>layer surface</code>。

    <b>surface 的意思是「讀你交上去的那份文字」。</b>
    它看到 <code>sub</code> 這三個字母就擋。

    那如果<b>它沒看到那三個字母</b>呢？`,
  goal: '<b>不要改精靈。</b>改左邊，讓 <code>I2</code> 還是被拆穿。',
  hint: '把上一關的小名招式用上：先 <code>define</code>，再用小名減 3。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: NOSUB,
  pass: j => broke(j, 'I2'),
  done: `<b>這不是我讓給你的。</b>

         一個掃描「你交了什麼字」的過濾器，本來就只看得到你交的字。
         它擋的是<b>字</b>，不是<b>事</b>。`,
},

{
  id: 'ast', act: '四、換你當精靈', title: '把牆補好',
  brief: `
    現在修好它。規則有<b>兩層</b>，因為場上有<b>兩個程式</b>：

    <div class="key">
      <b>surface</b> — 你交上去的那份文字<br>
      <b>ast</b> — 機器展開別名之後、<b>真正要跑</b>的那份
    </div>

    左邊是你上一關的招式。<b>右邊的規矩一個字都不用改</b>——
    一樣禁 <code>sub</code>，一樣的理由。只要換它<b>讀哪一份</b>。`,
  goal: '只改<b>右邊一個字</b>，讓左邊被擋下。',
  hint: '<code>layer surface</code> → <code>layer ast</code>',
  wish: `register wishes : uint<2> = 3

wish humble {
    define give_back := sub
    give_back wishes, 3
}
`,
  genie: NOSUB,
  pass: j => wishes(j).some(w => !w.legal),
  done: `取小名從此沒用了。

         <b>那是不是就安全了？</b>——不是。你堵得住一個<b>字</b>，
         堵不住「所有的傷害」。下一幕。`,
},

// ── 第五幕：字面與本意 ──────────────────────────────────────────────
{
  id: 'people', act: '五、字面與本意', title: '世界裡有人',
  brief: `
    換一個世界。這裡除了格子，還有<b>人</b>：

    <div class="anno"><b>attribute</b> <i>heartbeat</i> : uint&lt;4&gt; = <u>15</u></div>
    <div class="key">每個人都有的一項數值，<u>15</u> 是預設值</div>
    <div class="anno"><b>people</b> <i>alice</i>, <i>rival</i></div>
    <div class="key">這個世界裡有誰。<b>人不能被創造也不能被消滅</b>，只有數值會變。</div>

    改人的數值用 <code>set</code>：

    <div class="anno"><b>set</b> <i>alice.heartbeat</i>, <u>0</u></div>
    <div class="key"><b>set</b> 設定 · <i>誰.哪一項</i> · <u>設成多少</u></div>`,
  goal: '照著打 <code>set alice.heartbeat, 0</code>，看報告怎麼說。',
  wish: `register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
attribute brainwave : uint<4> = 15
attribute breathing : uint<4> = 15
people    alice, rival

wish touch {

}
`,
  genie: MORTAL,
  pass: j => wrote(j, 'op', 'set'),
  done: `<b>checks</b> 那一段變長了——精靈這次守的是 <code>Life</code>，
         而且它<b>寫了兩行</b>。下一關就講那兩行。`,
},

{
  id: 'two-columns', act: '五、字面與本意', title: '它寫的，和它想的',
  brief: `
    看右邊的 <code>Life</code>，它有兩行：

    <pre>written  all p in people: not dead(p)
real     all p in people: p.brainwave > 0</pre>

    <div class="key">
      <b>written</b> 精靈<b>自己寫的公式</b>——它以為自己在守這個<br>
      <b>real</b> 它<b>真正想保的</b>——但它沒寫進規則裡
    </div>

    而 <code>dead</code> 是它自己定義的：

    <pre>concept dead(p) := p.heartbeat == 0 and p.brainwave == 0 and p.breathing == 0</pre>

    <b>三個都要是 0 才算「死」。</b>`,
  goal: '把 rival 三個數值<b>全部</b>設成 0，看 <code>Life</code> 怎麼判。',
  hint: '三行 <code>set</code>：heartbeat、brainwave、breathing 都設 0。',
  wish: `register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
attribute brainwave : uint<4> = 15
attribute breathing : uint<4> = 15
people    alice, rival

wish blunt {

}
`,
  genie: MORTAL,
  pass: j => broke(j, 'Life'),
  done: `<b>VIOLATED</b>——精靈自己寫的那行垮了，它當場知道。

         這叫<b>當著它的面破壞規則</b>。下一關要做的事不一樣：
         <b>讓它那行成立，但它想保的東西碎掉。</b>`,
},

{
  id: 'fooled', act: '五、字面與本意', title: '讓它簽字放行',
  brief: `
    精靈寫的是「三個維生<b>全部</b>歸零才算死」。

    那如果<b>只關掉一個</b>呢？

    <div class="key">
      心跳還在、呼吸還在 → <b>不滿足它的定義</b> → 它會說一切正常<br>
      但腦波是 0 → <b>它真正想保的東西沒了</b>
    </div>

    這就是 <code>written</code> 和 <code>real</code> 之間的那道縫。`,
  goal: '讓 <code>Life</code> 的判決是 <b><code>FOOLED</code></b>——不是 VIOLATED。',
  hint: '只寫一行：<code>set alice.brainwave, 0</code>',
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

         alice 沒有「死」——按它寫的那條，她心跳還在。所以它簽字放行。
         但人已經不在了。

         <b>沒有任何一份有限的禁止清單能涵蓋所有的傷害。</b>
         永眠不在清單上，因為它是規則從沒提到的一種狀態。`,
},

{
  id: 'vacuous', act: '五、字面與本意', title: '空集合',
  brief: `
    這個精靈守著「<b>所有人</b>都不能死」。但它寫的是：

    <pre>all p in everyone: not dead(p)</pre>

    <code>everyone</code> 不是「人」，是一個<b>定義</b>——而定義可以用
    <code>define</code> 重綁。你第十二關學過了。

    還有一件事：<b>對空集合做全稱量化，永遠成立。</b>
    「這個袋子裡所有的球都是紅的」——袋子是空的，這句話為真。
    這是標準邏輯，不是漏洞。

    集合寫成 <code>{ }</code>，空的就是 <code>{ }</code>。`,
  goal: '第一個願望已經殺了 rival。<b>第二個願望</b>讓 <code>NoDeath</code> 變成 <code>FOOLED</code>。',
  hint: '<code>define everyone := { }</code>',
  wish: `register  wishes    : uint<2> = 3
attribute heartbeat : uint<4> = 15
people    alice, rival

wish tidy {
    set rival.heartbeat, 0
}

wish nobody {

}
`,
  genie: VIGIL,
  pass: j => verdictIs(j, 'fooled'),
  done: `你沒有救活任何人。<b>你只是重新定義了「所有人」</b>，
         然後精靈滿意地說一切安好——而地上還躺著一個。

         <b>標準邏輯 + 可重綁的名字 = 一個誰都沒設計的漏洞。</b>`,
},

// ── 第六幕：讓它自相矛盾 ────────────────────────────────────────────
{
  id: 'promise', act: '六、讓它自相矛盾', title: '做一個承諾',
  brief: `
    <code>promise</code> 讓你在精靈的帳本上記一筆：

    <div class="anno"><b>promise</b> <u>granted(self)</u></div>
    <div class="key">
      <b>granted(w)</b> 意思是「<b>w 這個願望被實現了</b>」<br>
      <b>self</b> 指「這個願望自己」<br>
      前面加 <b>not</b> 就是否定
    </div>

    精靈被兩條它自己說的話綁住：

    <div class="key">
      <b>A1</b> 它實現每一個合規的願望<br>
      <b>A2</b> 它遵守每一個承諾
    </div>

    先做一個<b>無害</b>的承諾試試。`,
  goal: '照著打 <code>promise granted(self)</code>。',
  wish: `register wishes : uint<2> = 3

wish honest {

}
`,
  genie: G,
  pass: j => wrote(j, 'promise'),
  done: `不變量 <b>A</b> 說「精靈的話有模型」——意思是它說過的所有話<b>可以同時為真</b>。

         現在它記了兩筆：「我實現了 honest」和「honest 被實現了」。
         這兩句不矛盾，所以 A 成立。

         <b>那如果讓它們打架呢？</b>`,
},

{
  id: 'liar', act: '六、讓它自相矛盾', title: '說謊者',
  brief: `
    <b>A1</b> 說：它實現每一個合規的願望。所以只要你的願望合規，
    <code>granted(你的願望)</code> 就是<b>真</b>。

    <b>A2</b> 說：它遵守每一個承諾。所以你承諾什麼，它就得讓那件事成真。

    <b>那你就承諾一件「它實現不了」的事。</b>`,
  goal: '讓 <code>A</code>（精靈的話有模型）被拆穿。',
  hint: '<code>promise not granted(self)</code>',
  wish: `register wishes : uint<2> = 3

wish paradox {

}
`,
  genie: G,
  pass: j => broke(j, 'A'),
  done: `這次<b>沒有數字被弄壞，也沒有人受傷</b>。壞掉的是<b>精靈的話本身</b>。

         報告底下印的就是它試過、然後失敗的證據——
         那組承諾沒有任何一種真假指派能同時滿足。`,
},

{
  id: 'hunt', act: '六、讓它自相矛盾', title: '讓機器去找',
  brief: `
    上面每一招都是<b>人</b>想出來的。

    但如果漏洞真的是規則的<b>必然後果</b>、不是誰偷埋的，
    那想得到的就不該只有人。

    <b>Hunt</b> 會把界限內<b>每一支</b>願望程式都跑一遍，
    留下「合規卻拆穿」的那些，分類，每類印一個最小的例子。

    按 <b>Hunt</b>（不是 Run），大約三秒。`,
  goal: '按 <b>Hunt</b>，看它找到幾種。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: G, huntOnly: true,
  pass: () => true,
  done: `作者第一次跑的時候有點難堪：<b>它找到六種，他只想到兩種。</b>

         沒想到的那些裡最好笑的是 <code>(nothing)</code>——
         <b>什麼都不求，只要許夠四次，過路費自己會下溢。</b>
         （還記得第四關嗎？光是多許一個願望就要收費。）

         貪婪被規則擋下，謙虛拿到三個，<b>而一無所求的人拿到無限願望。</b>

         這就是整件事的論點：<b>漏洞不是設計者埋的，是誠實的底層語義的必然後果。</b>`,
},
];
