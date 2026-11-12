// 12 Nov, from the gap report: the chorus of the song written by hand was not louder than its verse and nothing said so.
// In the Mix lens Semitone measures a verse and a chorus on the same meter; when the chorus arrives less than 1 dB above the verse it
// says so in Session with the numbers and offers Lift it (verses 1.5 dB down, choruses 1.5 dB up on the master, one undo step).
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 // a song whose chorus plays exactly what its verse plays: no lift at all
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; const v=S.song.sections.findIndex(s=>s.type==='verse'); S.song.mats.chorus=JSON.parse(JSON.stringify(S.song.mats.verse)); S.beats.chorus=JSON.parse(JSON.stringify(S.beats.verse)); S.song.sections.forEach(s=>{ s.energy=.5; }); if(typeof invalidateViews==='function') invalidateViews(); S.piece=sectionView(S.sel); H.reset=true; histCheck(true); lensShow('mix'); });
 const r0=await p.evaluate(()=>liftRun());
 console.log(JSON.stringify(r0));
 ok(r0&&r0.d<1&&!r0.lifted,'a chorus that plays what its verse plays does not arrive louder ('+(r0?r0.v+' and '+r0.c:'')+' dB)');
 const card=await p.evaluate(()=>{ const c=liftSession()[0], m=mixCheck().find(x=>x.k==='lift'); return c?{t:c.t,text:c.text,act:c.act,shown:!!m&&m.fixLabel==='Lift it'&&m.msg===c.t}:null; });
 ok(card&&/no louder than|only/.test(card.t)&&card.act==='Lift it'&&/usually lands 1\.5 to 3 dB/.test(card.text),'Session says so, with the numbers and a Lift it button: “'+(card?card.t:'')+'”');
 ok(card&&card.shown,'and Mix check lists it too, with the same Lift it (Session is tucked away in the Mix lens)');
 const past0=await p.evaluate(()=>H.past.length);
 await p.evaluate(()=>liftApply()); await p.waitForTimeout(300);
 const r1=await p.evaluate(async()=>{ for(let k=0;k<60&&(LIFT.busy||LIFT.key!==liftKey());k++) await new Promise(f=>setTimeout(f,150)); const r=LIFT.res; return {r,past:H.past.length,label:H.past[H.past.length-1].label,lane:S.song.sections.map((_,si)=>!!((S.auto[si]||{}).master||{}).vol),card:liftSession().length}; });
 console.log(JSON.stringify(r1.r));
 ok(r1.past===past0+1&&/Lift the choruses/.test(r1.label),'Lift it is one undo step (“'+r1.label+'”)');
 ok(r1.r&&r1.r.d>=r0.d+1.8&&r1.r.lifted,'measured again, the chorus now arrives '+(r1.r?r1.r.d:'?')+' dB above the verse (it was '+r0.d+')');
 ok(r1.lane.some(Boolean),'drawn as an ordinary master volume lane');
 ok(r1.card===0&&await p.evaluate(()=>!mixCheck().some(x=>x.k==='lift')),'and the card is gone from Session and from Mix check');
 await p.keyboard.press('Control+z'); await p.waitForTimeout(400);
 const r2=await p.evaluate(async()=>{ for(let k=0;k<60&&LIFT.busy;k++) await new Promise(f=>setTimeout(f,150)); return liftRun(); }); ok(r2&&Math.abs(r2.d-r0.d)<.6&&!r2.lifted,'Undo takes the lift back ('+(r2?r2.d:'?')+' dB again)');
 // outside the Mix lens nothing is measured or said; a song with a real lift gets no card
 const quiet=await p.evaluate(()=>{ lensShow('write'); const a=liftSession().length; lensShow('mix'); return {a,big:liftCard({v:-18,c:-15.5,d:2.5,vi:0,ci:1}),lifted:liftCard({v:-18,c:-18,d:0,vi:0,ci:1,lifted:true})}; });
 ok(quiet.a===0&&quiet.big===null&&quiet.lifted===null,'no card outside Mix, none when the chorus already lifts, none once you have lifted');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
