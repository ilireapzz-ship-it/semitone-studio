// 11 Nov: in the app the pitch pack said "Saved 8 files" while the Exports folder stayed empty (the app couldn't read the page's base64).
// The page now waits for the app's count of files that really landed (st_pack_done) and says so, or says it couldn't write them.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const errs=[], fails=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const p=await b.newPage(); p.on('pageerror',e=>errs.push(e.message)); await p.addInitScript(()=>{ window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); }); await p.goto(url); await p.waitForTimeout(1200);
 await p.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:'',recent:[]})); await p.waitForTimeout(800);
 const r=await p.evaluate(async()=>{ S.project={id:null,name:'Pack test'}; pitchShow(); Object.keys(PITCH.pack).forEach(k=>PITCH.pack[k]=false); PITCH.pack.notes=true; PITCH.pack.lyric=true; const files=await pitchMake(); const sent=__nlog.filter(x=>x[0]==='st_pack_file'); const ok64=sent.every(x=>{ try{ atob(x[1].b64); return true; }catch(e){ return false; } }); return {files:(files||[]).length,sent:sent.length,ok64,show:__nlog.some(x=>x[0]==='st_pack_show'),txt:$('#ptProg').textContent}; });
 ok(r.files>=1&&r.sent>=r.files&&r.ok64&&r.show&&!/Saved/.test(r.txt),'the pack is sent to the app as standard base64, and the page waits before saying Saved '+JSON.stringify(r));
 let t=await p.evaluate(()=>{ __fire('st_pack_done',{folder:'Pack test - pitch pack',files:0}); return $('#ptProg').textContent; });
 ok(/Couldn’t write the files/.test(t),'if nothing landed it says so '+t);
 t=await p.evaluate(n=>{ __fire('st_pack_done',{folder:'Pack test - pitch pack',files:n}); return $('#ptProg').textContent; },r.files);
 ok(new RegExp(`Saved ${r.files} file`).test(t),'when the files land it says how many '+t);
 console.log(fails.length?fails.length+' FAILED':'all passed'); console.log(errs.slice(0,6)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
