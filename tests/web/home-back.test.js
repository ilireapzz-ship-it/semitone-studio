// 12 Nov, found by writing a song by hand in the real app: Home's "Back to <song> →" rewrote the whole song (new sections, chords and
// melody, no message) whenever the song's length no longer matched the length in its recipe, because the button shared its id with
// "Start the song →" on the new-song page. Back to must only close Home. Start the song on a NEW song still writes for the length.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 // a song for "around 5 minutes" that you then cut down to two sections: its length is now far from the recipe's
 const pre=await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; S.recipe=S.recipe||{}; S.recipe.length=Object.keys(HEAD.shapes.lengths).find(k=>/5/.test(HEAD.shapes.lengths[k].label))||Object.keys(HEAD.shapes.lengths)[2]; lenApply(true); while(S.song.sections.length>2) secDelete(S.song.sections.length-1); histCheck(true); return {len:lenOf().label,off:lenOff(),n:S.song.sections.length}; });
 ok(pre.off&&pre.n===2,'the song is shorter than its recipe asks ('+pre.len+', '+pre.n+' sections)');
 const sig=()=>p.evaluate(()=>JSON.stringify([S.song.sections.map(x=>[x.name,x.bars,x.type]),Object.keys(S.song.mats).map(k=>[k,S.song.mats[k].melody.map(n=>[n.m,n.start,n.dur]),S.song.mats[k].chords.map(c=>c.sym)]),S.bpm,S.key]));
 const s0=await sig(), h0=await p.evaluate(()=>H.past.length);
 await p.click('#hdHome'); await p.waitForTimeout(400);
 const back=await p.evaluate(()=>({open:!$('#startView').hidden,txt:($('#stKeep')||{}).textContent}));
 ok(back.open&&/^Back to /.test(back.txt),'Home shows “'+back.txt+'”');
 await p.click('#stKeep'); await p.waitForTimeout(600);
 const s1=await sig(), after=await p.evaluate(()=>({closed:$('#startView').hidden,past:H.past.length,top:H.past.length?H.past[H.past.length-1].label:''}));
 ok(after.closed,'Back to closes Home');
 ok(s1===s0,'and the song is exactly as you left it: same sections, notes and chords');
 ok(after.past===h0&&!/Write it for/.test(after.top),'with no new undo step ('+after.top+')');
 // the new-song page still writes for the chosen length
 await p.click('#hdHome'); await p.waitForTimeout(300); await p.click('#hmNew'); await p.waitForTimeout(600);
 const nw=await p.evaluate(()=>({keep:($('#stKeep')||{}).textContent,home:!!($('#stKeep')||{dataset:{}}).dataset.home}));
 ok(/Start the song/.test(nw.keep)&&!nw.home,'New song shows “Start the song →”');
 await p.click('#stKeep'); await p.waitForTimeout(600);
 const fin=await p.evaluate(()=>({closed:$('#startView').hidden,off:lenOf()?lenOff():null,n:S.song.sections.length}));
 ok(fin.closed&&fin.off!==true,'and a new song is still written for its length ('+fin.n+' sections)');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
