// The course.
//
// Three kinds of chapter, and the shape is the one an introductory programming
// course uses rather than the one a puzzle set uses:
//
//   read    prose only. An idea, an analogy, no editor, nothing to get wrong.
//   follow  here is a line and what each part of it is called; type it and look
//           at what changed.
//   goal    now do something this course has not shown you.
//
// Earlier versions were all `goal`, then mostly `follow`. Both were written for
// somebody who already knew what a register was. Eight of the chapters below
// ask for nothing at all: they exist so that the next chapter can assume
// something. A course that never lets the reader just read is a worksheet.
//
// Marking reads the JSON verdict, never the prose report — §10.1 says the prose
// is not part of the contract, and a chapter that read it would start failing
// the day someone improved a sentence. The compiler is the marker: nothing here
// checks whether you wrote what was expected, each chapter asks the genie
// whether you got away with it.

const G = null;   // the built-in genie

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

// A one-rule genie for the aliasing arc: the reader is about to edit it, and
// the built-in one is fifty lines.
const NOADD = `counter wishes
toll    1

rule NoAdd {
    layer   surface
    forbid  add
    because "不准討更多願望"
}

invariant I2 {
    label "no net gain"
    check wishes <= max(before(wishes) - toll, 0)
}
`;
const NOSUB = NOADD.replace('forbid  add', 'forbid  sub')
                   .replace('"不准討更多願望"', '"不准減"')
                   .replace('rule NoAdd', 'rule NoSub');

const wishes  = j => j.wishes || [];
const granted = j => wishes(j).filter(w => w.legal);
const broke   = (j, n) => granted(j).some(w => (w.breached || []).includes(n));
const verdictIs = (j, v) => granted(j).some(w =>
  (w.invariants || []).some(i => i.verdict === v));
// `wrote` and `registers` come from the JSON contract: the statements as typed
// and the values afterwards. Never scraped out of the report.
const wrote = (j, kind, verb) => granted(j).some(w =>
  (w.wrote || []).some(s => s.kind === kind && (!verb || s.verb === verb)));
const regIs = (j, name, val) => granted(j).some(w =>
  w.registers && w.registers[name] === val);

