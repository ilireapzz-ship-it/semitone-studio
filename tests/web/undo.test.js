// Song history: every kind of change can be undone and redone, exactly, with a label.
const {chromium}=require('playwright');
const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1300,height:900}}); const errs=[]; const fails=[];
 p.on('pageerror',e=>errs.push('PAGEERR '+e.message+' '+(e.stack||'').split('\n')[1]));
 p.on('console',m=>{ if(m.type()==='error'&&!/ERR_TUNNEL|ERR_|Failed to load/.test(m.text())) errs.push('CON '+m.text()); });
 await p.goto(url); await p.waitForTimeout(2200);
 const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const state=()=>p.evaluate(()=>{ const s=histCapture(); return s.song+'|'+s.orch; });
 const steps=()=>p.evaluate(()=>[H.past.length,H.future.length]);
 const settle=()=>p.evaluate(()=>{ H.quietUntil=0; H.last=null; });
 const changes=[
  ['style','Changed style',()=>{ TL.pick=['genre:rnb']; TL.choices={}; $('#tagUse').click(); }],
  ['key','Changed key',()=>{ const e=$('#keySel'); e.value=String((S.key+3)%12); e.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['mode','Changed mode',()=>{ const e=$('#modeSel'); e.value=[...e.options].find(o=>o.value!==S.mode).value; e.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['time signature','Changed time signature',()=>{ const e=$('#meterSel'); e.value=[...e.options].find(o=>o.value!==S.meter).value; e.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['tempo','Changed tempo',()=>{ const e=$('#bpm'); e.value=String(S.bpm>100?90:110); e.dispatchEvent(new Event('input',{bubbles:true})); e.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['structure','Changed the structure',()=>{ const e=$('#structSel'); e.value=[...e.options].find(o=>o.value!==S.structure).value; e.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['new idea','',()=>{ $('#newBtn').click(); }],
  ['note','Edited the notes',()=>{ pushUndo(snapshotMel()); const m=matOfSel(); if(m.melody[0]) m.melody[0].m+=1; else m.melody.push({m:60,start:0,dur:1}); refreshAfterEdit(true); }],
  ['chords','Changed the chords',()=>{ const m=matOfSel(); setBlockChords(m.chords.map(c=>codeOf(c)).reverse(),'test'); }],
  ['lyrics','Changed the lyrics',()=>{ const e=$('#lyrIn'); e.value='a line for the test'; e.dispatchEvent(new Event('input',{bubbles:true})); e.blur(); }],
  ['sound','Changed the sounds',()=>{ S.parts.lead.vol=.33; S.parts.lead.sound='soft'; document.body.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['mix','Changed the mix',()=>{ S.mix.lead={...(S.mix.lead||{}),test:1}; document.body.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['automation','Changed the automation',()=>{ S.auto.lead={vol:[[0,1],[4,.5]]}; document.body.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['drums','Changed the drums',()=>{ const e=$('#kitSel'); e.value=[...e.options].find(o=>o.value!==S.kit).value; e.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['room','Changed the room reverb',()=>{ const e=document.querySelector('.room-sel'); e.value=S.room==='hall'?'studio':'hall'; e.dispatchEvent(new Event('change',{bubbles:true})); }],
  ['energy','Changed the energy',()=>{ S.song.sections[0].energy=.25; document.body.dispatchEvent(new Event('change',{bubbles:true})); }],
 ];
 for(const [name,label,fn] of changes){ const before=await state(); await settle(); const [n0]=await steps();
  await p.evaluate(fn); await p.waitForTimeout(500); const after=await state(); const [n1]=await steps(); const lab=await p.evaluate(()=>H.past.length?H.past[H.past.length-1].label:'');
  ok(after!==before&&n1===n0+1,`${name}: one step (“${lab}”)`); if(label) ok(lab.startsWith(label),`${name}: label starts “${label}”`);
  await p.evaluate(()=>histUndo()); await p.waitForTimeout(50); ok(await state()===before,`${name}: undo restores it exactly`);
  await p.evaluate(()=>histRedo()); await p.waitForTimeout(50); ok(await state()===after,`${name}: redo puts it back exactly`);
  await p.evaluate(()=>{ H.quietUntil=0; }); await p.waitForTimeout(1900); const [n2]=await steps(); ok(n2===n1,`${name}: no ghost step afterwards`); }
 const gh=await p.evaluate(async()=>{ await new Promise(f=>setTimeout(f,1700)); H.quietUntil=0; const n=H.past.length, r=H.future.length; for(const k of Object.keys(S.mix)) if(!S.mix[k].fx.some(Boolean)&&S.mix[k].vol===1&&!S.mix[k].pan) delete S.mix[k]; await renderSongBuffer(3); histCheck(); return [H.past.length-n,H.future.length-r]; }); ok(gh[0]===0&&gh[1]===0,'rendering or playing leaves no ghost step '+gh);
 // walking all the way back and forward again
 const back=await p.evaluate(()=>{ let n=0; while(H.past.length){ histUndo(); n++; } return n; }); const fwd=await p.evaluate(()=>{ let n=0; while(H.future.length){ histRedo(); n++; } return n; }); ok(back===fwd&&back===changes.length,`all the way back and forward: ${back} steps`);
 // a new change clears redo
 await p.evaluate(()=>histUndo()); await settle(); await p.evaluate(()=>{ S.parts.lead.vol=.2; }); await p.waitForTimeout(1800); ok((await steps())[1]===0,'a new change clears redo');
 // lyrics typed in one go are one step; the keyboard works, and leaves text boxes alone
 await settle(); const [l0]=await steps(); await p.click('#lyrIn'); await p.keyboard.type('hello there',{delay:30}); await p.waitForTimeout(600); await p.keyboard.type(' again',{delay:30}); await p.waitForTimeout(500); ok((await steps())[0]===l0+1,'typing lyrics is one step');
 const v0=await p.$eval('#lyrIn',e=>e.value); await p.keyboard.press('Meta+z'); ok((await p.$eval('#lyrIn',e=>e.value))!==v0&&(await steps())[1]===0,'⌘Z in a text box undoes typing only'); await p.keyboard.type('x'); await p.evaluate(()=>document.activeElement.blur()); await p.waitForTimeout(500);
 let s1=await p.evaluate(()=>H.past.length); await p.keyboard.press('Meta+z'); ok(await p.evaluate(()=>H.past.length)===s1-1,'⌘Z undoes the song'); await p.keyboard.press('Meta+Shift+z'); ok(await p.evaluate(()=>H.past.length)===s1,'⇧⌘Z redoes it');
 // note undo keeps a playing song playing
 await p.click('#songBtn'); await p.waitForTimeout(500); await p.evaluate(()=>{ pushUndo(); matOfSel().melody.push({m:70,start:.5,dur:.5}); refreshAfterEdit(true); }); await p.waitForTimeout(100); await p.evaluate(()=>histUndo()); ok(await p.evaluate(()=>S.playing),'undoing a note leaves the song playing'); await p.click('#songBtn');
 // orchestral: same history, and buttons there work
 await p.evaluate(()=>{ location.hash='#orch'; }); await p.waitForTimeout(300); await p.evaluate(()=>{ if(!O.sections.length) buildCue('heroic',{seed:3}); renderOrch(); }); await p.waitForTimeout(1800); await settle();
 const o0=await state(); await p.click('#oClrAll'); await p.waitForTimeout(500); const o1=await state(); ok(o1!==o0,'orchestral: clearing a section changes the cue'); ok((await p.evaluate(()=>H.past[H.past.length-1].label))==='Changed the orchestral parts','orchestral: label');
 await p.click('#oUndo'); await p.waitForTimeout(100); ok(await state()===o0,'orchestral: Undo button restores it'); await p.click('#oRedo'); await p.waitForTimeout(100); ok(await state()===o1,'orchestral: Redo button restores it');
 // history stops at 100 steps
 await p.evaluate(()=>{ for(let i=0;i<130;i++){ S.lift=i%2?1:0; S.seed+=1; H.quietUntil=0; histCheck(true); } }); ok(await p.evaluate(()=>H.past.length)===100,'history keeps the last 100 steps');
 // opening a song starts a fresh history, and saving isn't a step
 await p.evaluate(()=>{ applyProject(JSON.parse(JSON.stringify(projectData()))); }); await p.waitForTimeout(1800); ok((await steps()).join()==='0,0','opening a song starts a fresh history');
 ok(await p.evaluate(()=>$('#histUndo').disabled&&$('#undoBtn').disabled),'buttons grey out with nothing to undo');
 await p.evaluate(()=>{ S.project={...S.project,id:'x',name:'Renamed'}; O.name='Cue'; }); await p.waitForTimeout(1800); ok((await steps())[0]===0,'renaming or saving is not a step');
 // plugin version: restore from the DAW is not undoable, edits are, and the changed song goes back to the plugin
 const q=await b.newPage(); q.on('pageerror',e=>errs.push('PLUGIN '+e.message)); await q.addInitScript(()=>{ window.__nlog=[]; const L={}; window.__JUCE__={initialisationData:{},backend:{addEventListener:(n,f)=>{ (L[n]=L[n]||[]).push(f); return [n,f]; },removeEventListener(){},emitEvent:(n,d)=>window.__nlog.push([n,d])}}; window.__fire=(n,d)=>(L[n]||[]).forEach(f=>f(d)); });
 await q.goto(url); await q.waitForTimeout(1200); await q.evaluate(()=>window.__fire('st_init',{plugin:true,host:'FL Studio',recent:[]})); await q.waitForTimeout(2500); await q.evaluate(()=>{ H.quietUntil=0; });
 const r=await q.evaluate(async()=>{ const e=$('#bpm'), was=S.bpm; e.value=String(was>100?90:110); e.dispatchEvent(new Event('input',{bubbles:true})); e.dispatchEvent(new Event('change',{bubbles:true})); await new Promise(f=>setTimeout(f,500)); const n=H.past.length, changed=S.bpm; histUndo(); await new Promise(f=>setTimeout(f,1300)); const st=__nlog.filter(x=>x[0]==='st_state').pop(); return {n,changed,was,now:S.bpm,sent:st?JSON.parse((typeof st[1]==='string'?JSON.parse(st[1]):st[1]).json).song.bpm:null}; });
 ok(r.n===1&&r.now===r.was&&r.sent===r.was,`plugin: tempo undo works and reaches the plugin (${JSON.stringify(r)})`);
 console.log(fails.length?`${fails.length} FAILED`:'all passed'); console.log(errs.slice(0,8)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
