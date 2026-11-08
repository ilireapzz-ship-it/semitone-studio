// Home button, right-click a track (Fill in, change the instrument), Space plays the whole song, and the editor's tabs by job.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; });
 // Home: the start page for the song you're in, without touching it
 const sig=()=>p.evaluate(()=>JSON.stringify([S.song.sections.map(x=>[x.name,x.bars]),S.bpm,S.key,S.style.id,matOf(S.song.sections[0]).melody.slice(0,6).map(n=>n.m)]));
 const s0=await sig(); await p.click('#hdHome'); await p.waitForTimeout(300);
 const h=await p.evaluate(()=>({open:!$('#startView').hidden,hero:!!document.querySelector('.st-home'),back:($('#stKeep')||{}).textContent,save:!!$('#hmSave'),nw:!!$('#hmNew'),saved:[...document.querySelectorAll('#startView .label')].some(x=>/Saved songs/.test(x.textContent))}));
 ok(h.open&&h.hero&&/Back to/.test(h.back)&&h.save&&h.nw&&h.saved,'Home opens the start page with Back to your song, Save, New song and your saved songs '+JSON.stringify(h));
 ok(await sig()===s0,'opening Home changes nothing in the song');
 await p.keyboard.press('Escape'); await p.waitForTimeout(100);
 ok(await p.evaluate(()=>$('#startView').hidden),'Esc on Home goes back to the song');
 await p.click('#hdHome'); await p.waitForTimeout(200); await p.click('#hmNew'); await p.waitForTimeout(300);
 const nw=await p.evaluate(()=>({start:!$('#startView').hidden,home:!!document.querySelector('.st-home'),label:H.past[H.past.length-1].label}));
 ok(nw.start&&!nw.home&&nw.label==='New song','New song is one undo step and shows the fresh start page '+JSON.stringify(nw));
 await p.evaluate(()=>{ startClose(); histUndo(); }); await p.waitForTimeout(200);
 ok(await sig()===s0,'Undo brings the old song back');
 // a saved song shows on Home and opens from there
 const sv=await p.evaluate(async()=>{ $('#projName').value='Home test'; await saveProject(true); const id=S.project.id; homeShow(); await new Promise(r=>setTimeout(r,300)); const row=document.querySelector(`[data-stproj="${id}"]`); return {row:!!row,cur:row&&row.getAttribute('aria-current')}; });
 ok(sv.row&&sv.cur==='true','a saved song is listed on Home, marked as the open one '+JSON.stringify(sv));
 await p.evaluate(()=>startClose());
 // right-click a track: Fill in for this section or the whole song, and change the instrument
 const at=await p.evaluate(()=>{ const r=document.querySelector('.arr-row.track[data-track="bass"] .arr-name').getBoundingClientRect(); return {x:r.left+20,y:r.top+r.height/2}; });
 await p.mouse.click(at.x,at.y,{button:'right'}); await p.waitForTimeout(150);
 const it=await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].map(x=>x.textContent));
 ok(/Fill in Bass for /.test(it[0])&&/Fill in Bass for the whole song/.test(it[1])&&it.some(x=>/Change the instrument/.test(x)),'right-clicking the Bass track offers Fill in and Change the instrument '+JSON.stringify(it));
 const fl=await p.evaluate(async()=>{ const n=H.past.length; [...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>/for the whole song/.test(x.textContent)).click(); await new Promise(r=>setTimeout(r,150)); return {steps:H.past.length-n,label:H.past[H.past.length-1].label}; });
 ok(fl.steps===1&&/Fill in bassline for the whole song/.test(fl.label),'Fill in from the menu is one undo step '+JSON.stringify(fl));
 await p.mouse.click(at.x,at.y,{button:'right'}); await p.waitForTimeout(150);
 { const c=await p.evaluate(()=>{ const r=[...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>/Change the instrument/.test(x.textContent)).getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2}; }); await p.mouse.click(c.x,c.y); await p.waitForTimeout(150); }
 ok(await p.evaluate(()=>!$('#recipePop').hidden&&!!$('#spList')&&SPICK.p==='bass'),'Change the instrument opens the sound picker for that track');
 await p.evaluate(()=>{ $('#recipePop').hidden=true; });
 // Space plays the whole song from the playhead; Shift+Space plays just the section
 await p.evaluate(()=>{ selectSection(1); document.activeElement.blur(); });
 await p.keyboard.press('Space'); await p.waitForTimeout(200);
 const sp=await p.evaluate(()=>{ const r={mode:S.playMode,bars:S.PT?[...Array(400).keys()].map(k=>S.PT.barAt(k)).filter(Boolean).length:0}; stopPlay(); return r; });
 ok(sp.mode==='song','Space plays the song, not just the section '+JSON.stringify(sp));
 await p.keyboard.press('Shift+Space'); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>{ const m=S.playMode; stopPlay(); return m==='section'; }),'Shift+Space plays just the selected section');
 // the editor's tabs by job; Sound and Song are different pages
 const tb=await p.evaluate(()=>({tabs:[...document.querySelectorAll('.dock-tabs [data-drawer]')].map(x=>x.textContent),song:DRAWER.song.p.map(x=>x[1]),notes:DRAWER.notes.p.map(x=>x[1]),mix:DRAWER.mix.p.map(x=>x[1])}));
 ok(tb.tabs.join()==='Song,Notes,Sound,Mix,Ideas'&&tb.song.join()==='Settings,Emotion'&&tb.notes.includes('Lyrics')&&tb.mix.includes('Automation'),'tabs: Song, Notes, Sound, Mix, Ideas '+JSON.stringify(tb));
 await p.evaluate(()=>shellDock('style')); await p.click('[data-drawer="sound"]'); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>!document.querySelector('.dock-pane[data-pane="sound"]').hidden&&document.querySelector('.dock-pane[data-pane="style"]').hidden),'clicking Sound while in Song settings opens the instruments, not the same page');
 ok(!errs.length,'no page errors '+errs.join(' | '));
 console.log(fails.length?fails.length+' failed':'all passed'); await b.close(); process.exit(fails.length?1:0); })();
