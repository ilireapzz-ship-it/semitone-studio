// 21 Nov (Jamie: "semitone studio keeps crashing when i try to open it"): the song reopened with Omnisphere and Massive X and Omnisphere crashed the
// app every time; the guard only remembered the last plug-in it opened (Massive X). Now every plug-in being opened is listed, also in the app's
// settings.json, and after a crash all of them wait (the song keeps them) until you press Load them again.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
const OMNI='VST3-Omnisphere-6e03f378-adb7ab48', MASS='VST3-Massive X-77d8efba-bfb39c4e';
(async()=>{ const b=await chromium.launch(); const fails=[], errs=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const start=async(prefTry,lsTry)=>{ const p=await b.newPage({viewport:{width:1600,height:950}}); p.on('pageerror',e=>errs.push(e.message));
  await p.addInitScript(([ls])=>{ try{ localStorage.clear(); if(ls) localStorage.setItem('ss_host_try',ls); }catch(e){} window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); },[lsTry]);
  await p.goto(url); await p.waitForTimeout(1200);
  await p.evaluate(pt=>__fire('st_init',{plugin:false,host:'',project:'',recent:[],prefs:{hostTry:pt}}),prefTry); await p.waitForTimeout(600);
  return p; };
 const setup=([o,m])=>{ S.demo=false; S.parts.lead.sound='h:'+o; S.parts.chords.sound='h:'+m; BOOT.ready=true; __nlog.length=0; hostSync(); return {loads:__nlog.filter(x=>x[0]==='st_host_load'&&x[1].id).map(x=>x[1].id),prefs:__nlog.filter(x=>x[0]==='st_pref'&&x[1].key==='hostTry').map(x=>x[1].value),lead:S.parts.lead.sound,banner:($('#hostHeld')&&!$('#hostHeld').hidden)?$('#hostHeld').textContent:''}; };
 // a normal start: both load, and both are written to settings.json before they load
 let p=await start([],null); let r=await p.evaluate(setup,[OMNI,MASS]);
 ok(r.loads.includes(OMNI)&&r.loads.includes(MASS),'a normal start opens both plug-ins '+JSON.stringify(r.loads));
 ok(r.prefs.length&&r.prefs[r.prefs.length-1].includes(OMNI)&&r.prefs[r.prefs.length-1].includes(MASS),'both are listed in settings.json while they open '+JSON.stringify(r.prefs));
 await p.close();
 // after a crash while both were opening (settings.json survived, browser storage did not)
 p=await start([OMNI,MASS],null); r=await p.evaluate(setup,[OMNI,MASS]);
 ok(!r.loads.length,'after a crash neither is opened again '+JSON.stringify(r.loads));
 ok(r.lead==='h:'+OMNI,'the song still has Omnisphere on the part');
 ok(/Omnisphere/.test(r.banner)&&/Massive X/.test(r.banner)&&/Load them again/.test(r.banner),'Semitone says which plug-ins are waiting and offers to load them '+r.banner.slice(0,120));
 const again=await p.evaluate(()=>{ __nlog.length=0; $('#hostHeld [data-heldgo]').click(); return __nlog.filter(x=>x[0]==='st_host_load'&&x[1].id).map(x=>x[1].id); });
 ok(again.includes(OMNI)&&again.includes(MASS),'Load them again opens both '+JSON.stringify(again));
 await p.close();
 // the old single-name marker in browser storage still counts
 p=await start([],MASS); r=await p.evaluate(setup,[OMNI,MASS]);
 ok(r.loads.includes(OMNI)&&!r.loads.includes(MASS),'an old marker holds back just that plug-in '+JSON.stringify(r.loads));
 await p.close();
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