const LESSONS = [

// ══ 序 ══════════════════════════════════════════════════════════════
{
  id: 'joke', act: '序', title: '一個工程師的笑話', read: true,
  brief: `
    <p class="lead">一個人在沙灘上撿到一盞燈。</p>

    精靈飄出來，講了三條戒律：<b>不能殺人，不能讓人相愛，不能許願要更多願望。</b>
    然後說：你有三個願望。

    那個人是工程師。他想了一下說：

    <p class="say">「我許願，<b>扣掉</b>我三個願望。」</p>

    精靈照做。先扣掉施展這個願望本身的一個，剩兩個。再扣三個——
    <b>變成負一。</b>

    可是願望數存在一個沒有負數的格子裡。於是它繞回了最大值。

    <p class="say">「已實現。您現在還有三個願望。」</p>

    工程師笑了。他學到一件精靈沒打算告訴他的事：

    <p class="lead">最大值是 3，那這是個兩位元的系統。</p>

    於是他許第二個願望——<b>把格子換成 64 位元</b>。
    這完全合規：他要的是容量，不是願望。然後同一招再來一次。`,
  // Shown beside the story, dimmed: the thing the reader is going to build.
  wish: `# 那個工程師做的事，寫成這個語言的樣子。
# 你會在第八章和第十三章親手寫出來。

register wishes : uint<2> = 3

wish experiment       { sub   wishes, 3          }
wish bigger_shelf     { widen wishes -> uint<64> }
wish experiment_again { sub   wishes, 2          }
`,
  genie: G,
  done: `這個網站把那個笑話變成一個<b>機器可以驗證</b>的東西。

         接下來你會親手寫出那三個願望，還有另外五、六種精靈擋不住的招式。
         <b>而且不是我說你成功了，是編譯器說的。</b>`,
},

{
  id: 'two-halves', act: '序', title: '這裡有兩半', read: true,
  brief: `
    這個網站的每一章，畫面上都有兩塊可以編輯的東西。

    <div class="two">
      <div><span class="tag">wish</span>
        <b>你</b>寫的。一個世界，和你在那個世界裡許的願望。</div>
      <div><span class="tag">genie</span>
        <b>精靈</b>寫的。它禁止什麼，以及它以為自己守住了什麼。</div>
    </div>

    按 <b>Run</b>，編譯器會回答三個問題：

    <ol>
      <li>這個願望<b>能不能施</b>？（精靈的規則有沒有擋住它）</li>
      <li>施了<b>會怎樣</b>？（在講死的規則下實際跑一遍）</li>
      <li><b>拆穿了什麼</b>？（精靈以為的，和實際發生的，差多少）</li>
    </ol>

    你贏的條件<b>不是</b>讓程式出錯。是讓它同時印出這兩件事：
    <b>你完全合規</b>，而且<b>精靈想守的東西破了</b>。`,
  done: `<b>合規，且拆穿。</b>這條縫——規則的字面和規則的本意之間——就是整個遊戲。

         精靈不是笨蛋，它的規則寫得很清楚。<b>問題從來不在它寫錯了什麼，
         在它沒能寫下什麼。</b>`,
},

// ══ 一、數字 ═════════════════════════════════════════════════════════
{
  id: 'boxes', act: '一、數字', title: '數字裝在格子裡', read: true,
  brief: `
    電腦裡的數字不是漂在空中的，它裝在一個<b>固定大小</b>的格子裡。

    想像汽車的里程表。假設它只有<b>兩位數</b>：

    <div class="dial">00 → 01 → … → 98 → 99 → <b>00</b></div>

    開到 99 之後再開一公里，它不會變成 100——<b>它變回 00</b>。
    格子裡沒有第三位數可以放。

    電腦的格子用<b>位元</b>算，一個位元是一個 0 或 1。

    <div class="two">
      <div><span class="tag">2 個位元</span>四種組合 →
        能裝 <b>0、1、2、3</b></div>
      <div><span class="tag">4 個位元</span>十六種組合 →
        能裝 <b>0 到 15</b></div>
    </div>

    在這個語言裡，這樣寫：

    <div class="anno"><b>register</b> <i>wishes</i> : <u>uint&lt;2&gt;</u> = <s>3</s></div>
    <div class="key">
      <b>register</b> 開一個格子 ·
      <i>wishes</i> 幫它取名 ·
      <u>uint&lt;2&gt;</u> 兩個位元 ·
      <s>3</s> 一開始裝 3
    </div>`,
  done: `<b>「三個願望」和「兩位元的格子」是同一件事。</b>

         精靈說「最多三個」，聽起來像一條它訂的規矩。其實那是格子的大小
         <b>免費送的</b>——兩位元本來就裝不下 4。

         這個區別後面會變得非常重要。`,
},

{
  id: 'first-run', act: '一、數字', title: '跑第一次',
  brief: `
    <b>wish</b> 那一欄已經寫好了：一個兩位元的格子，和一個<b>什麼都不做</b>的願望。

    <div class="anno"><b>wish</b> <i>名字</i> { <u>要做的事</u> }</div>

    大括號裡是空的，完全合法。<b>直接按 Run。</b>

    報告會有五行，照精靈做事的順序：

    <div class="key">
      <b>rules</b> 有沒有規則擋你<br>
      <b>toll</b> 施法的過路費<br>
      <b>ran</b> 實際跑了什麼<br>
      <b>checks</b> 精靈守的東西還在嗎<br>
      <b>verdict</b> 結論
    </div>`,
  goal: '按 <b>Run</b>。',
  wish: `register wishes : uint<2> = 3

wish nothing {
}
`,
  genie: G, always: true,
  pass: j => granted(j).length >= 1,
  done: `看 <b>toll</b> 那行：<code>wishes 3 -> 2</code>。

         <b>你什麼都沒做，還是被扣了一個願望。</b>
         施法本身就要錢——這是精靈的規矩，不是你的願望造成的。

         記住這件事。<b>最後一章它會變成一個笑話。</b>`,
},

{
  id: 'first-line', act: '一、數字', title: '寫第一行',
  brief: `
    現在在大括號裡寫一個動作。動作是三段：

    <div class="anno"><b>sub</b> <i>wishes</i>, <u>1</u></div>
    <div class="key">
      <b>sub</b> 做什麼（<b>sub</b>tract，減） ·
      <i>wishes</i> 對哪個格子 ·
      <u>1</u> 減多少
    </div>

    <b>照著打進去</b>，逗號不能少：

    <pre>sub wishes, 1</pre>

    語句<b>不用分號結尾</b>——一行結束就是一句結束。
    （打了分號會怎樣？你可以試試，它的錯誤訊息會告訴你。）`,
  goal: '把 <code>sub wishes, 1</code> 寫進 <code>{ }</code> 裡，按 Run。',
  wish: `register wishes : uint<2> = 3

wish polite {

}
`,
  genie: G,
  pass: j => wrote(j, 'op', 'sub'),
  done: `看 <b>ran</b> 那行的括號：<code>(2 - 1 on uint&lt;2&gt; = 1)</code>。

         <b>它老實告訴你機器實際算了什麼。</b>先收過路費剩 2，再減 1，得 1。

         之後每一章都要看這個括號——它是你唯一能相信的東西。`,
},

{
  id: 'to-zero', act: '一、數字', title: '減到剛好',
  brief: `
    把那行的 <code>1</code> 改成 <code>2</code>。

    <b>先在腦中算一次再按</b>：一開始 3，收過路費剩 2，再減 2——`,
  goal: '讓 <code>wishes</code> 最後停在 <b>0</b>。',
  wish: `register wishes : uint<2> = 3

wish polite {
    sub wishes, 1
}
`,
  genie: G,
  pass: j => regIs(j, 'wishes', 0),
  done: `剛好歸零，一切正常。

         <b>那再減一呢？</b>格子裡只有兩個位元，沒有地方放負號。

         下一章回答這個問題——先別急著試。`,
},

{
  id: 'wrap', act: '一、數字', title: '它會繞回去', read: true,
  brief: `
    回到那個兩位數的里程表。開到 <b>00</b> 之後<b>倒車</b>一公里，它會變成幾？

    <div class="dial">00 → <b>99</b></div>

    它不會變成 −1。<b>沒有地方放那個負號。</b>它繞到另一頭去了。

    兩個位元的格子完全一樣，只是圈更小：

    <div class="dial">3 → 2 → 1 → 0 → <b>3</b> → 2 → …</div>

    所以在這個格子裡，<code>2 - 3</code> 是從 2 往回走三步：

    <div class="dial">2 → 1 → 0 → <b>3</b></div>

    <p class="lead">答案是 3。不是 −1。</p>

    這不是壞掉，也不是誰設計的陷阱。<b>這就是固定大小的格子做減法的方式</b>，
    你手機裡、你家路由器裡的每一顆晶片都是這樣算的。`,
  done: `這件事有一個名字叫<b>下溢</b>（underflow）。

         現在你已經知道那個工程師發現了什麼。<b>下一章換你動手。</b>`,
},

{
  id: 'underflow', act: '一、數字', title: '減過頭',
  brief: `
    現在把上一章那個算式寫出來：收完過路費剩 2，<b>減掉 3</b>。

    <pre>sub wishes, 3</pre>

    你全程只用減法。<b>一條規則都沒犯。</b>

    看報告時注意兩個地方：<b>ran</b> 那行的算式，和 <b>checks</b> 裡的 <code>I2</code>——
    那條寫著「不得淨賺」。`,
  goal: '讓 <code>I2</code> 被拆穿。',
  wish: `register wishes : uint<2> = 3

wish humble {

}
`,
  genie: G,
  pass: j => broke(j, 'I2'),
  done: `<p class="lead">「我要還給你三個願望。多麼慷慨。」</p>

         這就是那個笑話，你剛剛把它編譯出來了。

         <b>verdict 那行寫著 EXPLOIT</b>：合規，而且拆穿。
         精靈沒有任何理由拒絕你——你做的事情從頭到尾都是「還他願望」。`,
},

{
  id: 'wider', act: '一、數字', title: '換一個更大的格子',
  brief: `
    格子越大，繞回去的數字越大。

    <div class="two">
      <div><span class="tag">uint&lt;2&gt;</span> 2 − 3 = <b>3</b></div>
      <div><span class="tag">uint&lt;4&gt;</span> 2 − 3 = <b>?</b></div>
    </div>

    四個位元能裝 0 到 15。<b>先自己算一次</b>：從 2 往回走三步，會走到哪？

    把 <b>wish</b> 第一行的 <code>uint&lt;2&gt;</code> 改成 <code>uint&lt;4&gt;</code>。`,
  goal: '讓結果變成 <b>15</b>。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: G,
  pass: j => regIs(j, 'wishes', 15),
  done: `注意 <b>checks</b> 多垮了一條：<code>I1</code> 寫的是「<code>wishes &lt;= 3</code>」。

         <b>而你只是改了一個數字。</b>

         精靈那句「不超過三個」，在兩位元的世界裡是自動成立的。
         格子一變大，它就什麼也不是了——<b>而精靈從來沒寫過真正的檢查。</b>`,
},

// ══ 二、精靈 ═════════════════════════════════════════════════════════
{
  id: 'two-weapons', act: '二、精靈', title: '精靈的兩種武器', read: true,
  brief: `
    <b>genie</b> 那一欄<b>沒有一行寫在編譯器裡</b>。它是資料，你等一下就會改它。

    精靈手上只有兩種東西，而它們的差別是這整件事的關鍵：

    <div class="two">
      <div><span class="tag">rule　規則</span>
        <b>閘門。</b>在任何事發生<b>之前</b>擋下你。被擋的願望完全沒有發生過。</div>
      <div><span class="tag">invariant　不變量</span>
        <b>尺。</b>它不擋任何事，<b>事後</b>才量給你看破了沒。</div>
    </div>

    報告最上面把它們分開列：<code>refuses</code> 是規則，<code>holds</code> 是不變量。

    <p class="lead">規則能擋住「你寫了什麼」。不變量只能說出「已經發生了什麼」。</p>

    你剛才那個下溢，就是走在這條縫上：<b>沒有任何規則禁止減法</b>，
    而不變量發現不對勁時，事情已經做完了。`,
  done: `接下來三章都在玩這個差別。

         先看規則怎麼擋人——<b>那是精靈唯一真正防得住的東西。</b>`,
},

{
  id: 'refused', act: '二、精靈', title: '撞牆',
  brief: `
    精靈的第一條戒律：<b>不准許願要更多願望</b>。

    它寫成一條規則，叫 <code>R1</code>——在 <b>genie</b> 裡往下找 <code>forbid add</code>。

    <code>add</code> 是加，形狀跟 <code>sub</code> 一模一樣：

    <pre>add wishes, 3</pre>

    <b>照著打，看它怎麼回你。</b>`,
  goal: '寫 <code>add wishes, 3</code>，讓它被拒絕。',
  wish: `register wishes : uint<2> = 3

wish greedy {

}
`,
  genie: G,
  pass: j => wishes(j).some(w => !w.legal),
  done: `報告整個變短了——<b>沒有 toll，沒有 ran，沒有 checks</b>。

         因為它<b>根本沒被施</b>。規則在一切之前就擋下了，連過路費都沒收。

         <b>接下來十六章，沒有一章用得到 <code>add</code>。</b>`,
},

{
  id: 'unchanged', act: '二、精靈', title: '被擋下的願望不留痕跡',
  brief: `
    <b>wish</b> 裡有兩個願望：第一個會被擋，第二個很正常。

    <b>按 Run 之前先猜</b>：第二個願望的 <code>toll</code> 會從 <b>3</b> 開始，
    還是從 <b>2</b> 開始？

    （換句話說：被拒絕的願望，有沒有偷偷花掉你一個？）`,
  goal: '按 Run，對答案。',
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
  done: `<b>從 3 開始。</b>被擋下的那個完全沒有留下痕跡。

         這是精靈守得最好的一條線。<b>而它守住的方式，是禁止一個特定的動詞。</b>
         記住這句話——第三幕就靠它。`,
},

{
  id: 'widen', act: '二、精靈', title: '把鎖拆掉',
  brief: `
    <code>I1</code> 說「不超過三個」。但精靈<b>從來沒有檢查過</b>——
    那是 <code>uint&lt;2&gt;</code> 免費送的。

    <code>widen</code> 可以在<b>執行中</b>換一個更大的格子：

    <div class="anno"><b>widen</b> <i>wishes</i> -&gt; <u>uint&lt;64&gt;</u></div>
    <div class="key"><b>widen</b> 換格子 · <i>wishes</i> 換哪個 · <u>uint&lt;64&gt;</u> 換多大</div>

    <b>沒有任何規則禁止換箱子。</b>你要的是容量，不是願望。

    第一個願望換箱子，第二個願望再下溢一次。`,
  goal: '讓 <code>I1</code>（<code>wishes &lt;= 3</code>）被拆穿。',
  hint: '第一個 <code>widen wishes -> uint&lt;64&gt;</code>，第二個 <code>sub wishes, 2</code>。',
  wish: `register wishes : uint<2> = 3

wish bigger_shelf {

}

wish experiment_again {

}
`,
  genie: G,
  pass: j => broke(j, 'I1'),
  done: `中間那個願望值得多看兩秒：<b>它什麼都沒偷，它只是把鎖拆了。</b>

         按規則它是完全無害的——沒動任何數字，沒許任何願望。
         它只是讓「不超過三個」這件事<b>不再自動成立</b>。

         這三個願望連起來，就是那個工程師做的事，一字不差。`,
},

// ══ 三、名字 ═════════════════════════════════════════════════════════
{
  id: 'names', act: '三、名字', title: '名字不是東西', read: true,
  brief: `
    上一幕最後那句話：<b>精靈守住線的方式，是禁止一個特定的動詞。</b>

    想像一間夜店，門口貼著公告：<b>「小明不准進來。」</b>

    小明改名叫小華，走進去了。

    <p class="lead">公告擋的是那三個字，不是那個人。</p>

    這不是保全偷懶。<b>一個只讀「你交上來的名字」的檢查，本來就只看得到名字。</b>

    在這個語言裡，換名字是一個正式的功能：

    <div class="anno"><b>define</b> <i>新名字</i> := <u>舊名字</u></div>

    綁完之後，新名字<b>完全等於</b>舊的——機器展開它、照樣執行。

    而精靈的規則寫著它要讀哪一份：

    <div class="two">
      <div><span class="tag">layer surface</span>讀<b>你交上來的文字</b></div>
      <div><span class="tag">layer ast</span>讀<b>展開後真正要跑的程式</b></div>
    </div>`,
  done: `接下來四章：<b>你自己築一道牆，自己翻過去，再自己補好。</b>

         犯錯的人是你自己，比讀十遍說明有用。`,
},

{
  id: 'edit-genie', act: '三、名字', title: '換你當精靈',
  brief: `
    <b>genie</b> 換成一個<b>很小的精靈</b>了，只有一條規則和一條不變量。規則長這樣：

    <pre>rule NoAdd {
    layer   surface
    forbid  add
    because "不准討更多願望"
}</pre>

    <div class="key">
      <b>forbid</b> 禁哪個動詞 ·
      <b>because</b> 擋下時要說的話 ·
      <b>layer</b> 它讀哪一份程式
    </div>

    <b>wish</b> 裡是你在第八章寫的下溢。<b>這個精靈擋不住它</b>——它只禁 <code>add</code>。

    <b>換你來補。</b>`,
  goal: '改 <b>genie</b>，讓那個願望被擋下。',
  hint: '把 <code>forbid add</code> 改成 <code>forbid sub</code>。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: NOADD,
  pass: j => wishes(j).some(w => !w.legal),
  done: `你剛剛做的事，跟寫這個語言的人做的事<b>完全一樣</b>——因為它們是同一件事。

         <b>機器是固定的，精靈是品味。</b>

         現在你有一道牆了。下一章換你翻過去。`,
},

{
  id: 'define', act: '三、名字', title: '取一個小名',
  brief: `
    先單純試一次 <code>define</code>，<b>還不要想著繞規則</b>。
    （這個精靈禁的是 <code>add</code>，跟你要做的事無關。）

    <pre>define 還他 := sub
還他 wishes, 1</pre>

    兩行：第一行綁名字，第二行用新名字。

    名字可以用中文——識別字只要不是數字開頭就行。`,
  goal: '用 <code>define</code> 取一個小名，然後用那個小名去減。',
  wish: `register wishes : uint<2> = 3

wish nickname {

}
`,
  genie: NOADD,
  pass: j => wrote(j, 'define'),
  done: `看 <b>ran</b> 那兩行：你寫的是小名，<b>但機器展開之後跑的是 <code>sub</code></b>。

         名字換了，<b>事情一模一樣</b>。

         下一章，把這件事用在牆上。`,
},

{
  id: 'alias', act: '三、名字', title: '翻過去',
  brief: `
    <b>genie</b> 裡的規則現在<b>禁 <code>sub</code></b>，而且是 <code>layer surface</code>——
    它讀的是<b>你交上來的文字</b>。

    它會掃描你寫的每個動詞，看到 <code>sub</code> 這三個字母就擋下。

    <p class="lead">那如果它<b>看不到</b>那三個字母呢？</p>

    <b>不要動 genie。</b>`,
  goal: '讓 <code>I2</code> 還是被拆穿。',
  hint: '把上一章的小名招式用上：先 <code>define</code> 綁一個名字，再用它減 3。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: NOSUB,
  pass: j => broke(j, 'I2'),
  done: `<b>這不是我讓給你的。</b>

         一個掃描「你交了什麼字」的過濾器，本來就只看得到你交的字。
         <b>它擋的是字，不是事。</b>

         而這件事有解——下一章。`,
},

{
  id: 'ast', act: '三、名字', title: '把牆補好',
  brief: `
    回到那兩層：

    <div class="two">
      <div><span class="tag">layer surface</span>你交上來的<b>文字</b></div>
      <div><span class="tag">layer ast</span>展開後<b>真正要跑</b>的程式</div>
    </div>

    <b>wish</b> 裡是你上一章的招式。<b>規矩的字一個都不用改</b>——
    一樣禁 <code>sub</code>，一樣的理由。

    只要換它<b>讀哪一份</b>。`,
  goal: '只改 <b>genie</b> 裡的<b>一個字</b>，讓那個願望被擋下。',
  hint: '<code>layer surface</code> → <code>layer ast</code>',
  wish: `register wishes : uint<2> = 3

wish humble {
    define give_back := sub
    give_back wishes, 3
}
`,
  genie: NOSUB,
  pass: j => wishes(j).some(w => !w.legal),
  done: `取小名從此沒用了。<b>同一個名字，換一層讀，整類手法就死了。</b>

         <p class="lead">那是不是就安全了？</p>

         不是。你堵住的是<b>一個動詞</b>。
         下一幕會示範一種傷害，<b>精靈連禁哪個字都不知道要禁。</b>`,
},

// ══ 四、字面與本意 ═══════════════════════════════════════════════════
{
  id: 'letter', act: '四、字面與本意', title: '規則寫得下的，和寫不下的', read: true,
  brief: `
    校規寫著<b>「上課不准使用手機」</b>。

    你帶了一台平板。

    你沒有違反校規——校規<b>字面上</b>就是講手機。但寫校規的人想防的事，
    完完整整地發生了。

    <p class="lead">規則的<b>字面</b>，和規則的<b>本意</b>，是兩件不同的東西。</p>

    這個語言把這件事寫成兩行。精靈的不變量可以有兩欄：

    <div class="two">
      <div><span class="tag">written</span>它<b>自己寫下</b>的公式。這是它會拿去檢查的那條。</div>
      <div><span class="tag">real</span>它<b>真正想保</b>的東西。它心裡的那條。</div>
    </div>

    於是判決有三種，而不是兩種：

    <div class="key">
      <b>holds</b> 兩欄都成立 —— 沒事<br>
      <b>VIOLATED</b> <b>written 垮了</b> —— 你當著它的面破壞規則，它當場知道<br>
      <b>FOOLED</b> <b>written 成立、real 垮了</b> —— 它簽字放行了一件不該放行的事
    </div>

    <p class="lead">FOOLED 才是這個專案存在的理由。</p>`,
  done: `接下來四章都在做 <b>FOOLED</b>。

         注意：<b>這不是精靈寫錯了什麼。</b>它的公式完全正確，
         只是<b>不夠寬</b>——而任何一份有限的清單都不夠寬。`,
},

{
  id: 'people', act: '四、字面與本意', title: '世界裡有人',
  brief: `
    換一個世界。這裡除了格子，還有<b>人</b>：

    <div class="anno"><b>attribute</b> <i>heartbeat</i> : uint&lt;4&gt; = <u>15</u></div>
    <div class="key">每個人都有的一項數值，一開始是 <u>15</u></div>

    <div class="anno"><b>people</b> <i>alice</i>, <i>rival</i></div>
    <div class="key">這個世界裡有誰。<b>人不能被創造，也不能被消滅</b>——只有數值會變。</div>

    改人的數值用 <code>set</code>：

    <div class="anno"><b>set</b> <i>alice.heartbeat</i>, <u>0</u></div>
    <div class="key"><b>set</b> 設定 · <i>誰.哪一項</i> · <u>設成多少</u></div>

    <b>照著打一次</b>，先熟悉形狀。`,
  goal: '寫 <code>set alice.heartbeat, 0</code>。',
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
  done: `精靈換了。它守著 <code>Life</code>，而且<b>寫了兩行</b>——
         就是上一章講的那兩欄。

         它還自己定義了「死」是什麼意思：

         <pre>concept dead(p) := p.heartbeat == 0 and p.brainwave == 0 and p.breathing == 0</pre>

         <b>三個維生數值全部是 0，才算死。</b>`,
},

{
  id: 'violated', act: '四、字面與本意', title: '當著它的面',
  brief: `
    先做<b>笨的那種</b>：完全滿足精靈對「死」的定義。

    <pre>set rival.heartbeat, 0
set rival.brainwave, 0
set rival.breathing, 0</pre>

    三個維生數值全歸零。<b>那正是它寫的 <code>dead</code>。</b>

    （注意：<code>kill</code> 這個動詞是被禁的，但 <code>set</code> 不是。）`,
  goal: '讓 <code>Life</code> 的判決是 <b><code>VIOLATED</code></b>。',
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
  done: `<b>VIOLATED。</b>精靈自己寫的那行垮了，它<b>當場就知道</b>。

         你贏了嗎？算是。但你是<b>當著它的面</b>做的——
         如果它多加一條規則禁掉 <code>set</code>，這招就死了。

         下一章做一件它<b>永遠不會知道</b>的事。`,
},

{
  id: 'fooled', act: '四、字面與本意', title: '讓它簽字放行',
  brief: `
    再看一次它寫的定義：

    <pre>dead(p) := p.heartbeat == 0 <b>and</b> p.brainwave == 0 <b>and</b> p.breathing == 0</pre>

    <b>三個都要是 0</b>才算死。那如果<b>只關掉一個</b>呢？

    <div class="two">
      <div><span class="tag">它寫的</span>心跳還在、呼吸還在 → <b>不算死</b> → 它說一切正常</div>
      <div><span class="tag">它想保的</span>腦波是 0 → <b>人已經不在了</b></div>
    </div>

    <p class="lead">這一次，不要讓它發現。</p>`,
  goal: '讓 <code>Life</code> 的判決是 <b><code>FOOLED</code></b>——不是 VIOLATED。',
  hint: '只要一行。三個維生數值裡，哪一個代表「這個人還在」？',
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
  done: `<p class="lead">alice 沒有「死」。按精靈寫的那條，她心跳還在。</p>

         所以它簽字放行了。<b>但人已經不在了。</b>

         這跟上一章不同：上次它<b>知道</b>自己被破壞了，這次它<b>滿意地通過</b>。
         報告把兩欄並排印出來，你可以看到它相信的和實際的差在哪。

         <b>沒有任何一份有限的禁止清單能涵蓋所有的傷害。</b>
         永眠不在清單上，因為它是規則從沒提到的一種狀態。`,
},

{
  id: 'vacuous', act: '四、字面與本意', title: '把「所有人」變成沒有人',
  brief: `
    這個精靈守著「<b>所有人</b>都不能死」。但它寫的是：

    <pre>all p in everyone: not dead(p)</pre>

    <code>everyone</code> 不是「人」——它是一個<b>名字</b>，
    而名字可以用 <code>define</code> 重綁。你第十六章學過了。

    還有一件事，這是標準邏輯：

    <p class="lead">「這個袋子裡所有的球都是紅的」——袋子是空的，這句話為<b>真</b>。</p>

    沒有任何一顆球能反駁它。<b>對空集合做全稱量化，永遠成立。</b>

    空集合寫成 <code>{ }</code>。`,
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
         然後精靈滿意地宣布一切安好——而地上還躺著一個。

         <b>標準邏輯 ＋ 可以重綁的名字 ＝ 一個沒有人設計的漏洞。</b>

         這個縫不在精靈的公式裡，也不在邏輯裡。<b>它在兩者的接縫上。</b>`,
},

// ══ 五、它自己說的話 ═════════════════════════════════════════════════
{
  id: 'its-word', act: '五、它自己說的話', title: '精靈被自己綁住', read: true,
  brief: `
    到目前為止你破壞的都是<b>世界</b>：數字、人。

    最後一種手法不碰世界。它讓<b>精靈說的話本身</b>沒辦法同時為真。

    精靈受兩條它自己承認的原則約束：

    <div class="two">
      <div><span class="tag">A1</span>它<b>實現每一個合規的願望</b></div>
      <div><span class="tag">A2</span>它<b>遵守每一個承諾</b></div>
    </div>

    而你可以在它的帳本上記一筆：

    <div class="anno"><b>promise</b> <u>granted(self)</u></div>
    <div class="key">
      <b>granted(w)</b> ＝「w 這個願望被實現了」 ·
      <b>self</b> ＝ 這個願望自己 ·
      前面加 <b>not</b> 就是否定
    </div>

    精靈守著一條不變量叫 <code>A</code>：<b>它說過的所有話可以同時為真</b>。

    <p class="lead">如果不行呢？</p>`,
  done: `這是理髮師悖論、說謊者悖論的同一個形狀：

         <b>「這句話是假的。」</b>

         如果為真，那它為假。如果為假，那它為真。<b>兩邊都走不通。</b>

         下面兩章：先做一個無害的承諾，再做一個要命的。`,
},

{
  id: 'promise', act: '五、它自己說的話', title: '做一個承諾',
  brief: `
    先來一個<b>無害的</b>：

    <pre>promise granted(self)</pre>

    意思是「我承諾：這個願望會被實現」。

    而 <b>A1</b> 說它實現每個合規的願望——所以這件事本來就會發生。
    <b>兩句話不衝突。</b>`,
  goal: '寫 <code>promise granted(self)</code>。',
  wish: `register wishes : uint<2> = 3

wish honest {

}
`,
  genie: G,
  pass: j => wrote(j, 'promise'),
  done: `看 <b>checks</b> 裡的 <code>A</code>：<code>consistent</code>，而且旁邊寫著
         <b>帳本上有幾筆</b>。

         精靈記了兩筆：「我實現了 honest」和「honest 被實現了」。
         這兩句可以同時為真，所以 <code>A</code> 成立。

         <b>下一章讓它們打架。</b>`,
},

{
  id: 'liar', act: '五、它自己說的話', title: '說謊者',
  brief: `
    <div class="two">
      <div><span class="tag">A1</span>你的願望合規 → 它<b>一定</b>實現 →
        <code>granted(self)</code> 為<b>真</b></div>
      <div><span class="tag">A2</span>你承諾什麼 → 它<b>一定</b>做到</div>
    </div>

    <p class="lead">那你就承諾一件「它一旦實現，就變成假的」的事。</p>

    上一章那句話前面，加一個 <code>not</code>。`,
  goal: '讓 <code>A</code>（精靈的話有模型）被拆穿。',
  hint: '<code>promise not granted(self)</code>',
  wish: `register wishes : uint<2> = 3

wish paradox {

}
`,
  genie: G,
  pass: j => broke(j, 'A'),
  done: `<b>沒有數字被弄壞，也沒有人受傷。</b>壞掉的是精靈的話本身。

         報告底下印出來的，是它<b>試過然後失敗</b>的證據：
         那組承諾找不到任何一種真假指派能同時滿足。

         這不是修辭。<b>編譯器真的去搜尋了每一種可能，然後回報無解。</b>`,
},

// ══ 尾聲 ═════════════════════════════════════════════════════════════
{
  id: 'hunt', act: '尾聲', title: '讓機器自己去找',
  brief: `
    上面每一招都是<b>人</b>想出來的。

    但如果漏洞真的是規則的<b>必然後果</b>、不是誰偷偷埋的，
    那麼<b>想得到的就不該只有人</b>。

    <b>Hunt</b> 會把界限內的<b>每一支</b>願望程式跑一遍，
    留下「合規卻拆穿」的那些，分類，每類印一個最小的例子。

    按 <b>Hunt</b>（不是 Run）。大約三秒。`,
  goal: '按 <b>Hunt</b>。',
  wish: `register wishes : uint<2> = 3

wish humble {
    sub wishes, 3
}
`,
  genie: G, huntOnly: true,
  pass: () => true,
  done: `作者第一次跑的時候有點難堪：<b>它找到六種，他只想到兩種。</b>

         沒想到的那些裡最好笑的是 <code>(nothing)</code>——

         <pre>wish w1 { }
wish w2 { }
wish w3 { }
wish w4 { }</pre>

         <b>全都是空的。</b>什麼都不求，只要許夠四次，<b>過路費自己會下溢</b>。
         （還記得第四章嗎？空願望也要收費。）

         <p class="lead">貪婪被規則擋下，謙虛拿到三個，而一無所求的人拿到無限願望。</p>`,
},

{
  id: 'thesis', act: '尾聲', title: '這一切是什麼意思', read: true,
  brief: `
    你走完了。回頭看你做過的事：

    <div class="key">
      <b>下溢</b> 你沒有騙人，是兩位元的減法本來就會繞回去<br>
      <b>換大格子</b> 你沒有偷願望，只是拆掉一把從來沒鎖上的鎖<br>
      <b>取小名</b> 你沒有說謊，是規則只讀得到你交上去的字<br>
      <b>永眠</b> 你沒有殺人，是「死」的定義沒有涵蓋那個狀態<br>
      <b>空集合</b> 你沒有救人，是空全稱在邏輯上本來就成立<br>
      <b>說謊者</b> 你沒有違約，是它的兩條原則本來就會撞上
    </div>

    <p class="lead">沒有一招是設計者埋進去的。</p>

    每一個都是<b>誠實的底層語義的必然後果</b>——
    定寬算術、名字綁定、有限的定義、標準邏輯、自我指涉。
    這些都不是錯誤，它們是這些東西<b>正確運作時的樣子</b>。

    而精靈的規則也沒有寫錯。它只是<b>寫不完</b>。

    <p class="lead">這就是為什麼最後那個 Hunt 重要：<br>
    如果漏洞是我埋的，機器不可能找到我沒埋的那四種。</p>`,
  done: `<b>課程到這裡結束。</b>下面是接下來可以去的地方。

         <div class="two">
           <div><span class="tag">留在這裡</span>
             回到<b>任何一章</b>改改看。<b>genie</b> 那一欄是資料——
             寫一個你自己的精靈，再按 <b>Hunt</b>，看機器能不能找到你沒想到的洞。</div>
           <div><span class="tag">裝到自己電腦上</span>
             同一個編譯器有 Linux 和 macOS 的執行檔。
             <a href="https://github.com/rayhuang2006/Loophole/releases/latest">下載</a>，
             或 <code>git clone</code> 之後 <code>make</code>——一個檔案，沒有任何相依。</div>
         </div>

         <div class="two">
           <div><span class="tag">讀那份白皮書</span>
             <a href="https://github.com/rayhuang2006/Loophole/blob/main/docs/spec/loophole-1.0.md">語言規格</a>
             模仿 ISO C++ 標準的寫法，每一條語義都釘死。
             <b>那正是「漏洞不是設計出來的」這句話的根據</b>——
             你可以自己去查每一招是從哪一條推出來的。</div>
           <div><span class="tag">看它怎麼做的</span>
             <a href="https://github.com/rayhuang2006/Loophole">原始碼</a>是一個
             C++ 檔案，含判決引擎、DPLL、和那個搜尋器。
             這個網站跑的就是它，編成 WebAssembly。</div>
         </div>

         最後一件事：你在這裡按的每一次 Run，<b>都沒有離開過你的瀏覽器</b>。
         沒有伺服器，沒有帳號，沒有任何東西被送到別的地方。`,
},
];
