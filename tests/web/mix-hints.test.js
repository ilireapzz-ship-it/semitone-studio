// 12 Nov, from the gap report: the song written by hand was mono (every part dead centre) and nothing said so; and the
// "Your DAW" card still listed a track that had been deleted. Mix check now offers Smart pan when everything sits in the middle,
// and the card lists only the tracks the song has.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; xAdd('pad'); xAdd('counter'); mixChans().forEach(c=>{ const st=strip('song',c); st.pan=0; delete st.width; }); H.reset=true; histCheck(true); lensShow('mix'); shellDock('mix'); }); await p.waitForTimeout(1700);
 const c0=await p.evaluate(()=>{ const x=mixCheck().find(q=>q.k==='centre'); return x?{msg:x.msg,fix:x.fixLabel,why:x.why}:null; });
 ok(c0&&c0.fix==='Smart pan'&&/dead centre/.test(c0.msg)&&/bass, the kick and the lead stay in the middle/.test(c0.why),'with every part in the middle, Mix check says so and offers Smart pan: “'+(c0?c0.msg:'')+'”');
 const pans=await p.evaluate(()=>{ const x=mixCheck().find(q=>q.k==='centre'); mixFix([x],'Smart pan'); return {pans:mixChans().map(c=>c+':'+(strip('song',c).pan||0)),label:H.past.length?H.past[H.past.length-1].label:'',still:!!mixCheck().find(q=>q.k==='centre'),bass:strip('song','bass').pan||0,lead:strip('song','lead').pan||0}; });
 ok(pans.pans.some(x=>Math.abs(+x.split(':')[1])>.1)&&pans.bass===0&&pans.lead===0,'Smart pan moves the parts that can move and leaves bass and lead in the middle ('+pans.pans.join(' ')+')');
 ok(pans.label==='Smart pan'&&!pans.still,'as one undo step, and the hint is gone');
 // with only melody, chords, bass and drums there is nothing Smart pan may move, so it is not offered
 const four=await p.evaluate(()=>{ xList().map(x=>x.id).forEach(id=>trackDelete(id)); mixChans().forEach(c=>{ strip('song',c).pan=0; }); return {chans:mixChans().join(),tip:!!mixCheck().find(q=>q.k==='centre')}; });
 ok(!four.tip,'with only the four basic parts ('+four.chans+') nothing may move, so no empty offer is made');
 // the Your DAW card
 const d=await p.evaluate(()=>{ const a=/Bass/.test(dawCardHTML()); trackDelete('bass'); const b2=/>Bass</.test(dawCardHTML()), has=/>Melody</.test(dawCardHTML()); histUndo(); const c=/>Bass</.test(dawCardHTML()); return {a,b2,has,c}; });
 ok(d.a&&!d.b2&&d.has,'after deleting the Bass track the Your DAW card no longer lists it (the other parts stay)');
 ok(d.c,'Undo brings it back onto the card');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
