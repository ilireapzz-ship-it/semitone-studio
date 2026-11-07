// A volume knob on every drum row (added samples too): drag up louder, down quieter, all the way down is off,
// double-click resets; it changes what the browser, the app engine and exports play; one undo per turn; saved.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; shellDock('beat'); S.drumRows=[...BASE_ROWS,'s_x']; (S.rowNames=S.rowNames||{}).s_x='Vox chop'; const t=S.beatSel||S.song.sections[S.sel].type; S.beatSel=t; S.beats[t].s_x=Array(stepsFor(S.meter)).fill(0); S.beats[t].s_x[0]=1; renderSeq(); histCheck(true); });
 await p.waitForTimeout(300);
 const k=await p.evaluate(()=>({rows:ROWS.length, knobs:[...document.querySelectorAll('#seq [data-rk]')].map(x=>x.dataset.rk), first:document.querySelector('#seq .seq-row[data-row="kick"] .rl').firstElementChild.matches('[data-rk]')}));
 ok(k.knobs.length===k.rows&&k.knobs.includes('s_x')&&k.first,'every row has a knob in front of its name, the added sample too');
 const box=await p.locator('#seq [data-rk="kick"]').boundingBox(); const before=await p.evaluate(()=>H.past.length);
 await p.mouse.move(box.x+11,box.y+11); await p.mouse.down(); await p.mouse.move(box.x+11,box.y+71,{steps:6}); await p.mouse.up(); await p.waitForTimeout(100);
 const d=await p.evaluate(b0=>{ const bar={si:S.sel,b:0,type:S.beatSel,last:false}; const was=S.fill; S.fill=false; const ev=drumPerf(bar).filter(e=>e.r==='kick'); S.fill=was; return {g:rowLvl('kick'), steps:H.past.length-b0, label:H.past[H.past.length-1]&&H.past[H.past.length-1].label, vel:ev.map(e=>e.vel), aria:document.querySelector('#seq [data-rk="kick"]').getAttribute('aria-valuetext'), saved:JSON.stringify(projectData()).includes('"rowLvl"')}; },before);
 ok(d.g<1&&d.g>0&&d.steps===1&&/Kick volume/.test(d.label),'drag down turns the kick down, in one undo step: '+d.aria+' '+JSON.stringify([d.steps,d.label]));
 ok(d.vel.length&&d.vel.every(v=>v<=1.2*d.g+1e-6),'the kick hits play quieter everywhere (one place feeds browser, app engine, MIDI and export)');
 ok(d.saved,'the level is saved with the song');
 await p.mouse.move(box.x+11,box.y+11); await p.mouse.down(); await p.mouse.move(box.x+11,box.y-200,{steps:6}); await p.mouse.up();
 const up=await p.evaluate(()=>({g:rowLvl('kick'), txt:rkTxt(rowLvl('kick'))}));
 ok(Math.abs(up.txt==='+6.0 dB'?0:1)===0,'drag up goes louder, to +6 dB at the top: '+up.txt);
 await p.dblclick('#seq [data-rk="kick"]'); ok(await p.evaluate(()=>rowLvl('kick')===1&&!('kick' in (S.rowLvl||{}))),'double-click puts it back to 0 dB');
 const off=await p.evaluate(()=>{ rkSet('s_x',0); const ev=drumPerf({si:S.sel,b:0,type:S.beatSel,last:false}).filter(e=>e.r==='s_x'); renderSeq(); return {n:ev.length, cls:document.querySelector('#seq [data-rk="s_x"]').classList.contains('off'), txt:rkTxt(0)}; });
 ok(off.n===0&&off.cls&&off.txt==='off','all the way down is off: the sample row plays nothing');
 const un=await p.evaluate(()=>{ histUndo(); return rowLvl('s_x'); }); ok(un===1,'undo brings it back');
 const kb=await p.evaluate(()=>{ const el=document.querySelector('#seq [data-rk="snare"]'); el.focus(); el.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowDown',bubbles:true})); return rowLvl('snare'); }); ok(kb<1,'arrow keys work on a focused knob');
 await p.screenshot({path:require('path').resolve(__dirname,'../../drum-knobs.png'),clip:{x:0,y:380,width:900,height:330}});
 ok(!errs.length,'no page errors '+errs.join(' | '));
 await b.close(); console.log(fails.length?`${fails.length} failed`:'all passed'); process.exit(fails.length?1:0); })();
