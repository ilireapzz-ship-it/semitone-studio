// One header and transport: recipe sentence, lens switch, the time display, no second toolbar, and everything from the old bars still reachable.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ if(!c) fails.push(m); console.log((c?'ok  ':'FAIL')+' '+m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1800);
 const g=await p.evaluate(()=>({h:$('.topbar').offsetHeight,t:$('.transport').offsetHeight,over:$('.recipe').scrollWidth-$('.recipe').clientWidth,words:[...document.querySelectorAll('.rc-w')].map(x=>x.dataset.rc),extra:getComputedStyle(document.querySelector('.rc-extra')).display,bars:document.querySelectorAll('.song-top').length}));
 console.log(JSON.stringify(g)); ok(g.h===54&&g.t===48,'header 54px, transport 48px'); ok(g.over<=0,'the recipe fits at 1440'); ok(g.bars===1,'one transport bar'); ok(['style','producer','emotion','vibe','length','purpose'].every(k=>g.words.includes(k)),'recipe has style, producer, emotion, vibe, length, purpose');
 // recipe popovers change the song's recipe as one undo step
 await p.evaluate(()=>{ document.querySelector('[data-rc="more"]').click(); }); await p.waitForTimeout(80); await p.evaluate(()=>{ document.querySelector('#recipePop [data-rc="length"]').click(); }); await p.waitForTimeout(80);
 ok(await p.evaluate(()=>document.querySelectorAll('#recipePop [data-rset^="length"]').length)===5,'length offers five cards');
 const r=await p.evaluate(()=>{ const n=H.past.length; document.querySelector('#recipePop [data-rset="length:5min"]').click(); const after=H.past.length, w=recipeWords().find(x=>x.k==='length').t, saved=projectData().recipe.length; histUndo(); return {n,after,w,saved,back:recipeOf().length}; });
 console.log(JSON.stringify(r)); ok(r.after===r.n+1,'one undo step'); ok(r.saved==='5min','saved with the song'); ok(r.back==='','undo takes it back'); ok(/5 minutes/i.test(r.w),'the sentence says it');
 const q=await p.evaluate(()=>{ recipeSet('producer','antonoff','x'); const id=(TL.d.producers.find(x=>/antonoff/.test(x.id+x.tags[0]))||TL.d.producers[0]).id; recipeSet('producer',id,'x'); return {id,t:recipeWords().find(x=>x.k==='producer').t}; });
 ok(!!q.t,'producer shows in the sentence: '+q.t); await p.evaluate(()=>{ passOpen(); }); await p.waitForTimeout(200); ok(await p.evaluate(()=>PASS.q&&PASS.q.id)===q.id,'the pass opens on the recipe producer'); await p.evaluate(()=>{ passDiscard(); }); await p.waitForTimeout(150);
 // lenses
 await p.click('#lensProduce'); ok(await p.evaluate(()=>!$('#trLens').hidden&&!$('#view-compose').hidden),'Produce shows its transport tools');
 await p.click('#lensMix'); await p.waitForTimeout(150); ok(await p.evaluate(()=>location.hash==='#mix'&&$('#lensMix').getAttribute('aria-selected')==='true'),'Mix lens');
 await p.click('#lensWrite'); await p.waitForTimeout(150); ok(await p.evaluate(()=>!$('#view-compose').hidden&&$('#trLens').hidden),'Write lens');
 // time display
 const lcd=await p.evaluate(async()=>{ S.bpm=120; play('song'); await new Promise(r=>setTimeout(r,500)); const a=$('#songTime').textContent, bar=$('#trBar').textContent, lab=$('#songBtn').getAttribute('aria-label'); stopPlay(); return {a,bar,lab,lab2:$('#songBtn').getAttribute('aria-label')}; });
 console.log(JSON.stringify(lcd)); ok(/^\d\d:\d\d\.\d$/.test(lcd.a)&&/^\d+ \. \d+ \. \d+$/.test(lcd.bar),'time and bar.beat.tick'); ok(lcd.lab==='Stop'&&lcd.lab2==='Play the song','play button changes');
 // menu keeps the older tools
 await p.click('#hdLogo'); ok(await p.evaluate(()=>!$('#hdMenu').hidden),'logo opens the menu'); await p.click('#tab-orch'); await p.waitForTimeout(200); ok(await p.evaluate(()=>location.hash==='#orch'&&$('#hdMenu').hidden),'Orchestral is reachable from the menu');
 await p.evaluate(()=>{ location.hash='#compose'; }); await p.waitForTimeout(150); await p.keyboard.press('Meta+k'); await p.waitForTimeout(150); ok(await p.evaluate(()=>!$('#cmdModal').hidden),'⌘K opens'); await p.keyboard.press('Escape');
 const ph=await b.newPage({viewport:{width:390,height:800}}); ph.on('pageerror',e=>errs.push(e.message)); await ph.addInitScript(()=>{ window.__noStart=1; }); await ph.goto(url); await ph.waitForTimeout(1500); ok(await ph.evaluate(()=>document.documentElement.scrollWidth<=390),'no sideways scroll on a phone');
 console.log(fails.length?fails.length+' FAILED':'all passed'); console.log(errs.slice(0,6)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
