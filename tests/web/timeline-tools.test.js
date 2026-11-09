// 9 Nov requests: the timeline stays on every pane; the roll's tools sit in the tab row (no second row); the chord strip sits on the roll, under the
// part's style row; the velocity lane resizes by its top edge and the size saves with the song; a whole picked track shows every section on the roll
// and edits land in the right section; timeline tools (box select, Shift-click, rail ops, Slice, Erase, double-click marker, triple-click editor);
// family buses for new parts (one undo step with the part, joined by the original Bass, placed by their group, tidied when unused).
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; });

 // 1. no pane folds the timeline
 const tl=await p.evaluate(async()=>{ const o={}; for(const id of ['mix','sound','style','record','auto','roll']){ shellDock(id); await new Promise(r=>setTimeout(r,120)); o[id]={tracks:document.querySelectorAll('.arr-row.track').length,rows:!!document.querySelector('#arrRows')&&getComputedStyle($('#arrRows')).display!=='none',board:document.body.classList.contains('sp-board')}; } return o; });
 ok(Object.values(tl).every(v=>v.rows&&v.tracks>=4&&!v.board),'Mix, Sound, Settings, Record and Automation keep the timeline '+JSON.stringify(tl));
 await p.evaluate(()=>{ $('#trKey').click(); }); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>!document.querySelector('.dock-pane[data-pane="style"]').hidden&&getComputedStyle($('#arrRows')).display!=='none'),'clicking the key and tempo opens Settings with the timeline still there');

 // 2. tools in the tab row, chord strip under the style row
 await p.evaluate(()=>{ xAdd('piano'); shellDock('roll'); }); await p.waitForTimeout(300);
 const rows=await p.evaluate(()=>{ const T=document.querySelector('#ss_dock .dock-tabs'), pn=document.querySelector('.dock-pane[data-pane="roll"] .panel'), kids=[...pn.children].filter(e=>e.offsetHeight>0).map(e=>e.id||e.className.split(' ')[0]); return {inTabs:!!T.querySelector('#drTools'),tabRows:[...$('#ss_dock').children].filter(e=>e.offsetHeight>0&&e.classList.contains('dr-tools')).length,mel:!!T.querySelector('#layMel'),kids,over:T.scrollWidth-T.clientWidth}; });
 ok(rows.inTabs&&rows.mel&&rows.tabRows===0,'Melody / Chords / part buttons sit in the tab row; the second row is gone '+JSON.stringify(rows));
 ok(rows.kids.indexOf('partBar')>=0&&rows.kids.indexOf('partBar')<rows.kids.indexOf('clane-wrap')&&rows.kids.indexOf('clane-wrap')<rows.kids.indexOf('roll-box'),'chord strip sits right above the roll, under the piano style row '+rows.kids.join(' > '));
 ok(rows.over<=1,'the tab row does not overflow at 1440 wide');
 await p.setViewportSize({width:1024,height:680}); await p.waitForTimeout(300);
 const narrow=await p.evaluate(()=>{ const T=document.querySelector('#ss_dock .dock-tabs'), T2=$('#drTools'); return {over:T.scrollWidth-T.clientWidth, over2:T2.scrollWidth-T2.clientWidth, hidden:[...T2.querySelectorAll('.tb-g[hidden]')].length}; });
 ok(narrow.over<=1&&narrow.over2<=1,'at 1024 wide the row still fits: less-used groups step out '+JSON.stringify(narrow));
 await p.setViewportSize({width:1440,height:900}); await p.waitForTimeout(300);

 // 3. velocity lane: drag its top edge; saved with the song; closed is slim; undo
 const g=await p.evaluate(()=>{ VEL.open=true; velApply(); const r=document.querySelector('.vel-grip').getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2}; });
 await p.mouse.move(g.x,g.y); await p.mouse.down(); await p.mouse.move(g.x,g.y-40,{steps:4}); await p.mouse.move(g.x,g.y-80,{steps:4}); await p.mouse.up(); await p.waitForTimeout(200);
 const v=await p.evaluate(async()=>{ const L=$('.roll-lane'), o={h:L.offsetHeight,velH:S.velH,saved:projectData().velH,undo:$('#histUndo').title,scroll:document.documentElement.scrollHeight>innerHeight}; $('#velToggle').click(); await new Promise(r=>setTimeout(r,80)); o.closed=L.offsetHeight; $('#velToggle').click(); await new Promise(r=>setTimeout(r,80)); o.reopen=L.offsetHeight; const d=projectData(); d.velH=150; applyProject(d); await new Promise(r=>setTimeout(r,80)); o.loaded=L.offsetHeight; return o; });
 ok(v.h>=170&&v.velH===v.h&&v.saved===v.h&&/velocity lane/.test(v.undo),'dragging the top edge grows the velocity lane, saved in the song as one undo step '+JSON.stringify(v));
 ok(v.closed<v.h&&v.reopen===v.h,'closing goes back to the slim lane, opening returns to your size');
 ok(v.loaded===150,'a song opens with its own velocity lane size');
 ok(!v.scroll,'no page scroll with a tall lane');

 // 4. whole track on the roll
 await p.evaluate(()=>{ trPick('bass'); }); await p.waitForTimeout(300);
 const w=await p.evaluate(()=>({whole:rollWhole(),layer:S.editLayer,part:rollPart(),secs:Object.keys(ROLLW.x0).length,n:S.song.sections.length,notes:Object.values(ROLLW.rects).map(r=>r.length)}));
 ok(w.whole&&w.part==='bass'&&w.secs===w.n&&w.notes.every(n=>n>0),'picking the whole Bass track shows every section of it on the roll '+JSON.stringify(w));
 // small first moves used to drop the bass draft so the note snapped back on release (also in the one-section roll)
 const ed=await p.evaluate(async()=>{ const cv=$('#roll'), si=3, r=cv.getBoundingClientRect(), q=ROLLW.rects[si][1], x=r.left+ROLLW.x0[si]+q.x+q.w/2, y=r.top+q.y+q.h/2, src=q.n.src, m0=src.m;
   const P=(t,yy)=>cv.dispatchEvent(new PointerEvent(t,{bubbles:true,cancelable:true,clientX:x,clientY:yy,button:0,buttons:t==='pointerup'?0:1,pointerId:1,isPrimary:true}));
   P('pointerdown',y); for(let k=1;k<=6;k++) P('pointermove',y-k*5); P('pointerup',y-30); await new Promise(r=>setTimeout(r,200));
   const own=(S.bassOwn||{})[secKey(si)]; return {sel:S.sel,m0,src:src.m,selNote:S.selNote&&S.selNote.m,tool:S.rollTool,now:own&&own[1]&&own[1].m,others:Object.keys(S.bassOwn||{}),undo:$('#histUndo').title,whole:rollWhole()}; });
 ok(ed.sel===3&&ed.now>ed.m0&&ed.others.length===1&&ed.whole,'dragging a note in another section moves it there only, and the roll stays on the whole track '+JSON.stringify(ed));
 await p.evaluate(()=>{ const c=document.querySelector('[data-arrpart="lead"][data-arrsec="0"]'); c.click(); }); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>!rollWhole()),'clicking one clip goes back to the one-section roll');

 // 5. timeline tools
 const cellPt=(pp,si,fx)=>p.evaluate(([pp,si,fx])=>{ const r=document.querySelector(`#arrRows [data-arrpart="${pp}"][data-arrsec="${si}"]`).getBoundingClientRect(); return {x:r.left+r.width*fx,y:r.top+r.height/2}; },[pp,si,fx]);
 const a=await cellPt('lead',1,.1), z=await cellPt('bass',2,.9);
 await p.mouse.move(a.x,a.y); await p.mouse.down(); await p.mouse.move((a.x+z.x)/2,(a.y+z.y)/2,{steps:5}); await p.mouse.move(z.x,z.y,{steps:5}); await p.mouse.up(); await p.waitForTimeout(250);
 const sel=await p.evaluate(()=>({sel:[...TLX.sel].sort(),painted:document.querySelectorAll('#arr .tlx-sel').length,on:TLX.on}));
 ok(['bass|1','bass|2','chords|1','chords|2','lead|1','lead|2'].every(k=>sel.sel.includes(k))&&!sel.sel.some(k=>/[|](0|3|4|5)$/.test(k))&&sel.painted===sel.sel.length,'dragging a box across the timeline picks every clip it touches '+JSON.stringify(sel));
 const up=await p.evaluate(async()=>{ const m0=sectionView(1).melody.map(n=>n.m); $('#rollRail [data-rid="up"]').click(); await new Promise(r=>setTimeout(r,200)); const m1=sectionView(1).melody.map(n=>n.m), o={d:m1.map((m,i)=>m-m0[i]),undo:$('#histUndo').title,bass:Object.keys(S.bassOwn||{}).length}; $('#histUndo').click(); await new Promise(r=>setTimeout(r,200)); o.back=sectionView(1).melody.map(n=>n.m).join()===m0.join(); return o; });
 ok(up.d.every(x=>x===12)&&/Octave up/.test(up.undo)&&up.back,'Octave up on the rail moves every note in the picked clips, one undo step '+JSON.stringify({u:up.undo,bass:up.bass}));
 const q=await p.evaluate(async()=>{ TLX.sel=new Set(['lead|1']); TLX.on=true; const mel=matOf(S.song.sections[1]).melody; mel[0].start+=0.11; invalidateViews(); $('#rollRail [data-rid="quant"]').click(); await new Promise(r=>setTimeout(r,200)); const g=S.snapVal||.25; return {on:mel.every(n=>Math.abs(n.start/g-Math.round(n.start/g))<1e-6),undo:$('#histUndo').title}; });
 ok(q.on&&/Quantize/.test(q.undo),'clicking a clip then Quantize puts its notes on the grid '+JSON.stringify(q));
 const k=await p.evaluate(async()=>{ TLX.sel=new Set(['bass|2','lead|2']); TLX.on=true; document.dispatchEvent(new KeyboardEvent('keydown',{key:'Delete',bubbles:true})); await new Promise(r=>setTimeout(r,200)); return {m:JSON.stringify(S.secMute),undo:$('#histUndo').title}; });
 ok(/"bass":\{"2":true\}/.test(k.m)&&/"lead":\{"2":true\}/.test(k.m)&&/2 clips/.test(k.undo),'Delete takes every picked clip out as one step '+JSON.stringify(k));
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(150);
 const sh=await cellPt('chords',3,.5); await p.keyboard.down('Shift'); await p.mouse.click(sh.x,sh.y); await p.keyboard.up('Shift'); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>TLX.sel.has('chords|3')),'Shift-click adds a clip to the pick');
 // double-click = play marker, triple = big editor
 await p.evaluate(()=>{ tlxClear(); spPut({big:false,fold:false}); }); await p.waitForTimeout(500);
 const big0=await p.evaluate(()=>document.body.classList.contains('sp-big')+':'+JSON.stringify(spGet())+':'+HEAD.lens); const d=await cellPt('lead',2,.5); await p.mouse.click(d.x,d.y); await p.waitForTimeout(80); await p.mouse.click(d.x,d.y); await p.waitForTimeout(250);
 const mk=await p.evaluate(()=>({sel:S.sel,from:S.playFrom,B:METERS[S.meter].B,len:S.song.sections[2].bars*METERS[S.meter].B,big:document.body.classList.contains('sp-big')})); mk.big0=big0;
 ok(mk.sel===2&&Math.abs(mk.from-mk.len/2)<=mk.B&&!mk.big,'double-clicking the timeline moves the play marker there '+JSON.stringify(mk));
 await p.waitForTimeout(600); await p.mouse.click(d.x,d.y); await p.waitForTimeout(60); await p.mouse.click(d.x,d.y); await p.waitForTimeout(60); await p.mouse.click(d.x,d.y); await p.waitForTimeout(250);
 ok(await p.evaluate(()=>document.body.classList.contains('sp-big')),'triple-clicking a clip gives the editor the room');
 // Slice and Erase tools on the timeline
 await p.waitForTimeout(600); const n0=await p.evaluate(()=>{ rollTool('slice'); return S.song.sections.length; }); const sl=await cellPt('bass',4,.5); await p.mouse.click(sl.x,sl.y); await p.waitForTimeout(250);
 const sx=await p.evaluate(([x,y])=>({n:S.song.sections.length,undo:$('#histUndo').title,tool:S.rollTool,at:(document.elementFromPoint(x,y)||{}).className}),[sl.x,sl.y]);
 ok(sx.n===n0+1&&/Split/.test(sx.undo),'with Slice, clicking a clip cuts the section there '+JSON.stringify(sx));
 await p.evaluate(()=>rollTool('erase')); await p.waitForTimeout(500); const er=await cellPt('chords',0,.5); await p.mouse.click(er.x,er.y); await p.waitForTimeout(250);
 ok(await p.evaluate(()=>!!(S.secMute&&S.secMute.chords&&S.secMute.chords[0])),'with Erase, clicking a clip takes it out'); await p.evaluate(()=>rollTool('select'));

 // 6. family buses
 const fb=await p.evaluate(async()=>{ const route=()=>Object.fromEntries(mixChans().filter(c=>rtRoutable(c)).map(c=>[trName(c),rtOutName(c)])); const o={}; o.keys=route(); o.bus=rtShown().map(b=>b.n);
   xAdd('bass2'); await new Promise(r=>setTimeout(r,150)); o.r2=route(); o.b2=rtShown().map(b=>b.n); o.undo=$('#histUndo').title; $('#histUndo').click(); await new Promise(r=>setTimeout(r,200)); o.b3=rtShown().map(b=>b.n); o.r3=route();
   xDuplicate('bass'); await new Promise(r=>setTimeout(r,150)); o.dup=route()['Bass copy']; return o; });
 ok(fb.keys['Piano right hand']==='Keys bus'&&fb.keys['Piano left hand']==='Keys bus'&&fb.bus.indexOf('Keys bus')===fb.bus.indexOf('Music bus')+1,'piano hands share a new Keys bus, listed next to Music '+JSON.stringify(fb.bus));
 ok(fb.r2['Second bass']==='Low end bus'&&fb.r2['Bass']==='Low end bus'&&/second bass/i.test(fb.undo),'a second bass makes a Low end bus that Bass joins, in the same undo step '+JSON.stringify(fb.r2));
 ok(!fb.b3.includes('Low end bus')&&fb.r3['Bass']==='Music bus','Undo takes the bus away with the part');
 ok(fb.dup==='Low end bus','a copy of Bass is routed like bass, not to the Vocals bus ('+fb.dup+')');
 const tidy=await p.evaluate(async()=>{ const x=xList().find(q=>q.name==='Bass copy'); trackDelete(x.id); await new Promise(r=>setTimeout(r,200)); return rtShown().map(b=>b.n); });
 ok(!tidy.includes('Low end bus'),'deleting its last added part removes an untouched family bus '+JSON.stringify(tidy));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
