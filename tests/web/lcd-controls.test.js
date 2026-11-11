// 12 Nov (Jamie: "click on the tempo and drag it up or down to change the tempo, just like on FL studio... click on the 4/4, etc and have a
// little pull down menu... same with the song key"). The top bar's tempo drags (one undo step), the time signature and key open menus.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; H.quietUntil=0; histCheck(true); });
 const box=async sel=>{ const q=await (await p.$(sel)).boundingBox(); return {x:q.x+q.width/2,y:q.y+q.height/2}; };
 // tempo: drag up
 const b0=await p.evaluate(()=>({bpm:S.bpm,steps:H.past.length})); const t=await box('#trBpm');
 await p.mouse.move(t.x,t.y); await p.mouse.down(); await p.mouse.move(t.x,t.y-25,{steps:5}); await p.mouse.move(t.x,t.y-50,{steps:5}); await p.mouse.up(); await p.waitForTimeout(300);
 const b1=await p.evaluate(()=>({bpm:S.bpm,steps:H.past.length,label:H.past[H.past.length-1].label,shown:$('#trBpm').textContent}));
 ok(b1.bpm===b0.bpm+20&&b1.steps===b0.steps+1&&/Tempo/.test(b1.label)&&parseFloat(b1.shown)===b1.bpm,'dragging the tempo up 50px adds 20 BPM, one undo step '+JSON.stringify({b0,b1}));
 await p.mouse.move(t.x,t.y); await p.mouse.down(); await p.mouse.move(t.x,t.y+400,{steps:10}); await p.mouse.up(); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>S.bpm===40),'it stops at 40 BPM');
 await p.mouse.move(t.x,t.y); await p.mouse.down(); await p.mouse.move(t.x,t.y-700,{steps:10}); await p.mouse.up(); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>S.bpm===220),'and at 220 BPM, wider than before (60 to 150)');
 // time signature menu
 const m=await box('#trMeter'); await p.mouse.click(m.x,m.y); await p.waitForTimeout(200);
 const mItems=await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.textContent.replace('✓ ','')));
 ok(mItems.some(t=>t.startsWith('3/4'))&&mItems.some(t=>t.startsWith('6/8')),'clicking 4/4 opens a menu of time signatures '+JSON.stringify(mItems));
 await p.evaluate(()=>{ [...document.querySelectorAll('#ctxMenu [data-ci]')].find(b=>b.textContent.replace('✓ ','').startsWith('3/4')).click(); }); await p.waitForTimeout(300);
 const mm=await p.evaluate(()=>({meter:S.meter,shown:$('#trMeter').textContent,label:H.past[H.past.length-1].label}));
 ok(mm.meter==='3/4'&&mm.shown==='3/4'&&/Time signature 3\/4/.test(mm.label),'choosing 3/4 changes it, one undo step '+JSON.stringify(mm));
 // key menu
 const k=await box('#trKey'); await p.mouse.click(k.x,k.y); await p.waitForTimeout(200);
 const kItems=await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.textContent.replace('✓ ','')));
 ok(kItems.length>=24&&kItems.includes('D minor')&&kItems.includes('A major'),'clicking the key opens all 24 keys '+kItems.length);
 await p.evaluate(()=>{ [...document.querySelectorAll('#ctxMenu [data-ci]')].find(b=>b.textContent.replace('✓ ','')==='D minor').click(); }); await p.waitForTimeout(300);
 const kk=await p.evaluate(()=>({key:S.key,mode:S.mode,shown:$('#trKey').textContent,label:H.past[H.past.length-1].label}));
 ok(kk.key===2&&kk.mode==='aeolian'&&/^D min/.test(kk.shown)&&/Key: D minor/.test(kk.label),'choosing D minor changes the key, one undo step '+JSON.stringify(kk));
 await p.evaluate(()=>histUndo()); await p.waitForTimeout(200); const un=await p.evaluate(()=>({key:S.key,mode:S.mode})); ok(!(un.key===2&&un.mode==='aeolian'),'undo puts the key back '+JSON.stringify(un));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
