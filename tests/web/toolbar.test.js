// Snap and free move, the editing tools, the timeline grid and the menu bar.
const path=require('path'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto('file://'+path.join(__dirname,'..','..','web','semitone-studio.html')); await p.waitForTimeout(1200);
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; spPut({big:false}); shellDock('roll'); setLayer('melody'); S.parts.drums.on=true; arrRender(); }); await p.waitForTimeout(300);
 const note=()=>p.evaluate(()=>{ const R=$('#roll').getBoundingClientRect(), q=S.rects[2]; return {x:R.left+q.x+q.w/2,y:R.top+q.y+q.h/2,start:q.n.src.start,W:S.rl.W,TB:S.rl.TB,left:R.left,top:R.top,h:R.height}; });
 // toolbar is its own line, everything visible at 1440
 const t=await p.evaluate(()=>{ const d=$('#drTools'), r=$('#rollRail').getBoundingClientRect(), k=$('#rollKeys').getBoundingClientRect(); return {h:d.offsetHeight,fits:d.scrollWidth<=d.clientWidth+1,tools:document.querySelectorAll('#rollRail [data-tool]').length,snap:$('#snapBtn').getAttribute('aria-label'),more:!!$('#rollMoreBtn'),left:r.right<=k.left+1}; });
 ok(t.h<=36&&t.fits&&t.tools===4&&t.snap==='Snap'&&t.more&&t.left,'tool rail sits left of the piano keys',t);
 // snap on
 let n=await note(), px=n.W/n.TB; await p.mouse.move(n.x,n.y); await p.mouse.down(); await p.mouse.move(n.x+px*.33,n.y,{steps:4}); await p.mouse.up(); await p.waitForTimeout(120);
 let a=await p.evaluate(()=>S.selNote.start); ok(Math.abs(a/.25-Math.round(a/.25))<1e-6,'snap keeps a note on the grid',a);
 // free move
 await p.keyboard.press('g'); await p.waitForTimeout(80); ok(await p.getAttribute('#snapBtn','aria-label')==='Free move'&&await p.evaluate(()=>S.snap===0),'G switches to free move');
 n=await note(); await p.mouse.move(n.x,n.y); await p.mouse.down(); await p.mouse.move(n.x+px*.33,n.y,{steps:4}); await p.mouse.up(); await p.waitForTimeout(120);
 a=await p.evaluate(()=>S.selNote.start); ok(Math.abs(a/.25-Math.round(a/.25))>.05,'free move puts a note off the grid',a);
 // quantize
 await p.keyboard.press('q'); await p.waitForTimeout(120); a=await p.evaluate(()=>S.selNote.start); ok(Math.abs(a/.25-Math.round(a/.25))<1e-6,'quantize puts it back',a);
 await p.click('#snapBtn'); await p.waitForTimeout(60); ok(await p.getAttribute('#snapBtn','aria-label')==='Snap','the button switches back');
 // ⌘ while dragging frees it for a moment
 n=await note(); await p.keyboard.down('Meta'); await p.mouse.move(n.x,n.y); await p.mouse.down(); await p.mouse.move(n.x+px*.33,n.y,{steps:4}); await p.mouse.up(); await p.keyboard.up('Meta'); await p.waitForTimeout(120);
 a=await p.evaluate(()=>S.selNote.start); ok(Math.abs(a/.25-Math.round(a/.25))>.05,'holding ⌘ frees one drag',a); ok(await p.evaluate(()=>S.snap>0),'snap stays on after');
 // draw and erase tools
 const count=()=>p.evaluate(()=>rollArr().length); const c0=await count();
 await p.keyboard.press('p'); await p.mouse.click(n.left+n.W*.97,n.top+n.h*.12); await p.waitForTimeout(150); ok(await count()===c0+1,'Draw places a note with one click');
 await p.keyboard.press('d'); const e=await p.evaluate(()=>{ const R=$('#roll').getBoundingClientRect(), q=S.rects[S.rects.length-1]; return {x:R.left+q.x+q.w/2,y:R.top+q.y+q.h/2}; }); await p.mouse.click(e.x,e.y); await p.waitForTimeout(150); ok(await count()===c0,'Erase removes it with one click');
 await p.keyboard.press('v'); ok(await p.evaluate(()=>S.rollTool==='select'&&!S.slice),'V is back to Select');
 await p.keyboard.press('c'); ok(await p.evaluate(()=>S.rollTool==='slice'&&S.slice===true),'C is the Slice tool'); await p.keyboard.press('v');
 await p.evaluate(()=>setLayer('chords')); ok(await p.evaluate(()=>$('[data-tool="draw"]').disabled&&$('#tbQuant').disabled&&!$('#snapBtn').disabled),'note-only tools rest on Chords'); await p.evaluate(()=>setLayer('melody'));
 // free-timed drum hit plays where it was put and survives saving
 const d=await p.evaluate(()=>{ snapSet(.25,false); drumRollOpen(); const n=rollArr()[0]; S.selNote=n; S.selSet=new Set([n]); n.start=+(n.start+.07).toFixed(4); refreshAfterEdit(true); const t=S.song.sections[S.sel].type, ev=drumPerf(barList('song').find(b=>b.si===S.sel)), r=drumRow(n.m); return {saved:!!projectData().beatOff,off:Object.values(S.beatOff[t])[0],row:r,hit:ev.filter(e=>e.r===r).map(e=>e.x)}; });
 ok(d.saved&&Math.abs(Object.values(d.off)[0]-.07)<1e-6,'a drum hit can sit off the grid',d);
 await p.evaluate(()=>{ snapSet(.25,true); setLayer('melody'); }); await p.waitForTimeout(100);
 // timeline grid
 const g=await p.evaluate(()=>{ const c=$('#tlGrid'), r=c.getBoundingClientRect(), rows=$('#arrRows').getBoundingClientRect(), d=c.getContext('2d').getImageData(0,0,c.width,c.height).data; let on=0; for(let i=3;i<d.length;i+=4000) if(d[i]) on++; return {vis:!c.hidden,w:r.width,top:Math.abs(r.top-rows.top),h:Math.abs(r.height-rows.height),on,pe:getComputedStyle(c).pointerEvents}; });
 ok(g.vis&&g.w>500&&g.top<2&&g.h<2&&g.on>0&&g.pe==='none','the timeline grid is drawn over the tracks',g);
 await p.click('#tlGridBtn'); await p.waitForTimeout(100); ok(await p.isVisible('.tg-pop'),'Grid opens its options');
 const lines=()=>p.evaluate(()=>{ const c=$('#tlGrid'), d=c.getContext('2d').getImageData(0,0,c.width,1).data; let n=0; for(let i=3;i<d.length;i+=4) if(d[i]) n++; return n; });
 const l1=await lines(); await p.click('[data-tgl="beat"]'); await p.waitForTimeout(100); ok(await lines()>=l1,'Every beat adds lines');
 await p.click('[data-tgl="off"]'); await p.waitForTimeout(100); await p.evaluate(()=>{ TG.shade=false; tlGridSave(); }); ok(await p.evaluate(()=>$('#tlGrid').hidden),'the grid can be turned off');
 await p.evaluate(()=>{ TG.lines='4'; TG.shade=true; tlGridSave(); }); ok(await p.evaluate(()=>{ const q=JSON.parse(localStorage.getItem('ss_tgrid')); return q.lines==='4'; }),'grid options are remembered');
 await p.evaluate(()=>{ $('#recipePop').hidden=true; TG.lines='4'; TG.snap=true; }); ok(await p.evaluate(()=>{ const c=document.querySelector('#timeline [data-i="1"]').getBoundingClientRect(); const q=rulAt(c.left+c.width*.4); return q.beat%16===0; }),'the marker snaps to the grid lines');
 await p.evaluate(()=>{ TG.lines='1'; tlGridSave(); });
 // menu bar
 const sub=async k=>{ await p.click('[data-mb="all"]'); await p.waitForTimeout(60); await p.click(`#ctxMenu button:has-text("${k}")`); await p.waitForTimeout(60); };
 ok(await p.evaluate(()=>document.querySelectorAll('#menuBar button').length===1&&$('#menuBar button').getAttribute('aria-label')==='Settings and menus'),'one settings button instead of the menu words');
 await p.click('[data-mb="all"]'); await p.waitForTimeout(80); ok(/File[\s\S]*Help/.test(await p.textContent('#ctxMenu')),'the settings button lists every menu');
 await p.click('#ctxMenu button:has-text("Edit")'); await p.waitForTimeout(80); ok(await p.isVisible('#ctxMenu')&&/Quantize/.test(await p.textContent('#ctxMenu')),'Edit opens');
 await p.click('#ctxMenu button:has-text("Back")'); await p.waitForTimeout(80); ok(/Tools/.test(await p.textContent('#ctxMenu')),'Back returns to the list');
 await p.keyboard.press('Escape'); await p.mouse.click(700,300); await p.waitForTimeout(80); ok(!await p.isVisible('#ctxMenu'),'clicking away closes it');
 const before=await count(); await sub('Add'); await p.click('#ctxMenu button:has-text("Counter melody")'); await p.waitForTimeout(350); ok(await p.evaluate(()=>/^x/.test(SHELL.part)&&!!rollPart()),'Add makes a part and opens it');
 await sub('Help'); await p.click('#ctxMenu button:has-text("Keyboard shortcuts")'); await p.waitForTimeout(100); ok(await p.isVisible('.hk'),'Help lists the shortcuts');
 await p.mouse.click(700,300); await sub('View'); await p.click('#ctxMenu button:has-text("Timeline grid")'); await p.waitForTimeout(100); ok(await p.isVisible('.tg-pop'),'View opens the grid options');
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?fails+' failed':'toolbar ok'); process.exit(fails?1:0); })();
