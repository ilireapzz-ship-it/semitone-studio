// 12 Nov (Jamie: "when i put in a plug-in the sound stopped working for everything but the drums"): a trial plug-in (Pianoteq stops after 20
// minutes) went silent and adding any plug-in reloaded it. While the song plays, a plug-in part that should sound but has been silent 4 seconds
// is reloaded with its settings, once a minute at most.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:950}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); }); await p.goto(url); await p.waitForTimeout(1200);
 await p.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:'',recent:[]})); await p.waitForTimeout(800);
 const r=await p.evaluate(()=>{ S.demo=false; S.parts.chords.sound='h:VST3-Pianoteq'; S.hostState={chords:'STATE'}; HOSTS.names={1:'Pianoteq 9'}; window.engineOwns=()=>true; window.posAt=()=>({si:1,beat:0}); S.playing=true; MTN.ch=Array(24).fill(0); __nlog.length=0;
  pwTick(); const first=__nlog.filter(x=>x[0]==='st_host_load').length; PW.quiet.chords=Date.now()-4500; pwTick(); const loads=__nlog.filter(x=>x[0]==='st_host_load'); PW.quiet.chords=Date.now()-4500; pwTick(); const again=__nlog.filter(x=>x[0]==='st_host_load').length;
  MTN.ch[hostCh('chords')]=.2; PW.last={}; PW.quiet.chords=Date.now()-4500; pwTick(); const sounding=__nlog.filter(x=>x[0]==='st_host_load').length; S.playing=false;
  return {first,loads:loads.map(x=>x[1]),again,sounding,toast:$('#toast').textContent}; });
 ok(r.first===0,'it waits: a short quiet moment does nothing');
 ok(r.loads.length===1&&r.loads[0].id==='VST3-Pianoteq'&&r.loads[0].state==='STATE'&&/went quiet/.test(r.toast),'4 seconds of silence where Chords should play: Pianoteq is reloaded with its settings, and Semitone says so '+JSON.stringify(r));
 ok(r.again===1,'not again within a minute');
 ok(r.sounding===1,'a plug-in that is sounding is left alone');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
