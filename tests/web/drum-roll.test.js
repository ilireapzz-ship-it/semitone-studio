// Drums have both views, like FL: the step grid and the piano roll edit one pattern.
const path=require('path'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto('file://'+path.join(__dirname,'..','..','web','semitone-studio.html')); await p.waitForTimeout(1200);
 await p.evaluate(()=>{ spPut({big:false}); S.parts.drums.on=true; arrRender(); }); await p.waitForTimeout(300);
 await p.click('[data-arrpart="drums"][data-arrsec="1"]'); await p.waitForTimeout(250);
 ok(await p.evaluate(()=>!document.querySelector('.dock-pane[data-pane="beat"]').hidden),'a drum clip still opens the step grid');
 await p.click('#drSubs [data-subpane="roll"]'); await p.waitForTimeout(250);
 const a=await p.evaluate(()=>{ const t=S.song.sections[S.sel].type, lit=ROWS.reduce((c,r)=>c+(S.beats[t][r]||[]).filter(Boolean).length,0); return {part:rollPart(),rects:S.rects.length,lit,lay:$('#layPart').textContent,TB:S.rl.TB,rows:S.rl.hi-S.rl.lo+1,n:ROWS.length,strip:getComputedStyle($('.clane-wrap')).display}; });
 ok(a.part==='drums'&&a.rects===a.lit&&a.lit>0&&a.lay==='Drums'&&a.TB===4&&a.rows===a.n&&a.strip==='none','the piano roll shows every hit, one line per drum',a);
 const cell=(row,i)=>p.evaluate(([row,i])=>{ const R=$('#roll').getBoundingClientRect(), L=S.rl; return {x:R.left+(i+.5)*.25/L.TB*L.W,y:R.top+L.top+(ROWS.indexOf(row)+.5)*L.rh}; },[row,i]);
 const st=(row,i)=>p.evaluate(([row,i])=>S.beats[S.song.sections[S.sel].type][row][i],[row,i]);
 // add on the roll, it lights in the grid and plays
 const empty=await p.evaluate(()=>{ const t=S.song.sections[S.sel].type, r=S.beats[t].clap?'clap':'snare'; return {r,i:S.beats[t][r].findIndex((v,i)=>!v&&i>0&&!S.beats[t][r][i+1])}; });
 let c=await cell(empty.r,empty.i); await p.mouse.dblclick(c.x,c.y); await p.waitForTimeout(150);
 ok(await st(empty.r,empty.i)===1,'double-click on the roll adds a hit to the pattern');
 ok(await p.evaluate(([r,i])=>drumPerf(barList('song').find(b=>b.si===S.sel)).some(e=>e.r===r&&e.i===i),[empty.r,empty.i]),'the new hit plays');
 // drag it to another drum and later step
 const to=await p.evaluate(r=>{ const t=S.song.sections[S.sel].type, o=ROWS.filter(x=>x!==r); for(const q of o) for(let i=0;i<16;i++) if(!S.beats[t][q][i]) return {r:q,i}; },empty.r);
 const c2=await cell(to.r,to.i); await p.mouse.move(c.x,c.y); await p.mouse.down(); await p.mouse.move(c2.x,c2.y,{steps:5}); await p.mouse.up(); await p.waitForTimeout(150);
 ok(await st(empty.r,empty.i)===0&&await st(to.r,to.i)===1,'dragging moves the hit to another drum and step',to);
 // velocity on the roll reaches the grid data
 const v=await p.evaluate(()=>{ const q=S.selNote, cv=$('#rollLaneC'), R=cv.getBoundingClientRect(); return {x:R.left+VEL.X(q.start),t:R.top,h:R.height}; });
 await p.mouse.move(v.x,v.t+v.h-10); await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(150);
 const vv=await p.evaluate(([r,i])=>(((S.beatVel||{})[S.song.sections[S.sel].type]||{})[r]||{})[i],[to.r,to.i]);
 ok(vv>0&&vv<.3,'velocity on the roll changes the same hit',vv);
 // Delete and undo
 await p.keyboard.press('Delete'); await p.waitForTimeout(150); ok(await st(to.r,to.i)===0,'Delete removes the hit');
 await p.keyboard.press('Meta+z'); await p.waitForTimeout(300); ok(await st(to.r,to.i)===1,'undo brings it back');
 // change the grid, the roll follows
 await p.click('[data-pb="steps"]'); await p.waitForTimeout(200);
 const q=await p.evaluate(()=>{ const t=S.song.sections[S.sel].type; for(const r of ROWS) for(let i=0;i<16;i++) if(!S.beats[t][r][i]) return {r,i}; });
 await p.click(`#seq .step[data-r="${q.r}"][data-i="${q.i}"]`); await p.waitForTimeout(150);
 await p.evaluate(()=>drumRollOpen()); await p.waitForTimeout(250);
 ok(await p.evaluate(([r,i])=>S.rects.some(k=>drumRow(k.n.m)===r&&near(k.n.start,i*.25)),[q.r,q.i]),'a step lit in the grid shows on the roll',q);
 { const t=await p.evaluate(()=>{ const n=rollArr().find(q=>q.m<rollMax()), r0=drumRow(n.m); S.selSet=new Set([n]); S.selNote=n; songNudge(0,1); const a=drumRow(n.m)!==r0; n.m=rollMax(); songNudge(0,5); return {moved:a,top:n.m===rollMax()}; }); ok(t.moved&&t.top,'arrow keys move a hit between drums and stop at the top row',t); }
 // back to melody
 await p.evaluate(()=>setLayer('melody')); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>SHELL.part==='lead'&&!rollPart()&&S.rects.length>0&&getComputedStyle($('.clane-wrap')).display!=='none'),'back to Melody');
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?fails+' failed':'drum-roll ok'); process.exit(fails?1:0); })();
