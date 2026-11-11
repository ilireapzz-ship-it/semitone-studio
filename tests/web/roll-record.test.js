// 11 Nov (Jamie: "why would it be like this instead of just recording using the piano roll? recording voice or real audio would require the screen we
// designed for vocal stacking and comping"): Record with the piano roll open records notes into the roll's part, drawing them live; Space keeps the
// take; anywhere else Record opens the designed Record tab (not the old full-screen page).
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); S.bpm=150; LIVE.countIn=false; selectSection(1); shellDock('roll'); trPick('bass',true); renderRoll(); });
 await p.waitForTimeout(200);
 await p.click('#recOpen'); await p.waitForTimeout(250);
 const st=await p.evaluate(()=>({rec:LIVE.rec,into:LIVE.recInto,overlay:!$('#recView').hidden,pill:(($('#rollRec')||{}).textContent||'')}));
 ok(st.rec&&st.into==='bass'&&!st.overlay&&/Recording Bass/.test(st.pill),'Record with the piano roll open records into its part (Bass), no separate page '+JSON.stringify(st));
 for(const k of ['a','g','h','f']){ await p.keyboard.down(k); await p.waitForTimeout(250); await p.keyboard.up(k); await p.waitForTimeout(60); }
 const live=await p.evaluate(()=>{ const cv=$('#rollRecCv'); if(!cv) return {cv:false}; const g=cv.getContext('2d'), d=g.getImageData(0,0,cv.width,cv.height).data; let n=0; for(let i=3;i<d.length;i+=16) if(d[i]>0) n++; return {cv:true,inked:n,notes:LIVE.recNotes.length,pill:$('#rollRec').textContent}; });
 ok(live.cv&&live.inked>20&&live.notes===4&&/4 notes/.test(live.pill),'the notes draw into the roll in red as you play, and the pill counts them '+JSON.stringify(live));
 await p.keyboard.press(' '); await p.waitForTimeout(400);
 const kept=await p.evaluate(()=>({rec:LIVE.rec,bass:((S.bassOwn||{})[secKey(1)]||[]).map(n=>n.m),pill:!!($('#rollRec')&&!$('#rollRec').hidden),label:H.past[H.past.length-1].label}));
 ok(!kept.rec&&kept.bass.join()==='36,43,45,41'&&!kept.pill&&/Played Bass into Verse 1/.test(kept.label),'Space keeps the take: the bass line is in Verse 1, one undo step '+JSON.stringify(kept));
 // the chords layer records chords
 await p.evaluate(()=>{ trPick(null); setLayer('chords'); renderRoll(); }); await p.click('#recOpen'); await p.waitForTimeout(250);
 const ci=await p.evaluate(()=>LIVE.recInto); for(const k of ['a','d','g']) await p.keyboard.down(k); await p.waitForTimeout(500); for(const k of ['a','d','g']) await p.keyboard.up(k); await p.keyboard.press(' '); await p.waitForTimeout(400);
 const ch=await p.evaluate(()=>matOf(S.song.sections[1]).chords[0]); ok(ci==='chords'&&ch&&(ch.pad||[]).length===3,'with Chords showing, Record plays chords into the bar '+JSON.stringify({ci,sym:ch&&ch.sym}));
 // a note pressed a moment before the count-in ends lands on the first beat (in the app the first note of a take was lost)
 await p.evaluate(()=>{ setLayer('melody'); trPick(null); LIVE.countIn=true; renderRoll(); }); await p.click('#recOpen'); const cin=await p.evaluate(()=>({count:LIVE.recCount,ms:Math.round(4*60/S.bpm*1000)})); await p.waitForTimeout(cin.ms-250); await p.keyboard.down('k'); await p.waitForTimeout(500); await p.keyboard.up('k'); await p.waitForTimeout(100); await p.keyboard.press(' '); await p.waitForTimeout(400); await p.evaluate(()=>{ LIVE.countIn=false; });
 const early=await p.evaluate(()=>matOf(S.song.sections[1]).melody.map(n=>[n.m,n.start]));
 ok(cin.count&&early.some(([m,st])=>m===72&&st===0),'a note played just before the count-in ends is kept, on beat 1 '+JSON.stringify({cin,early}));
 // with the drum step grid open, Record records drums there (in the app it jumped to the Record tab)
 await p.evaluate(()=>{ S.beats[S.song.sections[1].type]={}; selectSection(1); SHELL.part='drums'; shellDock('beat'); }); await p.waitForTimeout(200); await p.click('#recOpen'); await p.waitForTimeout(250);
 const dr0=await p.evaluate(()=>({rec:LIVE.rec,into:LIVE.recInto,beat:!document.querySelector('#ss_dock .dock-pane[data-pane="beat"]').hidden,pill:(($('#rollRec')||{}).textContent||'')}));
 for(const k of ['a','d','s','d']){ await p.keyboard.down(k); await p.waitForTimeout(120); await p.keyboard.up(k); await p.waitForTimeout(80); }
 const lit=await p.evaluate(()=>document.querySelectorAll('#ss_dock .step.rec-hit').length);
 await p.keyboard.press(' '); await p.waitForTimeout(400);
 const dr=await p.evaluate(()=>{ const b=S.beats[S.song.sections[1].type]||{}; return {kick:(b.kick||[]).filter(Boolean).length,snare:(b.snare||[]).filter(Boolean).length,hat:(b.hat||[]).filter(Boolean).length,lit:document.querySelectorAll('#ss_dock .step.on,#ss_dock .step.acc').length}; });
 ok(dr0.rec&&dr0.into==='drums'&&dr0.beat&&/Recording Drums/.test(dr0.pill)&&lit>=3&&dr.kick>=1&&dr.snare>=1&&dr.hat>=1&&dr.lit>=3,'with the step grid open, Record plays drums into the grid, lit as you hit '+JSON.stringify({dr0,lit,dr}));
 // elsewhere Record opens the designed Record tab
 await p.evaluate(()=>shellDock('mix')); await p.click('#recOpen'); await p.waitForTimeout(250);
 const tab=await p.evaluate(()=>({pane:!document.querySelector('#ss_dock .dock-pane[data-pane="record"]').hidden,overlay:!$('#recView').hidden,rec:LIVE.rec}));
 ok(tab.pane&&!tab.overlay&&!tab.rec,'from the mixer, Record opens the Record tab for voice and instruments '+JSON.stringify(tab));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
