// 12 Nov, from the gap report ("things change without being asked"): (1) in a blank song a bass line started playing by itself as soon
// as a chord existed; (2) Verse 2 silently shared Verse 1's notes. A blank song's bass now waits to be asked, and the bar above the
// piano roll names the sections that share their notes, with one click to give a section its own.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); const si=S.song.sections.findIndex(s=>s.type==='verse'); selectSection(si); shellDock('roll'); setLayer('chords'); }); await p.waitForTimeout(300);
 // write a chord by hand (double-click three notes into bar 1)
 const at=(m,beat)=>p.evaluate(([m,beat])=>{ const L=S.rl, r=$('#roll').getBoundingClientRect(); return {x:r.left+beat*(L.W/L.TB)+5,y:r.top+L.top+(L.hi-m)*L.rh+L.rh/2}; },[m,beat]);
 for(const m of [55,58,62]){ const q=await at(m,.5); await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(300); }
 const st=await p.evaluate(()=>{ const bars=barList('section'); return {chord:matOfSel().chords[0].sym,bass:bars.reduce((n,b)=>n+bassNotes(b).length,0)}; });
 ok(/^G/.test(st.chord),'a chord is written ('+st.chord+')');
 ok(st.bass===0,'and the bass stays silent: a blank song plays only what you wrote ('+st.bass+' bass notes)');
 // Bass follows the chords brings it in when you ask
 const asked=await p.evaluate(()=>{ bassFollow(); const bars=barList('section'); return bars.reduce((n,b)=>n+bassNotes(b).length,0); }).catch(e=>'err '+e.message);
 ok(asked>0,'Bass follows the chords brings a bass line in when you ask ('+asked+' notes)');
 // sections that share their notes say so
 const lk=await p.evaluate(()=>{ setLayer('melody'); drawerTitle(); const b=$('#drLink'); return b?{t:b.textContent,title:b.title,linked:secLinked().map(i=>S.song.sections[i].name)}:null; });
 ok(lk&&lk.linked.length>0&&/↔/.test(lk.t)&&/play the same melody and chords/.test(lk.title),'the bar says which sections share these notes: '+(lk?lk.t+' ('+lk.linked.join(', ')+')':'nothing'));
 const sig=i=>p.evaluate(i=>JSON.stringify(matOf(S.song.sections[i]).chords.map(c=>c.sym)),i);
 const other=await p.evaluate(()=>secLinked()[0]), cur=await p.evaluate(()=>S.sel), before=await sig(other), h0=await p.evaluate(()=>H.past.length);
 await p.click('#drLink'); await p.waitForTimeout(400);
 const after=await p.evaluate(()=>({linked:secLinked().length,past:H.past.length,label:H.past[H.past.length-1].label}));
 ok(after.past===h0+1&&/gets its own notes/.test(after.label)&&after.linked===0,'one click gives this section its own notes, as one undo step (“'+after.label+'”)');
 // now a change here leaves the other alone
 await p.evaluate(()=>{ setLayer('chords'); }); await p.waitForTimeout(200); const q=await at(67,4.5); await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(300);
 ok(await sig(other)===before&&await sig(cur)!==before,'and a chord added here no longer changes '+(lk?lk.linked[0]:'the other section'));
 await p.keyboard.press('Control+z'); await p.waitForTimeout(250); await p.keyboard.press('Control+z'); await p.waitForTimeout(350);
 ok(await p.evaluate(()=>secLinked().length>0),'Undo links them again');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
