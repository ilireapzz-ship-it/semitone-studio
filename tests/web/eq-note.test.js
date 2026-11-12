// 12 Nov: the selected EQ band shows the note it sits on beside its frequency (a resonance is usually a note of the song),
// and Listen (hear only this band) is on the same panel.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ S.demo=false; shellDock('mix'); eqOpen('chords','song'); const eq=eqOf('song','chords'); eq.on=true; eq.bands=[{id:'n1',t:'bell',f:440,g:-3,q:2,s:12,on:true,who:'You'}]; EQ.sel=new Set(['n1']); eqRender(); const P=$('#eqPanel')||document; return {notes:[440,261.63,1000,82.41].map(eqNote),txt:(document.querySelector('.eq-disp')||document.body).innerText.replace(/\s+/g,' '),listen:!!document.querySelector('[data-eqx="listen"]')}; });
 ok(/^A4,(C4),(B5),(E2)$/.test(r.notes.join())||r.notes.join()==='A4,C4,B5,E2','frequencies are named as notes ('+r.notes.join(', ')+')');
 ok(/440 Hz · A4/.test(r.txt),'the selected band shows “440 Hz · A4”');
 ok(r.listen,'and Listen (hear only this band) is on the same panel');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
