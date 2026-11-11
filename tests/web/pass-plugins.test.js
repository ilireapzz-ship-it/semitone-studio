// 12 Nov (Jamie: "improve the producer passes and profiles by allowing them to use their favourite plugins and presets ...
// sometimes randomize preset selection when it makes sense"). In the desktop app (faked here) a pass offers "Use your <plug-in> on
// <part>" for an instrument the producer is known for when a plug-in of yours recreates it. Off until you tick it; never takes over a
// part that already plays through a plug-in; once the plug-in has listed its sounds one that fits is chosen, among several the same one
// for the same take and another on another take; nothing is invented when none fits.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const errs=[], fails=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const open=async(app,init)=>{ const p=await b.newPage({viewport:{width:1440,height:900}}); p.on('pageerror',e=>errs.push(e.message+' '+(e.stack||'').split('\n')[1])); await p.addInitScript(app=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} if(!app) return; window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); },app); await p.goto(url); await p.waitForTimeout(1200); if(app){ await p.evaluate(d=>__fire('st_init',d),init); await p.waitForTimeout(600); } return p; };
 const p=await open(true,{plugin:false,host:'',project:'',recent:[]});
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; __fire('st_host_plugins',{list:[{id:'VST3-TAL-1',name:'TAL-U-NO-LX',maker:'TAL Software'},{id:'VST3-Other',name:'Some Reverb Synth',maker:'X'}],scanning:false}); });
 await p.waitForTimeout(100);
 const mv=await p.evaluate(()=>{ passOpen('antonoff'); const m=passMoves().filter(x=>x.fav); return m.map(x=>({id:x.id,what:x.what,why:x.why,op:x.op,on:!!PASS.on[x.id],soon:!!x.soon,src:(x.src||[]).length,level:x.level})); });
 console.log(JSON.stringify(mv).slice(0,600));
 const juno=mv.find(x=>x.id==='fav-juno');
 ok(juno&&/^Use your TAL-U-NO-LX on Chords/.test(juno.what)&&/Jack Antonoff is known for/.test(juno.why)&&juno.src>0,'a Jack Antonoff-style pass offers your own Juno plug-in, with what he is known to use it for and a source: “'+(juno?juno.what:'')+'”');
 ok(juno&&!juno.on&&!juno.soon,'it starts switched off, and it is a real move, not “coming soon”');
 const before=await p.evaluate(()=>S.parts.chords.sound);
 await p.evaluate(()=>{ __nlog.length=0; const c=document.querySelector('#passView input[data-pm="fav-juno"]'); c.checked=true; c.dispatchEvent(new Event('change',{bubbles:true})); }); await p.waitForTimeout(900);
 const on=await p.evaluate(()=>({sound:S.parts.chords.sound,load:__nlog.filter(x=>x[0]==='st_host_load').map(x=>x[1].id),side:PASS.side}));
 ok(on.sound==='h:VST3-TAL-1','ticking it puts the plug-in on Chords in the pass version ('+on.sound+', listening to '+on.side+')');
 ok(on.load.includes('VST3-TAL-1'),'and the app is asked to load it');
 // the plug-in loads and lists its sounds: one that fits the part is chosen
 const pick=await p.evaluate(async()=>{ const ch=hostCh('chords'); __nlog.length=0; __fire('st_host_loaded',{ch,ok:true,info:'TAL-U-NO-LX'}); await new Promise(f=>setTimeout(f,60)); const asked=__nlog.some(x=>x[0]==='st_host_presets'); const progs=[{i:0,name:'Init'},{i:1,name:'Warm Pad'},{i:2,name:'Soft Poly Keys'},{i:3,name:'Juno Bass'},{i:4,name:'Arp Lead'}]; __fire('st_host_presetlist',{ch,programs:progs,sounds:[]}); await new Promise(f=>setTimeout(f,60)); const sent=__nlog.filter(x=>x[0]==='st_host_program').map(x=>x[1].i), toast=$('#toast').textContent;
   // the same take picks the same sound; other takes reach the other one
   const seen={}; for(let t=0;t<12;t++){ for(let k=0;k<2;k++){ FAV.wish.chords={id:'VST3-TAL-1',inst:'juno',take:t,t:Date.now()}; __nlog.length=0; __fire('st_host_presetlist',{ch,programs:progs,sounds:[]}); const i=(__nlog.find(x=>x[0]==='st_host_program')||[0,{}])[1].i; (seen[t]=seen[t]||[]).push(i); } }
   return {asked,sent,toast,same:Object.values(seen).every(a=>a[0]===a[1]),kinds:[...new Set(Object.values(seen).map(a=>a[0]))].sort()}; });
 console.log(JSON.stringify(pick));
 ok(pick.asked&&pick.sent.length===1&&[1,2].includes(pick.sent[0]),'when the plug-in has listed its sounds, one whose name fits chords is chosen (preset '+pick.sent[0]+'), not the bass or the lead one');
 ok(/picked from 2 that fit chords/.test(pick.toast)&&/Another take picks another/.test(pick.toast),'and it says so: “'+pick.toast+'”');
 ok(pick.same&&pick.kinds.join()==='1,2','the same take always picks the same sound, and other takes reach the other one (takes gave presets '+pick.kinds.join(' and ')+')');
 // your own sound saved for this style wins; nothing is invented when none fits
 const own=await p.evaluate(async()=>{ const ch=hostCh('chords'), tag=(S.tags&&S.tags[0])||'x'; S.tags=S.tags&&S.tags.length?S.tags:[tag]; FAV.wish.chords={id:'VST3-TAL-1',inst:'juno',take:0,t:Date.now()}; __nlog.length=0; __fire('st_host_presetlist',{ch,programs:[{i:1,name:'Warm Pad'}],sounds:[{name:'My chorus pad',tags:[S.tags[0]]},{name:'Other',tags:['zzz']}]}); const a=__nlog.find(x=>x[0]==='st_host_sound_load'); FAV.wish.chords={id:'VST3-TAL-1',inst:'juno',take:0,t:Date.now()}; __nlog.length=0; __fire('st_host_presetlist',{ch,programs:[{i:0,name:'Init'},{i:1,name:'Default'}],sounds:[]}); return {saved:a&&a[1].name,none:__nlog.filter(x=>x[0]==='st_host_program'||x[0]==='st_host_sound_load').length,toast:$('#toast').textContent}; });
 ok(own.saved==='My chorus pad','a sound you saved for this style is used before any preset (“'+own.saved+'”)');
 ok(own.none===0&&/its own sound/.test(own.toast),'when no sound is named for the job the plug-in keeps its own, and says so: “'+own.toast+'”');
 // Keep is one undo step; Undo brings the old sound back
 const kept=await p.evaluate(()=>{ passKeep(); return {sound:S.parts.chords.sound,labels:H.past.slice(-40).map(h=>h.label)}; });
 ok(kept.sound==='h:VST3-TAL-1'&&kept.labels.some(l=>/Use your TAL-U-NO-LX on Chords/.test(l)),'Keep keeps it as its own undo step');
 const un=await p.evaluate(b4=>{ let n=0; while(S.parts.chords.sound!==b4&&n<12){ histUndo(); n++; } return {sound:S.parts.chords.sound,n}; },before);
 ok(un.sound===before,'and Undo brings the sound you had back ('+un.sound+')');
 // a part that already plays through one of your plug-ins is never taken over
 const safe=await p.evaluate(()=>{ S.parts.chords.sound='h:VST3-Other'; const d=JSON.parse(JSON.stringify(projectData())), f=favOp('host:chords=VST3-TAL-1:juno'); f(d); const r=d.parts.chords.sound; S.parts.chords.sound='style'; return r; });
 ok(safe==='h:VST3-Other','a part that already plays through one of your plug-ins is left alone');
 // outside the desktop app there is nothing to offer
 const w=await open(false); const web=await w.evaluate(()=>{ S.demo=false; passOpen('antonoff'); return passMoves().filter(x=>x.fav).length; });
 ok(web===0,'on the web page, with no plug-ins, the pass makes no such offer');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
