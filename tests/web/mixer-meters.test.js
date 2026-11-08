// Every mixer strip's level bars are wide enough to see and move while the song plays; the piano hands get their own EQ suggestions.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch({args:['--autoplay-policy=no-user-gesture-required']}); const p=await b.newPage({viewport:{width:1282,height:837}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; xAdd('piano'); shellDock('mix'); }); await p.waitForTimeout(400);
 await p.evaluate(()=>{ ensureAudio(); play('song'); }); await p.waitForTimeout(2500);
 const m=await p.evaluate(()=>[...document.querySelectorAll('#cMix .strip.st2[data-ch] .sf-m .vu')].map(v=>{ const bars=[...v.querySelectorAll('i')].map(i=>i.getBoundingClientRect().width), lit=[...v.querySelectorAll('i')].map(x=>x.style.getPropertyValue('--p')); return {k:v.dataset.vu,bars,lit}; }));
 const thin=m.filter(x=>x.bars.some(w=>w<2)); ok(m.length>=6&&!thin.length,'every strip has two level bars at least 2 px wide '+JSON.stringify(thin.slice(0,3)));
 const moving=m.filter(x=>x.lit.some(s=>parseFloat(s)>0)).map(x=>x.k); ok(['song:lead','song:bass','song:drums'].every(k=>moving.includes(k))&&moving.some(k=>/song:x/.test(k)),'the level bars move for the melody, bass, drums and the piano '+JSON.stringify(moving));
 const eq=await p.evaluate(()=>xList().filter(x=>/^piano/.test(x.role)).map(x=>({role:x.role,n:(eqSuggest(x.id)||[]).length}))); ok(eq.length===2&&eq.every(x=>x.n>=2),'both piano hands get EQ suggestions '+JSON.stringify(eq));
 const fx=await p.evaluate(()=>{ const q=sel=>!!document.querySelector('#cMix .strip.st2[data-ch="fx"] '+sel); return {eq:q('.eq-mini:not(.eq-none)'),width:q('[data-kn="fx:width"]'),smart:q('[data-kn="fx:smart"]'),comp:q('.pcb'),m:q('[data-ms="m"]'),s:q('[data-ms="s"]'),rec:q('[data-starm]'),ch:nativeFxCh('fx'),compOn:pcActive('fx')}; });
 ok(fx.eq&&fx.width&&fx.smart&&fx.comp&&fx.m&&fx.s&&!fx.rec&&fx.ch!=null&&fx.ch!==9&&!fx.compOn,'the Transitions strip has EQ, Width, the smart knob, Comp (off to start), M and S like the others, and its own channel in the app '+JSON.stringify(fx));
 await p.evaluate(()=>stripMS('fx','m')); const mu=await p.evaluate(()=>({on:S.transOn,strip:!!document.querySelector('#cMix .strip[data-ch="fx"] [data-ms="m"][aria-pressed="true"]')})); await p.evaluate(()=>stripMS('fx','m'));
 ok(!mu.on&&mu.strip,'M on Transitions mutes them and the strip stays so you can unmute '+JSON.stringify(mu));
 ok(!errs.length,'no page errors '+errs.join(' | ')); await b.close(); if(fails.length) process.exit(1); })();
