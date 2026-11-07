// Start blank and Fill in: a blank song keeps key, tempo and sections with every part empty (no drum fills either);
// Fill in writes chords that fit your melody, a bassline (from chords, or from the melody with no chords), a melody
// and drums; Try another replaces the last try (one undo); "This section" leaves the others alone; New song offers both.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); p.on('console',m=>{ if(m.type()==='error'&&/NaN/.test(m.text())) errs.push(m.text()); }); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; startShow(); }); ok(await p.locator('#stBlank').isVisible(),'the start screen offers Start blank next to Start the song');
 const key0=await p.evaluate(()=>[S.key,S.bpm,S.song.sections.length]); await p.click('#stBlank'); await p.waitForTimeout(200);
 const a=await p.evaluate(()=>{ const L=barList('song'); return {closed:$('#startView').hidden, key:[S.key,S.bpm,S.song.sections.length], notes:L.reduce((s,x)=>s+x.notes.length,0), ch:L.filter(x=>!x.ch.rest).length, bass:L.reduce((s,x)=>s+bassNotes(x).length,0), drums:L.reduce((s,x)=>s+drumPerf(x).length,0), extra:xList().length, label:H.past[H.past.length-1].label}; });
 ok(a.closed&&JSON.stringify(a.key)===JSON.stringify(key0),'blank keeps the key, tempo and sections');
 ok(a.notes===0&&a.ch===0&&a.bass===0&&a.drums===0&&a.extra===0,'every part is empty, no drum fills: '+JSON.stringify(a));
 ok(a.label==='Start blank','one undo step');
 // write a melody in the selected section
 await p.evaluate(()=>{ const m=matOf(S.song.sections[S.sel]); histCheck(true); H.quietUntil=0; const k=S.key+60, sc=[0,2,3,5,7,8,10].map(x=>k+x); m.melody=[[0,0,2],[2,2,2],[4,4,3],[8,0,2],[10,3,2],[12,4,4],[16,2,2],[20,0,4],[24,4,2],[28,2,4]].map(([s,i,d])=>({start:s,m:sc[i],dur:d,vel:.9})); histCheck(true); invalidateViews(); });
 await p.click('#drFill'); ok(await p.locator('#recipePop .fill-row').count()===4,'Fill in in the top bar lists chords, bassline, melody and drums');
 const n0=await p.evaluate(()=>H.past.length); await p.click('#recipePop [data-fill="chords"]');
 const c=await p.evaluate(n0=>{ const m=matOf(S.song.sections[S.sel]), B=METERS[S.meter].B; let hit=0, tot=0; m.melody.forEach(n=>{ const ch=m.chords[Math.floor(n.start/B)%m.blockBars]; tot++; if(ch.pcs.includes(((n.m%12)+12)%12)) hit++; }); return {fit:hit/tot, steps:H.past.length-n0, mel:m.melody.length, other:S.song.sections.filter(x=>matKey(x)!==matKey(S.song.sections[S.sel])).every(x=>!matHasChords(matOf(x)))}; },n0);
 ok(c.fit>=.7,'the chords fit the melody: '+Math.round(c.fit*100)+'% of notes are chord tones'); ok(c.steps===1&&c.mel===10,'one undo, and the melody is left as written');
 ok(c.other,'"this section" leaves other sections alone');
 const again=await p.evaluate(()=>{ const n=H.past.length; document.querySelector('#recipePop [data-fill="chords"]').click(); return {n:H.past.length-n, again:!!document.querySelector('#recipePop [data-again]')}; });
 ok(again.n===0&&again.again,'Try another replaces the last try instead of piling up undo steps');
 await p.click('#recipePop [data-fill="bass"]'); await p.click('#recipePop [data-fill="drums"]');
 const d=await p.evaluate(()=>{ const L=barList('section'); return {bass:L.reduce((s,x)=>s+bassNotes(x).length,0), drums:L.reduce((s,x)=>s+drumPerf(x).length,0)}; }); ok(d.bass>0&&d.drums>0,'bassline and drums written: '+JSON.stringify(d));
 // another section with nothing in it: bass without chords, melody without chords
 const e=await p.evaluate(()=>{ const o=S.song.sections.findIndex(x=>matKey(x)!==matKey(S.song.sections[S.sel])); selectSection(o); FILL.scope='sec'; fillGo('bass'); const b=barList('section').reduce((s,x)=>s+bassNotes(x).length,0), noCh=!matHasChords(matOf(S.song.sections[o])); fillGo('melody'); return {b,noCh,mel:matOf(S.song.sections[o]).melody.length, still:!matHasChords(matOf(S.song.sections[o]))}; });
 ok(e.b>0&&e.noCh,'a bassline without chords, from the key or melody'); ok(e.mel>0&&e.still,'a melody without adding chords');
 ok(await p.evaluate(()=>{ FILL.scope='all'; fillGo('chords'); return Object.values(S.song.mats).every(matHasChords); }),'Whole song fills every section');
 ok(await p.evaluate(()=>{ try{ return JSON.stringify(nativeTimeline()).length>100; }catch(e){ return false; } }),'the app engine gets it');
 await p.evaluate(()=>{ $('#recipePop').hidden=true; $('#projNew').click(); }); ok(await p.evaluate(()=>!$('#startView').hidden&&!!$('#stBlank')),'New song opens the start screen with both choices');
 ok(!errs.length,'no page errors '+errs.join(' | '));
 await b.close(); console.log(fails.length?`${fails.length} failed`:'all passed'); process.exit(fails.length?1:0); })();
