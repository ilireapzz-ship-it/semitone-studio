// Redo, articulation mode, scrolling and zooming the roll and timeline, splitting, track and mixer order.
const path=require('path'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto('file://'+path.join(__dirname,'..','..','web','semitone-studio.html')); await p.waitForTimeout(1200);
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; shellDock('roll'); setLayer('melody'); S.ctxOn=false; partHear=()=>{}; }); await p.waitForTimeout(300);
 // redo sits by undo
 const rd=await p.evaluate(async()=>{ const w=()=>new Promise(r=>setTimeout(r,60)); const a=S.bpm; histCheck(true); H.quietUntil=0; S.bpm=a+7; H.nextLabel='tempo'; histCheck(true); await w(); $('#histUndo').click(); await w(); const u=S.bpm; $('#histRedoTop').click(); await w(); return [a,u,S.bpm,$('#histRedoTop').previousElementSibling.id]; });
 ok(rd[1]===rd[0]&&rd[2]===rd[0]+7&&rd[3]==='histUndo','Redo button next to Undo',rd);
 // the articulation pop-up only in Articulation mode
 const nb=async()=>p.evaluate(()=>$('#cNoteBar').hidden);
 const at=i=>p.evaluate(i=>{ const R=$('#roll').getBoundingClientRect(), q=S.rects[i]; return {x:R.left+q.x+q.w/2,y:R.top+q.y+q.h/2,m:q.n.src.m}; },i);
 let n=await at(1); await p.mouse.click(n.x,n.y); await p.waitForTimeout(100); ok(await nb(),'no articulation pop-up while writing');
 await p.click('#artModeBtn'); await p.mouse.click(n.x,n.y); await p.waitForTimeout(100); ok(!await nb(),'Articulation mode shows it'); await p.click('#artModeBtn');
 await p.keyboard.press('Escape'); await p.evaluate(()=>{ S.selSet=new Set(); S.selNote=null; renderRoll(); });
 // scroll the roll up and down, pinch to zoom the keys, double-click the keys to fit
 const L=()=>p.evaluate(()=>({hi:S.rl.hi,lo:S.rl.lo,rh:S.rl.rh}));
 const R=await p.evaluate(()=>{ const r=$('#roll').getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2}; }); await p.mouse.move(R.x,R.y);
 const l0=await L(); await p.mouse.wheel(0,400); await p.waitForTimeout(120); const l1=await L(); ok(l1.hi<l0.hi,'scrolling moves down the keys',[l0,l1]);
 await p.mouse.wheel(0,-800); await p.waitForTimeout(120); ok((await L()).hi>l1.hi,'and back up');
 await p.keyboard.down('Control'); await p.mouse.wheel(0,-120); await p.keyboard.up('Control'); await p.waitForTimeout(120); const l2=await L(); ok(l2.rh>l1.rh*1.2,'pinch zooms the keys',[l1,l2]);
 const K=await p.evaluate(()=>{ const r=$('#rollKeys').getBoundingClientRect(); return {x:r.left+20,y:r.top+r.height/2}; }); await p.mouse.dblclick(K.x,K.y); await p.waitForTimeout(120); ok(await p.evaluate(()=>!Object.keys(ROLLV).length),'double-clicking the keys fits the notes again');
 // with notes selected, scrolling moves them
 const sel=await p.evaluate(()=>{ const a=matOfSel().melody.slice(0,2); S.selSet=new Set(a); S.selNote=null; renderRoll(); return a.map(x=>x.m); });
 await p.mouse.move(R.x,R.y); await p.mouse.wheel(0,-100); await p.waitForTimeout(150); const sel2=await p.evaluate(()=>[...S.selSet].sort((a,b)=>a.start-b.start).map(x=>x.m)); ok(sel2.length===2&&sel2[0]>sel[0]&&sel2[0]-sel[0]===sel2[1]-sel[1],'scrolling with notes selected moves them up together',[sel,sel2]);
 await p.evaluate(()=>{ S.selSet=new Set(); renderRoll(); });
 // timeline grid under the clips, zoom that fills the window
 const z=await p.evaluate(()=>[+getComputedStyle($('#tlGrid')).zIndex,+getComputedStyle(document.querySelector('#arrRows .arr-cells')).zIndex]); ok(z[0]<z[1],'grid lies under the clips',z);
 const A=await p.evaluate(()=>{ const r=$('#arr').getBoundingClientRect(); return {x:r.left+r.width*.6,y:r.top+r.height/2}; }); await p.mouse.move(A.x,A.y); await p.keyboard.down('Control'); await p.mouse.wheel(0,-150); await p.keyboard.up('Control'); await p.waitForTimeout(250);
 const tz=await p.evaluate(()=>({z:TLZ,sw:$('#arr').scrollWidth,cw:$('#arr').clientWidth,g:$('#tlGrid').offsetWidth,hd:document.querySelector('#arrRows .arr-name').getBoundingClientRect().left-$('#arr').getBoundingClientRect().left}));
 ok(tz.z>1.5&&tz.sw>tz.cw*1.4&&Math.abs(tz.g-tz.sw)<3,'pinch zooms the timeline and the grid follows',tz);
 await p.evaluate(()=>{ $('#arr').scrollLeft=300; }); await p.waitForTimeout(60); ok(await p.evaluate(()=>Math.abs(document.querySelector('#arrRows .arr-name').getBoundingClientRect().left-$('#arr').getBoundingClientRect().left)<2),'track names stay put while scrolling');
 await p.evaluate(()=>{ tlZoomTo(1); }); await p.waitForTimeout(100);
 // split a section where you right-click: anywhere, and the song sounds the same
 const sig=(si,off)=>p.evaluate(([si,off])=>{ const v=sectionView(si), B=METERS[S.meter].B; return {mel:v.melody.map(q=>q.m+'@'+(+(q.start+off*B).toFixed(3))+':'+(+q.dur.toFixed(3))),ch:v.chords.map(c=>c.sym)}; },[si,off]);
 const whole=await sig(1,0), sp=await p.evaluate(()=>{ const n=S.song.sections.length, c=document.querySelector('[data-arrpart="lead"][data-arrsec="1"]').getBoundingClientRect(); return {n,bars:S.song.sections[1].bars,x:c.left+c.width*3/S.song.sections[1].bars,y:c.top+c.height/2}; });
 await p.mouse.click(sp.x,sp.y,{button:'right'}); await p.waitForTimeout(100); await p.click('#ctxMenu button:has-text("here")'); await p.waitForTimeout(250);
 const sp2=await p.evaluate(()=>({n:S.song.sections.length,a:S.song.sections[1].bars,b:S.song.sections[2].bars,name:S.song.sections[2].name,own:!!S.song.sections[2].mat,type:S.song.sections[2].type}));
 ok(sp2.n===sp.n+1&&sp2.a===3&&sp2.b===sp.bars-3&&/\(2\)/.test(sp2.name)&&sp2.own&&sp2.type==='verse','right-click splits a one-pattern verse after bar 3',sp2);
 const h1=await sig(1,0), h2=await sig(2,3); ok(JSON.stringify([...h1.mel,...h2.mel])===JSON.stringify(whole.mel)&&JSON.stringify([...h1.ch,...h2.ch])===JSON.stringify(whole.ch),'both halves together play exactly what the verse did',{whole,h1,h2});
 const other=await p.evaluate(()=>{ const i=S.song.sections.findIndex((s,k)=>k>2&&s.type==='verse'); return i<0?null:sectionView(i).melody.length; }); ok(other==null||other===whole.mel.length,'the other verses are untouched',other);
 const rt=await p.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())); H.restoring=true; try{ applyProject(d); } finally{ H.restoring=false; } renderAll(); return {mat:S.song.sections[2].mat,n:sectionView(2).melody.length}; }); ok(!!rt.mat&&rt.n===(await sig(2,3)).mel.length,'the split survives saving and opening',rt);
 await p.evaluate(()=>{ selectSection(2); histCheck(true); H.quietUntil=0; const m=matOfSel(); m.melody[0]&&(m.melody[0].m+=1); H.nextLabel='edit'; histCheck(true); }); ok(await p.evaluate(()=>sectionView(1).melody.length>0&&matOf(S.song.sections[1])!==matOf(S.song.sections[2])),'editing the second half leaves the first alone');
 await p.evaluate(()=>{ $('#histUndo').click(); }); await p.waitForTimeout(150); await p.evaluate(()=>{ $('#histUndo').click(); }); await p.waitForTimeout(250); ok(await p.evaluate(n=>S.song.sections.length===n,sp.n),'undo joins it again');
 ok(await p.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())); d.mats[d.sections[1][0]].blockBars=4; d.mats[d.sections[1][0]].prog=d.mats[d.sections[1][0]].prog.slice(0,4); d.mats[d.sections[1][0]].cv=(d.mats[d.sections[1][0]].cv||[]).slice(0,4); applyProject(d); renderAll(); const n=S.song.sections.length; secSplitAt(1,4); return S.song.sections.length===n+1&&!S.song.sections[2].mat&&S.song.sections[2].bars===4; }),'a cut where the pattern repeats shares the notes');
 await p.evaluate(()=>{ $('#histUndo').click(); }); await p.waitForTimeout(200);
 // duplicate a track, add an empty one
 const tr=await p.evaluate(()=>{ S.parts.lead.sound='i:junopoly'; trackDup('lead'); const d=xList()[xList().length-1], dn=S.song.sections.reduce((a,_,i)=>a+xNotes(d,i).length,0), ln=S.song.sections.reduce((a,_,i)=>a+sectionView(i).melody.length,0); trackAddEmpty('chords'); const e=xList()[xList().length-1]; return {dn,ln,dname:d.name,snd:S.parts[d.id].sound,ename:e.name,en:S.song.sections.reduce((a,_,i)=>a+xNotes(e,i).length,0),fam:xFamily(e)}; });
 ok(tr.dn===tr.ln&&tr.dname==='Melody copy'&&tr.snd==='i:junopoly','Duplicate copies every note and the sound',tr); ok(tr.en===0&&tr.ename==='Chords 2'&&tr.fam==='chords','Add an empty track makes an empty one of the same kind',tr);
 const cm=await p.evaluate(()=>ctxTrack('bass',document.body).map(x=>x[0]||x)); ok(cm.some(x=>/Add an empty bass track/.test(x))&&cm.some(x=>/Duplicate Bass/.test(x)),'both are on the track menu',cm);
 // drag a track to a new place
 const hd=await p.evaluate(()=>{ const g=id=>document.querySelector(`#arrRows .tr-n[data-partpop="${id}"]`).getBoundingClientRect(); const a=g('bass'), t=g('lead'); return {x:a.left+30,y:a.top+a.height/2,ty:t.top+3}; });
 await p.mouse.move(hd.x,hd.y); await p.mouse.down(); await p.mouse.move(hd.x,hd.ty+20,{steps:5}); await p.mouse.move(hd.x,hd.ty,{steps:3}); await p.mouse.up(); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>trackIds()[0]==='bass'),'dragging a track moves it',await p.evaluate(()=>trackIds())); ok(await p.evaluate(()=>$('#recipePop').hidden),'the drag does not open the sound picker');
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(200); ok(await p.evaluate(()=>trackIds()[0]==='lead'),'undo puts it back');
 // the mixer: tidy, sized to fit, master movable
 await p.evaluate(()=>shellDock('mix')); await p.waitForTimeout(400);
 const mx=await p.evaluate(()=>{ const pane=document.querySelector('.dock-pane[data-pane="mix"]'), kids=[...pane.children], row=$('#cMix .mix-row'); return {h3:[...document.querySelectorAll('h3.mix-h')].filter(h=>/Channel strips/.test(h.textContent)&&h.getClientRects().length).length,listenLast:kids.indexOf($('#mixListen'))>kids.indexOf($('#cMix').closest('.dock-pane>*')),fad:getComputedStyle($('#cMix')).getPropertyValue('--fadH'),fits:row.scrollWidth<=row.clientWidth+1,last:[...row.children].pop().dataset.ch}; });
 ok(!mx.h3&&mx.listenLast&&/px/.test(mx.fad)&&mx.fits&&mx.last==='master','mixer: no Channel strips title, Listen on at the bottom, faders sized, strips fit',mx);
 const ms=await p.evaluate(()=>{ const g=c=>document.querySelector(`#cMix .strip[data-ch="${c}"] .st-top`).getBoundingClientRect(); const m=g('master'), l=g('lead'); return {x:m.left+m.width/2,y:m.top+6,tx:l.left+4}; });
 await p.mouse.move(ms.x,ms.y); await p.mouse.down(); await p.mouse.move(ms.tx+40,ms.y,{steps:5}); await p.mouse.move(ms.tx,ms.y,{steps:3}); await p.mouse.up(); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>$('#cMix .mix-row').firstElementChild.dataset.ch==='master'),'drag the master bus to the left');
 // less repetition: the Sound tab has no second track list, Room is in Mix, Performance in Style & settings, the picker loads samples
 const td=await p.evaluate(()=>{ shellDock('sound'); const sv=!$('#mixer').getClientRects().length, room=!!document.querySelector('.dock-pane[data-pane="mix"] .room-row'), perf=!!document.querySelector('.dock-pane[data-pane="style"] #perfBox'), hand=[...document.querySelectorAll('.mix-off')].filter(x=>/Hand it off/.test(x.textContent)).every(x=>x.hidden); document.querySelector('[data-partpop="bass"]').click(); const pop=$('#recipePop'), smp=!!pop.querySelector('[data-sa="sample"]'), sf=!!pop.querySelector('[data-sa="sf2"]'); pop.hidden=true; return {sv,room,perf,hand,smp,sf}; });
 ok(Object.values(td).every(Boolean),'Sound tab, Mix and the picker are tidied',td);
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?fails+' failed':'daw-tools ok'); process.exit(fails?1:0); })();
