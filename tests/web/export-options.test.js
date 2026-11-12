// 12 Nov, from the gap report: Export audio was a 16-bit WAV of the whole song inside a ZIP, with no choice.
// Under the button: what (whole song, loop region, this section), quality (16 or 24-bit), loudness (as mixed, or for the song's
// destination). In the app (faked here) a plain WAV goes straight into Exports with the time in its name; on the web it stays a ZIP.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const errs=[], fails=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const open=async app=>{ const p=await b.newPage({viewport:{width:1440,height:900}}); p.on('pageerror',e=>errs.push(e.message)); await p.addInitScript(app=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} if(!app) return; window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); },app); await p.goto(url); await p.waitForTimeout(1200); if(app){ await p.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:'',recent:[]})); await p.waitForTimeout(600); } await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; }); return p; };
 const p=await open(true);
 const d0=await p.evaluate(()=>{ expPaint(); const d=$('#expOpts'); return d?{hidden:d.hidden,on:[...d.querySelectorAll('[aria-pressed="true"]')].map(x=>x.textContent),all:[...d.querySelectorAll('[data-exp]')].map(x=>x.dataset.exp+(x.disabled?' off':''))}:null; });
 console.log(JSON.stringify(d0));
 ok(d0&&!d0.hidden&&d0.on.join('|')==='Whole song|16-bit|As mixed','under Export audio: what, quality and loudness, starting as before (whole song, 16-bit, as mixed)');
 ok(d0&&d0.all.includes('range:region off'),'the loop region can only be chosen once there is one');
 // this section, 24-bit, for the destination
 const wav=async()=>p.evaluate(async()=>{ __nlog.length=0; let n=0; const pl=pitchLoud; pitchLoud=function(...a){ n++; return pl.apply(this,a); }; await exportAudio(); pitchLoud=pl; const parts=__nlog.filter(x=>x[0]==='st_pack_file').map(x=>x[1]), show=__nlog.filter(x=>x[0]==='st_pack_show').length, zip=__nlog.filter(x=>x[0]==='st_save').length; if(!parts.length) return {parts:0,zip}; const bin=atob(parts[0].b64), dv=new DataView(Uint8Array.from(bin.slice(0,44),c=>c.charCodeAt(0)).buffer), total=parts.reduce((q,x)=>q+atob(x.b64).length,0); return {parts:parts.length,name:parts[0].name,folder:parts[0].folder,order:parts.every((x,i)=>x.i===i&&x.n===parts.length),riff:bin.slice(0,4)+bin.slice(8,12),ch:dv.getUint16(22,true),sr:dv.getUint32(24,true),bits:dv.getUint16(34,true),dataLen:dv.getUint32(40,true),total,show,zip,loud:n,toast:$('#toast').textContent}; });
 await p.evaluate(()=>{ selectSection(1); for(const k of ['range:section','bits:24','loud:dest']) document.querySelector('#expOpts [data-exp="'+k+'"]').click(); });
 const w=await wav(); console.log(JSON.stringify(w));
 const secs=await p.evaluate(()=>barList('section').map(barTiming).reduce((a,x)=>a+x.dur,0)+2.5);
 ok(w.parts>0&&w.riff==='RIFFWAVE'&&w.bits===24&&w.ch===2,'in the app a plain 24-bit stereo WAV is written (no ZIP)');
 ok(Math.abs(w.dataLen/(w.sr*2*3)-secs)<.6&&w.total===w.dataLen+44,'it holds just this section ('+(w.dataLen/(w.sr*6)).toFixed(1)+' s of '+secs.toFixed(1)+'), whole and in order');
 ok(/-verse-1-\d{8}-\d{6}\.wav$/.test(w.name)&&w.folder==='Audio'&&w.show===1&&w.zip===0,'named after the song and the section, with the time so nothing is written over, and the folder is shown (“'+w.name+'”)');
 ok(w.loud===1,'the loudness is brought to the destination’s level');
 ok(/Documents\/Semitone Studio\/Exports\/Audio/.test(w.toast),'and it says where it went');
 // 16-bit as mixed, loop region
 await p.evaluate(()=>{ S.loopSel={a:0,b:2}; expPaint(); for(const k of ['range:region','bits:16','loud:mix']) document.querySelector('#expOpts [data-exp="'+k+'"]').click(); });
 const w2=await wav(); const two=await p.evaluate(()=>barList('region').map(barTiming).reduce((a,x)=>a+x.dur,0)+2.5);
 ok(w2.bits===16&&w2.loud===0&&Math.abs(w2.dataLen/(w2.sr*4)-two)<.6&&/-bars-1-2-/.test(w2.name),'a loop region exports only those bars, 16-bit, as mixed (“'+w2.name+'”)');
 const rem=await p.evaluate(()=>localStorage.getItem('ss_export')); ok(/"range":"region"/.test(rem)&&/"bits":16/.test(rem),'the choices are remembered');
 // the web page: still a ZIP download, with the same choices
 const q=await open(false);
 const z=await q.evaluate(async()=>{ const saved=[]; downloadsCap={save:async o=>{ saved.push({name:o.filename,len:o.data.length}); }}; $('#audioBtn').hidden=false; expPaint(); selectSection(1); document.querySelector('#expOpts [data-exp="range:section"]').click(); await exportAudio(); return saved; });
 ok(z.length===1&&/-verse-1-audio\.zip$/.test(z[0].name)&&z[0].len>1000,'on the web page it is still a ZIP download (“'+(z[0]||{}).name+'”)');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
