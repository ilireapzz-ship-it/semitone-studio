// 12 Nov, from the gap report: a section's right-click menu had only Play from here, Split and Delete.
// It now also has Duplicate (a copy right after it, across every part) and Rename (typed in place), each one undo step.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; });
 const secs=()=>p.evaluate(()=>S.song.sections.map(s=>s.name+':'+s.type+':'+s.bars));
 const head=i=>p.evaluate(i=>{ const e=document.querySelector('#arr .timeline .sec[data-i="'+i+'"]')||document.querySelector('.timeline .sec[data-i="'+i+'"]'); const r=e.getBoundingClientRect(); return {x:r.left+Math.min(40,r.width/2),y:r.top+r.height/2}; },i);
 const menu=()=>p.evaluate(()=>[...document.querySelectorAll('#ctxMenu button, #ctxMenu [role=menuitem]')].map(x=>x.textContent.trim()));
 const s0=await secs(); let h=await head(1); await p.mouse.click(h.x,h.y,{button:'right'}); await p.waitForTimeout(300);
 const m=await menu(); console.log(JSON.stringify(m));
 ok(m.some(t=>/^Duplicate /.test(t))&&m.some(t=>/^Rename /.test(t))&&m.some(t=>/^Split /.test(t))&&m.some(t=>/^Delete /.test(t)),'the section menu offers Duplicate and Rename beside Split and Delete');
 const past0=await p.evaluate(()=>H.past.length);
 await p.click('#ctxMenu >> text=/^Duplicate /'); await p.waitForTimeout(500);
 const s1=await secs(), d1=await p.evaluate(()=>({past:H.past.length,label:H.past[H.past.length-1].label,sel:S.sel,same:JSON.stringify(matOf(S.song.sections[1]).melody)===JSON.stringify(matOf(S.song.sections[2]).melody)}));
 ok(s1.length===s0.length+1&&s1[2].split(':')[1]===s0[1].split(':')[1]&&s1[2].split(':')[2]===s0[1].split(':')[2],'Duplicate puts a copy right after the section ('+s1[1]+' then '+s1[2]+')');
 ok(s1[2].split(':')[0]!==s1[1].split(':')[0]&&/\(2\)$/.test(s1[2].split(':')[0]),'and the copy has a name of its own ('+s1[2].split(':')[0]+')');
 ok(d1.same&&d1.past===past0+1,'with the same notes, as one undo step (“'+d1.label+'”)');
 // rename the copy in place
 h=await head(2); await p.mouse.click(h.x,h.y,{button:'right'}); await p.waitForTimeout(300); await p.click('#ctxMenu >> text=/^Rename /'); await p.waitForTimeout(300);
 ok(await p.evaluate(()=>document.activeElement&&document.activeElement.id==='secRename'),'Rename opens a box on the section with its name selected');
 await p.keyboard.type('Verse two big'); await p.waitForTimeout(100);
 ok(await p.evaluate(()=>!S.playing),'typing a space in the box does not start the song');
 await p.keyboard.press('Enter'); await p.waitForTimeout(500);
 const r1=await p.evaluate(()=>({name:S.song.sections[2].name,label:H.past[H.past.length-1].label,saved:projectData().sections[2][2],box:!!$('#secRenameBox'),shown:(document.querySelector('.timeline .sec[data-i="2"]')||{}).textContent||''}));
 ok(r1.name==='Verse two big'&&r1.saved==='Verse two big'&&!r1.box&&/Verse two big/.test(r1.shown),'Enter keeps the new name on the timeline and in the saved song');
 ok(/^Rename /.test(r1.label),'as one undo step (“'+r1.label+'”)');
 // Escape cancels
 h=await head(2); await p.mouse.click(h.x,h.y,{button:'right'}); await p.waitForTimeout(300); await p.click('#ctxMenu >> text=/^Rename /'); await p.waitForTimeout(300); await p.keyboard.type('nope'); await p.keyboard.press('Escape'); await p.waitForTimeout(300);
 ok(await p.evaluate(()=>S.song.sections[2].name)==='Verse two big','Escape leaves the name alone');
 await p.keyboard.press('Control+z'); await p.waitForTimeout(400); await p.keyboard.press('Control+z'); await p.waitForTimeout(500);
 ok(JSON.stringify(await secs())===JSON.stringify(s0),'two Undos bring the song back exactly');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
