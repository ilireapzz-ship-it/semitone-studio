// Home is a hub: one card per area (Song, Orchestral, Mix, Words, Record, the five Classroom rooms, Pitch pack, Review, Degree, a game, Export); each opens its area in one click, Back to your song and the saved songs stay, and it fits every window size and theme.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; });
 const sig=()=>p.evaluate(()=>JSON.stringify([S.song.sections.map(x=>[x.name,x.bars]),S.bpm,S.key,S.style.id]));
 const s0=await sig();
 // a plain web page can't save files: no Export card there
 ok(await p.evaluate(()=>{ homeShow(); const n=document.querySelectorAll('#startView [data-area]').length, ex=!!document.querySelector('#startView [data-area="export"]'); startClose(); return !downloadsCap&&n===14&&!ex; }),'a plain web page (no file saving) shows every card but Export');
 await p.evaluate(()=>{ downloadsCap={save:()=>{}}; }); /* as in the app */
 await p.click('#hdHome'); await p.waitForTimeout(300);
 const h=await p.evaluate(()=>{ const v=$('#startView'), kids=[...v.children].map(x=>x.className.split(' ')[0]); return {open:!v.hidden,back:($('#stKeep')||{}).textContent,nw:!!$('#hmNew'),save:!!$('#hmSave'),saved:[...v.querySelectorAll('.label')].some(x=>/Saved songs/.test(x.textContent)),order:kids,cards:[...v.querySelectorAll('[data-area]')].map(x=>x.dataset.area),groups:[...v.querySelectorAll('.st-hub-g>.label')].map(x=>x.textContent),here:(v.querySelector('[data-area][aria-current="true"]')||{}).dataset}; });
 ok(h.open&&/^Back to /.test(h.back)&&h.nw&&h.save&&h.saved,'Home still has Back to your song, Save, New song and your saved songs '+JSON.stringify({back:h.back,saved:h.saved}));
 ok(h.order.indexOf('st-hub')===h.order.indexOf('st-hero')+1&&h.order.indexOf('st-cols')>h.order.indexOf('st-hub'),'the area cards sit under Back to your song, above the songs '+JSON.stringify(h.order));
 const AREAS=['song','orch','mix','words','record','learn','time','read','play','ear','pitch','review','degree','game','export'];
 ok(h.cards.join()===AREAS.join()&&h.groups.join()==='Make,Classroom,Check and share','one card per area in three groups '+JSON.stringify(h.groups));
 ok(h.here&&h.here.area==='song','the card for where you are is marked "here"');
 ok(await p.evaluate(()=>[...document.querySelectorAll('#startView [data-area]')].every(x=>x.querySelector('b').textContent&&x.querySelector('small').textContent.length>12)),'every card has a name and a one-line description');
 const views=['compose','learn','time','read','play','orch','ear'];
 const shown=()=>p.evaluate(v=>v.filter(k=>!$('#view-'+k).hidden),views);
 const pane=id=>p.evaluate(id=>{ const x=document.querySelector(`.dock-pane[data-pane="${id}"]`); return !!x&&!x.hidden&&x.offsetParent!==null; },id);
 const open=async()=>{ await p.evaluate(()=>{ ['pitchView','reviewView','degreeView','recipePop','dxModal'].forEach(i=>{ const x=$('#'+i); if(x) x.hidden=true; }); homeShow(); }); await p.waitForTimeout(150); };
 const go=async a=>{ await open(); await p.click(`#startView [data-area="${a}"]`); await p.waitForTimeout(350); return p.evaluate(()=>$('#startView').hidden); };
 const CHECK={
  song:async()=>{ const v=await shown(); return v.join()==='compose'&&await p.evaluate(()=>HEAD.lens!=='mix'); },
  orch:async()=>(await shown()).join()==='orch'&&await p.evaluate(()=>location.hash==='#orch'),
  mix:async()=>(await shown()).join()==='compose'&&await p.evaluate(()=>HEAD.lens==='mix'),
  words:async()=>(await shown()).join()==='compose'&&await pane('lyrics'),
  record:async()=>(await shown()).join()==='compose'&&await pane('record'),
  pitch:()=>p.evaluate(()=>!$('#pitchView').hidden),
  review:()=>p.evaluate(()=>!$('#reviewView').hidden),
  degree:()=>p.evaluate(()=>!$('#degreeView').hidden),
  game:()=>p.evaluate(()=>!$('#recipePop').hidden&&!!document.querySelector('#recipePop [data-game]')),
  export:()=>p.evaluate(()=>!$('#dxModal').hidden) };
 for(const a of ['learn','time','read','play','ear']) CHECK[a]=async()=>{ const v=await shown(); return v.join()===a&&await p.evaluate(()=>!$('#classNav').hidden); };
 // from a lesson back to Song and Mix too, so each card works whichever area you came from
 for(const a of [...AREAS,'song','orch','mix','ear','song']){ const closed=await go(a); const r=await CHECK[a](); ok(closed&&r,`"${a}" card closes Home and opens its area (${(await shown()).join()}${a==='mix'||a==='song'?' lens '+await p.evaluate(()=>HEAD.lens):''})`); }
 await p.evaluate(()=>{ ['pitchView','reviewView','degreeView','recipePop','dxModal'].forEach(i=>{ const x=$('#'+i); if(x) x.hidden=true; }); location.hash='#orch'; }); await p.waitForTimeout(200);
 await p.evaluate(()=>homeShow()); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>(document.querySelector('#startView [data-area][aria-current="true"]')||{}).dataset.area==='orch'),'opened from Orchestral, the Orchestral card is marked "here"');
 await p.keyboard.press('Escape'); await p.waitForTimeout(100);
 await p.evaluate(()=>{ location.hash='#compose'; lensShow('write'); }); await p.waitForTimeout(200);
 ok(await sig()===s0,'opening areas from Home changes nothing in the song');
 // the new-song start page gets the same cards; Start the song still works
 await p.evaluate(()=>{ homeShow(); }); await p.waitForTimeout(100); await p.click('#hmNew'); await p.waitForTimeout(300);
 ok(await p.evaluate(()=>!$('#startView').hidden&&!document.querySelector('.st-home')&&document.querySelectorAll('#startView [data-area]').length===15&&!document.querySelector('#startView [data-area][aria-current]')),'the new-song start page has the cards too, with none marked');
 await p.evaluate(()=>{ startClose(); histUndo(); }); await p.waitForTimeout(200);
 // window sizes from space.test.js, a phone, and every theme: nothing wider than the window, cards readable
 const fit=async(w,hh)=>{ await p.setViewportSize({width:w,height:hh}); await p.evaluate(()=>homeShow()); await p.waitForTimeout(150); return p.evaluate(w=>{ const hub=$('#stHub'), r=hub.getBoundingClientRect(), cards=[...hub.querySelectorAll('[data-area]')].map(x=>x.getBoundingClientRect()); return {hubR:Math.round(r.right),cardsIn:cards.every(c=>c.left>=0&&c.right<=w+.5&&c.width>=150),scroll:hub.scrollWidth<=hub.clientWidth+1,minW:Math.round(Math.min(...cards.map(c=>c.width)))}; },w); };
 for(const [w,hh] of [[1440,900],[1280,800],[1024,680],[900,600],[390,800]]){ const f=await fit(w,hh); ok(f.hubR<=w&&f.cardsIn&&f.scroll,`${w}×${hh}: every card fits the window ${JSON.stringify(f)}`); }
 await p.setViewportSize({width:1440,height:900});
 const themes=await p.evaluate(()=>[...new Set(($('#stThemes').textContent.match(/data-theme="([^"]+)"/g)||[]).map(x=>x.slice(12,-1)))]);
 for(const t of themes){ const c=await p.evaluate(t=>{ themeUse(t,false); homeShow(); const x=document.querySelector('#startView [data-area="orch"]'), ic=getComputedStyle(x.querySelector('.ar-ic')).color, bg=getComputedStyle(x).backgroundColor, fg=getComputedStyle(x.querySelector('b')).color; return {ic,bg,fg}; },t).catch(e=>({err:e.message}));
  ok(c.ic&&c.ic!=='rgba(0, 0, 0, 0)'&&c.fg!==c.bg&&!c.err,`theme ${t}: cards take its colours ${JSON.stringify(c)}`); }
 await p.evaluate(()=>startClose());
 ok(!errs.length,'no page errors '+errs.join(' | '));
 console.log(fails.length?fails.length+' failed':'all passed'); await b.close(); process.exit(fails.length?1:0); })();
