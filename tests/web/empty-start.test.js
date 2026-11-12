// 12 Nov, from the gap report: "Start blank" still gave ten sections, six transitions, and there was no way to add a section of a
// chosen kind. Start empty is one empty 8-bar section; a blank or empty song has no transitions sounding; a section's menu adds a
// Verse, Pre-chorus, Chorus or Bridge after it (a new kind starts empty), each as one undo step.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; });
 const key0=await p.evaluate(()=>[S.key,S.mode,S.bpm].join('|'));
 await p.click('#hdHome'); await p.waitForTimeout(300); await p.click('#hmNew'); await p.waitForTimeout(600);
 ok(await p.evaluate(()=>!!$('#stEmpty')&&/One empty 8-bar section/.test($('#stEmpty').title)),'the new-song page offers Start empty beside Start blank');
 const kt=await p.evaluate(()=>[S.key,S.mode,S.bpm].join('|')); const past0=await p.evaluate(()=>H.past.length);
 await p.click('#stEmpty'); await p.waitForTimeout(700);
 const e=await p.evaluate(()=>({closed:$('#startView').hidden,secs:S.song.sections.map(s=>s.name+':'+s.bars),notes:Object.values(S.song.mats).reduce((n,m)=>n+m.melody.length+m.chords.filter(c=>c&&!c.rest).length,0),beats:Object.values(S.beats).reduce((n,t)=>n+Object.values(t).reduce((q,r)=>q+(Array.isArray(r)?r.filter(Boolean).length:0),0),0),kt:[S.key,S.mode,S.bpm].join('|'),past:H.past.length,label:H.past[H.past.length-1].label,trans:S.song.sections.map((_,i)=>Object.keys(transOf(i)).length)}));
 console.log(JSON.stringify(e));
 ok(e.closed&&e.secs.length===1&&/:8$/.test(e.secs[0]),'Start empty gives one 8-bar section ('+e.secs.join(', ')+')');
 ok(e.notes===0&&e.beats===0,'with no notes, chords or drum hits');
 ok(e.kt===kt,'in the key and tempo the page showed');
 ok(e.past===past0+1&&e.label==='Start empty','as one undo step');
 // add a chorus after it from the section's menu
 const head=i=>p.evaluate(i=>{ const r=document.querySelector('.timeline .sec[data-i="'+i+'"]').getBoundingClientRect(); return {x:r.left+Math.min(40,r.width/2),y:r.top+r.height/2}; },i);
 let h=await head(0); await p.mouse.click(h.x,h.y,{button:'right'}); await p.waitForTimeout(300);
 const menu=await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu button, #ctxMenu [role=menuitem]')].map(x=>x.textContent.trim()));
 ok(['Verse','Pre-chorus','Chorus','Bridge'].every(k=>menu.includes('Add after: '+k)),'a section’s menu adds a Verse, Pre-chorus, Chorus or Bridge after it');
 await p.click('#ctxMenu >> text="Add after: Chorus"'); await p.waitForTimeout(600);
 const a=await p.evaluate(()=>({secs:S.song.sections.map(s=>s.type+':'+s.bars),sel:S.sel,empty:matOf(S.song.sections[1]).melody.length===0&&matOf(S.song.sections[1]).chords.every(c=>c.rest),label:H.past[H.past.length-1].label,trans:Object.keys(transOf(1)).length,bass:barList('song').reduce((n,b)=>n+bassNotes(b).length,0)}));
 ok(a.secs.join()==='verse:8,chorus:8'&&a.sel===1,'Add after: Chorus puts an 8-bar chorus after the verse and selects it ('+a.secs.join(', ')+')');
 ok(a.empty&&a.trans===0&&a.bass===0,'the new kind of section starts empty: no chords, no melody, no transition, no bass');
 ok(a.label==='Add Chorus','as one undo step (“'+a.label+'”)');
 // a kind the song already has plays that kind's notes, and says so
 h=await head(1); await p.mouse.click(h.x,h.y,{button:'right'}); await p.waitForTimeout(300); await p.click('#ctxMenu >> text="Add after: Verse"'); await p.waitForTimeout(600);
 const v=await p.evaluate(()=>({secs:S.song.sections.map(s=>s.name),link:!!document.querySelector('.timeline .sec[data-i="2"] .sec-link'),toast:$('#toast').textContent}));
 ok(v.secs.length===3&&v.link&&/plays the same notes/.test(v.toast),'adding a second verse shares the verse’s notes and says so (↔ on “'+v.secs[2]+'”)');
 await p.keyboard.press('Control+z'); await p.waitForTimeout(350); await p.keyboard.press('Control+z'); await p.waitForTimeout(350);
 ok(await p.evaluate(()=>S.song.sections.length)===1,'two Undos take both sections away again');
 // Start blank: no transitions sounding either
 const bl=await p.evaluate(()=>{ blankSong(); return {n:S.song.sections.length,trans:S.song.sections.map((_,i)=>Object.keys(transOf(i)).length).reduce((x,y)=>x+y,0)}; });
 ok(bl.trans===0,'a blank song has no risers or hits at its joins either');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
