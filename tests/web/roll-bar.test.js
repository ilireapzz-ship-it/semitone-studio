// 12 Nov (Jamie: "the bars are different above the melody piano roll vs counter melody and harmony... no universality... i like the second bar
// with another take, oct down, oct up, plays in... but all on the row above it. change another take to an icon of dice... the arrow up and arrow
// down icons... replace oct down and up... keep the plays in"). Every part gets the same controls in the roll's top row.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const xid=await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; xAdd('counter'); selectSection(1); return xList()[0].id; });
 const bar=()=>p.evaluate(()=>{ const b=$('#partBar'); return {inTop:!!b&&!!b.closest('#drTools'),shown:!!b&&!b.hidden&&b.getClientRects().length>0,dice:!!b.querySelector('[data-rb="take"] svg'),plays:b.querySelectorAll('[data-rbsec],[data-xsec]').length,octaveWords:/Octave (up|down)/.test(b.textContent),arrows:!!document.querySelector('#drTools [data-sc="rt:up"]')&&!!document.querySelector('#drTools [data-sc="rt:down"]')}; });
 for(const id of ['lead','chords','bass',xid]){ await p.evaluate(id=>trSelectPart(id),id); await p.waitForTimeout(300); const s=await bar(); const n=await p.evaluate(()=>S.song.sections.length);
  ok(s.inTop&&s.shown&&s.dice&&s.plays===n&&!s.octaveWords&&s.arrows,`${id}: the same controls in the top row (dice, Plays in, ↑ ↓) `+JSON.stringify(s)); }
 // Plays in on the melody leaves it out of a section, one undo step
 await p.evaluate(()=>trSelectPart('lead')); await p.waitForTimeout(200);
 await p.click('#partBar [data-rbsec="2"]'); await p.waitForTimeout(300);
 const m=await p.evaluate(()=>({off:!!(S.secMute&&S.secMute.lead&&S.secMute.lead[2]),pressed:document.querySelector('#partBar [data-rbsec="2"]').getAttribute('aria-pressed')}));
 ok(m.off&&m.pressed==='false','Plays in: the melody can be left out of a section '+JSON.stringify(m));
 await p.click('#partBar [data-rbsec="2"]'); await p.waitForTimeout(300); ok(await p.evaluate(()=>!(S.secMute&&S.secMute.lead&&S.secMute.lead[2])),'and brought back');
 // the dice writes another take
 await p.click('#partBar [data-rb="take"]'); await p.waitForTimeout(400); const t=await p.evaluate(()=>({p:WT.p,bar:!!$('#wtBar')&&!$('#wtBar').hidden}));
 ok(t.p==='lead'&&t.bar,'the dice writes another melody take, with the takes bar to go back and forth '+JSON.stringify(t));
 // an added part: the dice and Plays in still work
 await p.evaluate(id=>{ wtClose(); trSelectPart(id); },xid); await p.waitForTimeout(200); const take0=await p.evaluate(id=>xList().find(x=>x.id===id).take,xid);
 await p.click('#partBar [data-rb="take"]'); await p.waitForTimeout(300); const take1=await p.evaluate(id=>xList().find(x=>x.id===id).take,xid);
 ok(take1!==take0,'the dice on an added part writes another take '+JSON.stringify({take0,take1}));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
