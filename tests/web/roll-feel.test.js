// The piano roll behaves like FL's: every touch is heard, a selection moves as one, and picking a plug-in opens it.
const path=require('path'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto('file://'+path.join(__dirname,'..','..','web','semitone-studio.html')); await p.waitForTimeout(1200);
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; spPut({big:false}); shellDock('roll'); setLayer('melody'); S.ctxOn=false; window.__h=[]; const q=partHear; partHear=(p,m,v,d)=>{ __h.push([p,m]); }; }); await p.waitForTimeout(300);
 const rect=i=>p.evaluate(i=>{ const R=$('#roll').getBoundingClientRect(), q=S.rects[i]; return {x:R.left+q.x+q.w/2,y:R.top+q.y+q.h/2,m:q.n.src.m,rh:S.rl.rh,left:R.left,top:R.top,W:S.rl.W,h:R.height}; },i);
 const heard=()=>p.evaluate(()=>{ const a=__h.slice(); __h.length=0; return a; });
 // clicking a note plays it
 let n=await rect(1); await p.mouse.click(n.x,n.y); await p.waitForTimeout(80); let h=await heard(); ok(h.length===1&&h[0][0]==='lead'&&h[0][1]===n.m,'clicking a note plays it',h);
 // entering a note plays it
 await p.mouse.dblclick(n.left+n.W*.985,n.top+n.h*.3); await p.waitForTimeout(150); h=await heard(); ok(h.length>=1&&h[h.length-1][0]==='lead','double-clicking a new note plays it',h);
 await p.keyboard.press('Delete'); await p.waitForTimeout(120); await heard();
 // dragging up plays each new pitch
 n=await rect(1); await p.mouse.move(n.x,n.y); await p.mouse.down(); await p.mouse.move(n.x,n.y-n.rh*2.1,{steps:6}); await p.mouse.up(); await p.waitForTimeout(120); h=await heard(); ok(h.some(x=>x[1]===n.m+1)&&h.some(x=>x[1]===n.m+2),'dragging plays the notes it crosses',h);
 // select three, drag one, all move together and keep their shape
 const before=await p.evaluate(()=>{ const a=matOfSel().melody.slice(0,3); S.selSet=new Set(a); S.selNote=null; renderRoll(); return a.map(x=>[x.m,x.start]); });
 n=await rect(1); const px=n.W/(await p.evaluate(()=>S.rl.TB)); await p.mouse.move(n.x,n.y); await p.mouse.down(); await p.mouse.move(n.x+px,n.y-n.rh*2,{steps:6}); await p.mouse.up(); await p.waitForTimeout(150);
 let after=await p.evaluate(()=>[...S.selSet].sort((a,b)=>a.start-b.start).map(x=>[x.m,x.start])); ok(after.length===3&&after.every((a,i)=>a[0]===before[i][0]+2&&Math.abs(a[1]-before[i][1]-1)<1e-6),'a selection moves as one',{before,after});
 ok(await p.evaluate(()=>S.selSet.size===3),'the notes stay selected after the move');
 // a selection acts together: velocity, octave, right-click delete
 await p.evaluate(()=>{ $('#rollRail [aria-label="Octave up"]').click(); }); await p.waitForTimeout(120); ok(await p.evaluate(b=>[...S.selSet].sort((a,c)=>a.start-c.start).every((a,i)=>a.m===b[i][0]+14),before),'octave moves all selected');
 const c0=await p.evaluate(()=>matOfSel().melody.length); const q=await p.evaluate(()=>{ const R=$('#roll').getBoundingClientRect(), r=S.rects.find(x=>S.selSet.has(x.n.src)); return {x:R.left+r.x+r.w/2,y:R.top+r.y+r.h/2}; }); await p.mouse.click(q.x,q.y,{button:'right'}); await p.waitForTimeout(150); ok(await p.evaluate(()=>matOfSel().melody.length)===c0-3,'right-click removes all selected');
 // the piano keys
 await heard(); const k=await p.evaluate(()=>{ const R=$('#rollKeys').getBoundingClientRect(), L=S.rl; return {x:R.left+30,y:R.top+L.top+(L.hi-64+.5)*L.rh}; }); await p.mouse.click(k.x,k.y); await p.waitForTimeout(80); h=await heard(); ok(h.length===1&&h[0][1]===64,'clicking a key plays it',h);
 const px0=await p.evaluate(()=>{ const c=$('#rollKeys'), d=c.getContext('2d').getImageData(0,0,c.width,c.height).data, seen=new Set(); for(let i=0;i<d.length;i+=40) seen.add(d[i]+','+d[i+1]+','+d[i+2]); return seen.size; }); ok(px0>40,'the keys have real shading',px0);
 // other parts sound as themselves
 await p.evaluate(()=>{ shellDock('bass'); }); await p.waitForTimeout(200); n=await rect(0); await heard(); await p.mouse.click(n.x,n.y); await p.waitForTimeout(80); h=await heard(); ok(h.length===1&&h[0][0]==='bass','bass notes play on bass',h);
 await p.evaluate(()=>setLayer('melody')); await p.waitForTimeout(100);
 // plug-ins open, and play previews through the plug-in
 const em=await p.evaluate(()=>{ window.__e=[]; hostsHere=()=>true; nEmit=(n,d)=>__e.push([n,d]); S.parts.lead.sound='h:Test'; arrRender(); document.querySelector('[data-partpop="lead"]').click(); return __e.filter(x=>x[0]==='st_host_open').length; });
 ok(em===1&&await p.evaluate(()=>$('#recipePop').hidden),'clicking an instrument with a plug-in opens it');
 const nt=await p.evaluate(()=>{ __e.length=0; delete window.partHear; return 1; });
 ok(await p.evaluate(()=>{ __e.length=0; S.parts.bass.sound=''; soundSet('bass','h:Other'); const a=__e.filter(x=>x[0]==='st_host_open').length; HOSTS.showCh===hostCh('bass'); return a===0&&HOSTS.showCh===hostCh('bass'); }),'picking a plug-in waits for it to load');
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?fails+' failed':'roll-feel ok'); process.exit(fails?1:0); })();
