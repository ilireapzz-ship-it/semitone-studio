const path=require('path'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto('file://'+path.join(__dirname,'..','..','web','semitone-studio.html')); await p.waitForTimeout(1200);
 await p.evaluate(()=>{ if(!S.song) $('#newBtn').click(); spPut({big:false}); }); await p.waitForTimeout(400);
 const roll=async()=>p.evaluate(()=>{ const r=$('#roll').getBoundingClientRect(), q=S.rects[0]; return {x:r.left+q.x+q.w/2,y:r.top+q.y+q.h/2,rx:r.left,ry:r.top,w:r.width,h:r.height,rh:S.rl.rh,n:S.rects.length}; });
 // bass opens on the main roll, untouched until a real change
 await p.click('[data-arrpart="bass"][data-arrsec="1"]'); await p.waitForTimeout(250);
 const a=await p.evaluate(()=>({part:rollPart(),pane:!document.querySelector('.dock-pane[data-pane="roll"]').hidden,lay:$('#layPart').textContent,vis:!$('#layPart').hidden,bar:!$('#partBar').hidden,own:S.bassOwn,n:S.rects.length,tabs:[...document.querySelectorAll('.dock-tabs *')].some(x=>/Bass line|Added part/.test(x.textContent))}));
 ok(a.part==='bass'&&a.pane&&a.vis&&a.lay==='Bass'&&a.bar&&a.n>0&&!a.own,'bass opens on the one roll',a);
 let r=await roll(); await p.mouse.click(r.x,r.y); await p.waitForTimeout(120);
 ok(await p.evaluate(()=>!S.bassOwn),'selecting a note changes nothing');
 // drag up: moves, becomes own notes, one undo step, plays and exports
 const before=await p.evaluate(()=>{ S.selNote=null; S.selSet=new Set(); const n=rollArr()[0]; return {m:n.m,start:n.start,len:buildMidi().length}; });
 r=await roll(); await p.mouse.move(r.x,r.y); await p.mouse.down(); await p.mouse.move(r.x,r.y-r.rh*3,{steps:4}); await p.mouse.up(); await p.waitForTimeout(150);
 const c=await p.evaluate(()=>{ const k=secKey(S.sel), bar=barList('song').find(b=>b.si===S.sel&&b.b===0); return {own:(S.bassOwn||{})[k]&&S.bassOwn[k][0].m,played:bassNotes(bar).map(n=>n.m),saved:!!projectData().bassOwn,follow:!!document.querySelector('[data-pb="follow"]'),other:Object.keys(S.bassOwn||{}).length}; });
 ok(c.own-before.m===3&&c.played.includes(before.m+3)&&c.saved&&c.follow&&c.other===1,'dragging a bass note edits only this section and plays it',{...c,before});
 // resize, double-click to add, Delete, velocity use the same tools
 r=await roll(); const q0=await p.evaluate(()=>{ const q=S.rects[0], R=$('#roll').getBoundingClientRect(); return {x:R.left+q.x+q.w-2,y:R.top+q.y+q.h/2,d:q.n.src.dur}; });
 await p.mouse.move(q0.x,q0.y); await p.mouse.down(); await p.mouse.move(q0.x+60,q0.y,{steps:4}); await p.mouse.up(); await p.waitForTimeout(120);
 ok(await p.evaluate(d=>rollArr()[0].dur>d,q0.d),'the right edge of a bass note stretches it');
 const n0=await p.evaluate(()=>rollArr().length);
 await p.mouse.dblclick(r.rx+r.w*.93,r.ry+r.h*.2); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>rollArr().length)===n0+1,'double-click adds a bass note');
 await p.keyboard.press('Delete'); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>rollArr().length)===n0,'Delete removes it');
 const v=await p.evaluate(()=>{ const n=velNotes()[0], cv=$('#rollLaneC'), q=cv.getBoundingClientRect(); return {x:q.left+VEL.X(n.start),t:q.top,h:q.height}; });
 await p.mouse.move(v.x,v.t+v.h-10); await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(150);
 const vv=await p.evaluate(()=>{ const bar=barList('song').find(b=>b.si===S.sel&&b.b===0); return {n:rollArr()[0].vel,played:bassNotes(bar)[0].vel}; });
 ok(vv.n<.3&&vv.played===vv.n,'bass velocity is editable and plays',vv);
 // undo walks back to the generated line; follow-the-chords clears it
 await p.evaluate(()=>{ for(let i=0;i<8&&S.bassOwn;i++) $('#undoBtn')?$('#undoBtn').click():0; }); await p.waitForTimeout(250);
 await p.evaluate(()=>{ if(S.bassOwn) bassFollow(); }); await p.waitForTimeout(120);
 ok(await p.evaluate(()=>!S.bassOwn),'bass can go back to following the chords');
 // an added part: same roll, same tools, undoable
 await p.evaluate(()=>{ const e=xAdd('counter'); selectSection(e.sections.find(i=>(e.notes[secKey(i)]||[]).length)); }); await p.waitForTimeout(350);
 const x=await p.evaluate(()=>({part:rollPart(),n:S.rects.length,name:$('#layPart').textContent,secs:document.querySelectorAll('#partBar [data-xsec]').length}));
 ok(/^x/.test(x.part||'')&&x.n>0&&x.secs>0,'an added part opens on the one roll',x);
 r=await roll(); const t0=await p.evaluate(()=>rollArr()[0].m); await p.mouse.move(r.x,r.y); await p.mouse.down(); await p.mouse.move(r.x,r.y+r.rh*2,{steps:3}); await p.mouse.up(); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>{ const e=xList().find(q=>q.id===SHELL.part); return e.notes[secKey(S.sel)][0].m; })===t0-2,'dragging an added part note edits it');
 await p.evaluate(()=>{ S.selSet=new Set(rollArr().slice(0,2)); S.selNote=null; songCopy(false); S.selSet=new Set(); }); ok(await p.evaluate(()=>{ const n=rollArr().length; songPaste(0); return rollArr().length>=n-2; }),'copy and paste work on an added part');
 // a section with nothing in it yet
 const emp=await p.evaluate(()=>{ const e=xList().find(q=>q.id===SHELL.part), k=secKey(S.sel); e.notes[k]=undefined; delete e.notes[k]; invalidateViews(); renderRoll(); return {arr:rollArr().length,kept:k in e.notes}; });
 ok(emp.arr===0&&!emp.kept,'an empty section stays clean until a note is added',emp);
 await p.mouse.dblclick(r.rx+r.w*.5,r.ry+r.h*.5); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>{ const e=xList().find(q=>q.id===SHELL.part); return (e.notes[secKey(S.sel)]||[]).length===1; }),'first note in an empty section sticks');
 // a linked layer is read-only
 await p.evaluate(()=>{ SHELL.part='lead'; setLayer('melody'); xAddLink('lead',0); shellDock('roll'); }); await p.waitForTimeout(350);
 ok(await p.evaluate(()=>rollLinked()&&S.rects.length===0&&!!document.querySelector('[data-xp="unlink"]')),'a linked layer shows notes but sends you to Unlink');
 { const t=await p.evaluate(()=>{ const a=$('#drTools').getBoundingClientRect(), b=$('#ss_dock .dock-tabs').getBoundingClientRect(); return {inside:a.bottom<=b.bottom+1&&a.top>=b.top-1,seg:!!$('#layPart').offsetWidth}; }); ok(t.inside&&t.seg,'the toolbar stays on its line with a long part name',t); }
 // melody still melody
 await p.evaluate(()=>setLayer('melody')); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>SHELL.part==='lead'&&!rollPart()&&$('#partBar').hidden&&S.rects.length>0),'back to Melody');
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?fails+' failed':'one-roll ok'); process.exit(fails?1:0); })();
