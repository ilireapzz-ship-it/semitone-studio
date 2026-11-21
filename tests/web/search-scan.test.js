// 21 Nov (Jamie: "the search box doesn't do that?"): Search or do anything finds "Find my plug-ins" by typing find my plug-ins, scan or rescan,
// and Enter starts the scan in the desktop app.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:950}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ try{ localStorage.clear(); }catch(e){} window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); });
 await p.goto(url); await p.waitForTimeout(1200); await p.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:'',recent:[]})); await p.waitForTimeout(800);
 await p.evaluate(()=>{ ['#startView','#restoreBar','#hostHeld'].forEach(s=>{ const e=document.querySelector(s); if(e) e.hidden=true; }); });
 for(const words of ['find my plug-ins','scan','rescan']){
  await p.click('#cmdBtn'); await p.fill('#cmdIn',words); await p.waitForTimeout(150);
  const top=await p.evaluate(()=>{ const el=document.querySelector('#cmdRows > *'); return el?el.textContent:''; });
  ok(/Find my plug-ins/.test(top),`typing "${words}" puts Find my plug-ins first: ${top}`);
  await p.evaluate(()=>__nlog.length=0); await p.keyboard.press('Enter'); await p.waitForTimeout(200);
  const sent=await p.evaluate(()=>__nlog.some(x=>x[0]==='st_host_scan')); ok(sent,`Enter starts the plug-in scan ("${words}")`);
  await p.evaluate(()=>{ const m=document.querySelector('#cmdModal'); if(m&&!m.hidden) document.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape'})); });
 }
 // 21 Nov (Jamie: "that re-scan plugins feature should also be in the sound tab")
 await p.evaluate(()=>{ shellDock('sound'); sbRender(); __fire('st_host_plugins',{list:[{id:'a',name:'Serum 2'}],effects:[],scanning:false}); }); await p.waitForTimeout(200);
 const btn=await p.evaluate(()=>{ const b=document.querySelector('#sndBrowse #sbScan'); return b?{t:b.textContent,vis:b.offsetWidth>0}:null; }); ok(btn&&btn.vis&&/Re-scan my plug-ins/.test(btn.t),'the Sound tab has Re-scan my plug-ins '+JSON.stringify(btn));
 await p.evaluate(()=>__nlog.length=0); await p.click('#sbScan'); const r2=await p.evaluate(()=>({sent:__nlog.some(x=>x[0]==='st_host_scan'),busy:!!document.querySelector('#sbScanning')}));
 ok(r2.sent&&r2.busy,'clicking it starts the scan and shows its progress '+JSON.stringify(r2));
 await p.evaluate(()=>__fire('st_host_plugins',{list:[{id:'a',name:'Serum 2'}],effects:[],scanning:false})); await p.waitForTimeout(100); ok(await p.evaluate(()=>!!document.querySelector('#sbScan')),'when it finishes the button comes back');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
