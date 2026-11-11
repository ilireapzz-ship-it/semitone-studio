// 12 Nov, from the gap report: "Scale lock" shaded the key's rows but locked nothing. A Snap switch beside it makes notes you draw
// or drag land on the nearest note of the key. Off by default (drawing works as it always has); saved with the song.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); selectSection(1); shellDock('roll'); setLayer('melody'); S.ctxOn=false; renderRoll(); drawerTitle(); }); await p.waitForTimeout(300);
 const at=(m,beat)=>p.evaluate(([m,beat])=>{ const L=S.rl, r=$('#roll').getBoundingClientRect(); return {x:r.left+beat*(L.W/L.TB)+5,y:r.top+L.top+(L.hi-m)*L.rh+L.rh/2}; },[m,beat]);
 const mel=()=>p.evaluate(()=>matOfSel().melody.map(n=>n.m)), inKey=m=>p.evaluate(m=>xScale().has(m%12),m);
 // a pitch outside the key and one inside it
 const pcs=await p.evaluate(()=>{ const c=xScale(), out=[60,61,62,63,64,65,66,67,68,69,70,71].find(m=>!c.has(m%12)), inn=[60,62,64,65,67,69,71].find(m=>c.has(m%12)&&c.has((m+2)%12)&&!c.has((m+1)%12)); return {out,inn,btn:!!$('#drSnapK'),pressed:$('#drSnapK')&&$('#drSnapK').getAttribute('aria-pressed')}; });
 ok(pcs.btn&&pcs.pressed==='false','a Snap switch sits beside Scale lock and starts off');
 let q=await at(pcs.out,0); await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(250);
 ok((await mel())[0]===pcs.out,'with Snap off a note outside the key goes exactly where you clicked, as before ('+pcs.out+')');
 await p.keyboard.press('Control+z'); await p.waitForTimeout(250);
 await p.evaluate(()=>$('#drSnapK').click()); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>$('#drSnapK').getAttribute('aria-pressed'))==='true','clicking it turns Snap on');
 q=await at(pcs.out,0); await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(250);
 const m1=(await mel())[0]; ok(m1!==pcs.out&&Math.abs(m1-pcs.out)<=2&&await inKey(m1),'now a click on a row outside the key lands on the nearest note of the key ('+pcs.out+' → '+m1+')');
 // drag a note in the key up one row: it goes to the next note of the key, not to the black row between
 q=await at(pcs.inn,2); await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(250);
 const rh=await p.evaluate(()=>S.rl.rh); q=await at(pcs.inn,2); await p.mouse.move(q.x+8,q.y); await p.mouse.down(); await p.mouse.move(q.x+8,q.y-rh*.6,{steps:3}); await p.mouse.move(q.x+8,q.y-rh*1.05,{steps:3}); await p.mouse.up(); await p.waitForTimeout(300);
 const all=await mel(), moved=await p.evaluate(()=>(matOfSel().melody.find(n=>Math.abs(n.start-2)<.01)||{}).m); ok(moved===pcs.inn+2,'dragging a note up one row carries it to the next note of the key ('+pcs.inn+' → '+moved+')');
 ok((await Promise.all(all.map(inKey))).every(Boolean),'every note is in the key');
 // saved with the song; Scale lock off turns it off
 const sv=await p.evaluate(()=>({saved:projectData().scaleSnap,on:scaleSnapOn()})); ok(sv.saved===true&&sv.on,'the setting is saved with the song');
 await p.evaluate(()=>$('#drLock').click()); await p.waitForTimeout(150);
 const off=await p.evaluate(()=>({on:scaleSnapOn(),dis:$('#drSnapK').disabled})); ok(!off.on&&off.dis,'with Scale lock off, Snap is off too');
 q=await at(pcs.out,6); await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(250);
 ok((await mel()).includes(pcs.out),'and any note can be drawn again');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
