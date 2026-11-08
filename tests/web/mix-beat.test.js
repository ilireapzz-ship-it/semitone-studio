// Editor tabs stay put, the beat grid shows drum names, the mixer strips line up, the Room row is gone, and your effect plug-ins go on any strip.
const path=require('path'), {chromium}=require('playwright'); const url='file://'+path.join(__dirname,'..','..','web','semitone-studio.html');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), errs=[];
 const p=await b.newPage({viewport:{width:1440,height:900}}); p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1200);
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; shellDock('roll'); setLayer('melody'); }); await p.waitForTimeout(200);
 // Piano roll, Beat, Automation stay reachable from the piano roll
 const tabs=await p.evaluate(()=>[...document.querySelectorAll('#drSubs [data-subpane]')].filter(b=>b.getClientRects().length).map(b=>b.dataset.subpane));
 ok(['roll','beat'].every(t=>tabs.includes(t))&&!tabs.includes('auto'),'Piano roll and Beat tabs show on the piano roll (Automation has its own drawer tab)',tabs);
 await p.click('[data-drawer="mix"]'); await p.waitForTimeout(100); await p.click('#drSubs [data-subpane="auto"]'); await p.waitForTimeout(100); ok(await p.evaluate(()=>!document.querySelector('.dock-pane[data-pane="auto"]').hidden&&!!$('#autCv').getClientRects().length),'Automation opens from the Mix tab');
 await p.evaluate(()=>shellDock('beat')); await p.waitForTimeout(150);
 // the beat grid: drum names readable, compact header, one hint line
 const bt=await p.evaluate(()=>{ const r=document.querySelector('.seq-row[data-row="hat"] .rl, .seq-row[data-row] .rl'), n=[...document.querySelectorAll('.seq-row[data-row] .rl-n')]; return {w:r.getBoundingClientRect().width,cut:n.filter(x=>x.scrollWidth>x.clientWidth+1).map(x=>x.textContent),hint:$('.beat-hint').getBoundingClientRect().height,about:!!$('.beat-about'),roll:!!$('#beatRollBtn')}; });
 ok(bt.w>=120&&!bt.cut.length,'drum names have room',bt); ok(bt.hint<20&&bt.about&&!bt.roll,'one hint line, About box, no duplicate Piano roll button',bt);
 ok(await p.evaluate(()=>{ const s=document.querySelector('.seq-row[data-row] .step.on'); return s&&getComputedStyle(s).backgroundColor!==getComputedStyle(s.parentElement.querySelector('.step:not(.on):not(.acc)')).backgroundColor; }),'lit steps stand out');
 // the mixer: no Room row, every fader starts at the same height, nothing cut off
 await p.evaluate(()=>shellDock('mix')); await p.waitForTimeout(300);
 const mx=await p.evaluate(()=>{ const pane=document.querySelector('.dock-pane[data-pane="mix"]'), S_=[...document.querySelectorAll('#cMix .mix-row .strip')], fad=S_.map(s=>Math.round(s.querySelector('.st-fz').getBoundingClientRect().top)), bot=S_.map(s=>Math.round(s.querySelector('.st-fz').getBoundingClientRect().bottom)), pr=pane.getBoundingClientRect(); return {room:!!document.querySelector('.room-row')&&!!document.querySelector('.room-row').getClientRects().length,fad,bot,inside:S_.every(s=>s.getBoundingClientRect().bottom<=pr.bottom+1),n:S_.length,roomOn:S.room}; });
 ok(!mx.room&&mx.roomOn==='off','Room row gone; new songs are dry',mx); ok(mx.n>=5&&Math.max(...mx.fad)-Math.min(...mx.fad)<=1&&Math.max(...mx.bot)-Math.min(...mx.bot)<=1,'faders line up across strips',mx); ok(mx.inside,'strips fit the pane',mx);
 // an older song that used Room says so and can turn it off in one step
 const ro=await p.evaluate(async()=>{ const d=JSON.parse(JSON.stringify(projectData())); d.room='hall'; d.roomMix=.28; applyProject(d); const shown=!$('#roomOld').hidden; histCheck(true); $('#roomOld button').click(); const off=S.room, gone=$('#roomOld').hidden; await new Promise(r=>setTimeout(r,60)); $('#histUndo').click(); await new Promise(r=>setTimeout(r,60)); return {shown,off,gone,back:S.room}; });
 ok(ro.shown&&ro.off==='off'&&ro.gone&&ro.back==='hall','old room reverb: notice, turn off, undo',ro);
 await p.close();
 // your effect plug-ins on any strip (desktop app, faked here)
 const q=await b.newPage({viewport:{width:1440,height:900}}); q.on('pageerror',e=>errs.push(e.message));
 await q.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); });
 await q.goto(url); await q.waitForTimeout(1200); await q.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:'',recent:[]})); await q.waitForTimeout(700);
 await q.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; __fire('st_host_plugins',{list:[],effects:[{id:'VST3-Valhalla-1',name:'ValhallaRoom',maker:'Valhalla'},{id:'AU-Comp-2',name:'Pro-C 2',maker:'FabFilter'}],scanning:false}); BOOT.ready=true; shellDock('mix'); }); await q.waitForTimeout(300);
 const pf=await q.evaluate(()=>[...document.querySelectorAll('#cMix [data-pfxadd]')].map(b=>b.dataset.pfxadd));
 ok(['lead','chords','bass','drums','master'].every(k=>pf.includes(k)),'+ plug-in on every part and the master',pf);
 await q.evaluate(()=>{ __nlog.length=0; document.querySelector('#cMix [data-pfxadd="drums"]').click(); }); await q.waitForTimeout(60);
 const items=await q.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.textContent.trim()));
 ok(items.some(t=>/ValhallaRoom/.test(t))&&items.some(t=>/Pro-C 2/.test(t)),'the menu lists your effects',items);
 const ad=await q.evaluate(async()=>{ [...document.querySelectorAll('#ctxMenu [data-ci]')].find(b=>/ValhallaRoom/.test(b.textContent)).click(); await new Promise(r=>setTimeout(r,80)); const ld=__nlog.filter(x=>x[0]==='st_host_fx_load'&&x[1].id), chip=document.querySelector('#cMix .strip[data-ch="drums"] .pfx-chip'); return {list:(S.hostFx.drums||[]).map(x=>x.name),load:ld.map(x=>x[1]),chip:chip&&chip.textContent,eng:engineOwns(),t:hostFxTarget('drums')}; });
 ok(ad.list[0]==='ValhallaRoom'&&ad.load.some(x=>x.t===ad.t&&x.slot===0&&x.id==='VST3-Valhalla-1')&&/ValhallaRoom/.test(ad.chip||''),'adding loads it on the drums channel and shows it on the strip',ad);
 ok(ad.eng,'with an effect on a built-in part, the engine plays the song');
 const op=await q.evaluate(()=>{ __nlog.length=0; document.querySelector('#cMix .strip[data-ch="drums"] [data-pfxopen]').click(); return __nlog.find(x=>x[0]==='st_host_fx_open'); }); ok(op&&op[1].slot===0,'clicking it opens its window',op);
 const sv=await q.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())); return (d.hostFx.drums||[]).length; }); ok(sv===1,'saved with the song');
 const rm=await q.evaluate(async()=>{ __nlog.length=0; document.querySelector('#cMix .strip[data-ch="drums"] [data-pfxdel]').click(); await new Promise(r=>setTimeout(r,60)); const gone=!(S.hostFx.drums||[]).length, cleared=__nlog.some(x=>x[0]==='st_host_fx_load'&&x[1].id===''&&x[1].slot===0); $('#histUndo').click(); await new Promise(r=>setTimeout(r,80)); return {gone,cleared,back:(S.hostFx.drums||[]).length}; });
 ok(rm.gone&&rm.cleared&&rm.back===1,'remove unloads it, Undo brings it back',rm);
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?`mix-beat: ${fails} failed`:'mix-beat ok'); process.exit(fails?1:0); })();
