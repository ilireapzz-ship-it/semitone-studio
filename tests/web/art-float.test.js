// 11 Nov (Jamie: "a window pops up whenever user uses the selector on piano roll"): with Articulation mode on, selecting a note shows the
// articulation strip. In the app it stretched into a dark sheet over half the roll (an older note-bar rule pinned its top). It stays a small
// strip at the bottom of the roll, and its × turns Articulation mode off so selecting stops showing it.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); localStorage.setItem('ss_artmode','1'); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; document.body.classList.add('is-native'); /* the app's look */ selectSection(1); shellDock('roll'); trPick(null); setLayer('melody'); renderRoll(); }); await p.waitForTimeout(300);
 const r=await (await p.$('#roll')).boundingBox();
 await p.evaluate(()=>{ S.selNote=matOfSel().melody[0]; renderRoll(); }); await p.waitForTimeout(400); /* a note selected, as a click or a drag over it does */
 const f=await p.evaluate(()=>{ const nb=$('#cNoteBar'), q=nb.getBoundingClientRect(), roll=$('#roll').getBoundingClientRect(); return {art:S.artMode,shown:!nb.hidden&&q.height>0,h:Math.round(q.height),rollH:Math.round(roll.height),bottomGap:Math.round(roll.bottom-q.bottom)}; });
 ok(f.art&&f.shown&&f.h<130&&f.h<f.rollH/3,'in the app the articulation strip is small, not a sheet over the roll '+JSON.stringify(f));
 await p.click('#cNoteBar [data-artoff]'); await p.waitForTimeout(300);
 const off=await p.evaluate(()=>({art:S.artMode,hidden:$('#cNoteBar').hidden,stored:localStorage.getItem('ss_artmode')}));
 ok(!off.art&&off.hidden&&!off.stored,'its × turns Articulation mode off '+JSON.stringify(off));
 await p.mouse.move(r.x+r.width*.04,r.y+r.height*.1); await p.mouse.down(); await p.mouse.move(r.x+r.width*.45,r.y+r.height*.9,{steps:8}); await p.mouse.up(); await p.waitForTimeout(400);
 const again=await p.evaluate(()=>({sel:songSel().length,hidden:$('#cNoteBar').hidden}));
 ok(again.sel>0&&again.hidden,'then selecting notes shows nothing '+JSON.stringify(again));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
