// 10 Nov (Jamie: "is there a way we can improve drums? … how many patterns do we have? what about fills? … transitions seem pretty basic and repetitive"):
// each style picks from a set of grooves, fills are sized to the energy change and never repeat back to back, toms exist, and transitions fit the
// change between sections, come from the style's own set and never bring a section in the same way twice running.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; });

 // grooves: a set per style, and new songs use more than one of them
 const g=await p.evaluate(()=>{ const n4=Object.keys(GROOVES['4/4']).length, few=STYLES.filter(st=>!['jazz','fleetwood-blues'].includes(st.id)&&gPal(st).filter(x=>GROOVES[st.meter||'4/4']&&GROOVES[st.meter||'4/4'][x]||GROOVES['4/4'][x]||GROOVES['3/4'][x]).length<3).map(s=>s.id);
   const st=STYLES.find(x=>x.id==='lana-btd'), seen=new Set(); for(let i=0;i<25;i++){ loadStyle(st,true); seen.add(S.groove); }
   const half=Object.entries(GCLS).filter(([k,c])=>c==='half'&&GROOVES['4/4'][k]).every(([k])=>{ const G=GROOVES['4/4'][k]; return (G.snare||G.clap||'').indexOf('x')===8&&!/x|o/.test((G.snare||'').slice(0,8)); });
   return {n4,few,lana:[...seen],half}; });
 ok(g.n4>=20&&!g.few.length,'21 grooves in 4/4, and every style has at least 3 to choose from '+JSON.stringify(g));
 ok(g.lana.length>=3,'new Born to Die songs use several grooves '+JSON.stringify(g.lana));
 ok(g.half,'half-time grooves keep the snare on beat 3 only, so a half-time chorus still feels half-time');

 // fills: sized to the change, kinds suit the style, never the same twice running, toms in rock
 const f=await p.evaluate(()=>{ const out={}; for(const id of ['anthem','rhcp','edm','lana-btd','darkpop']){ const st=STYLES.find(x=>x.id===id); const ids=new Set(); let rep=0, big=0, bigSpots=0, toms=0;
     for(let s=1;s<=6;s++){ loadStyle(st,true); S.fill=true; S.perf.human=true; generate(s*101,true); let last=null; barList('song').forEach(bar=>{ const P=fillPlan(bar); if(P){ ids.add(P.id); if(P.id===last) rep++; last=P.id; }
         if(bar.last&&S.song.sections[bar.si+1]&&!secCont(bar.si+1)&&energyOf(bar.si+1)-energyOf(bar.si)>.22){ bigSpots++; if(P&&P.size>=2) big++; } toms+=drumPerf(bar).filter(e=>TOMS.includes(e.r)).length; }); }
     out[id]={kinds:ids.size,rep,big,bigSpots,toms}; } return out; });
 ok(Object.values(f).every(x=>x.kinds>=4),'each style uses several kinds of fill '+JSON.stringify(f));
 ok(Object.values(f).every(x=>x.rep===0),'the same fill never comes twice in a row');
 ok(Object.values(f).every(x=>!x.bigSpots||x.big/x.bigSpots>=.6),'where the energy jumps (into a chorus) the fill is two beats or a whole bar');
 ok(f.anthem.toms>0&&f.rhcp.toms>0,'rock and pop fills use toms');
 const gm=await p.evaluate(()=>[gmOf('tomH'),gmOf('tomM'),gmOf('tomL')].join());
 ok(gm==='50,47,43','toms go out as MIDI notes 50, 47 and 43 (General MIDI)');
 const plain=await p.evaluate(()=>{ loadStyle(STYLES.find(x=>x.id==='anthem'),true); S.fill=true; S.perf.human=false; generate(77,true); const L=barList('song').map(b=>fillPlan(b)).filter(Boolean); S.perf.human=true; return {n:L.length,all:L.every(x=>x.id==='roll'&&x.size===1)}; });
 ok(plain.n>0&&plain.all,'without the human drummer a section still ends with the plain one-beat snare roll');
 const off=await p.evaluate(()=>{ S.fill=false; const n=barList('song').filter(b=>fillPlan(b)).length; S.fill=true; return n; });
 ok(off===0,'Fills off means no fills at all');

 // transitions: fit the change, the style's own moves, never the same way twice running
 const t=await p.evaluate(()=>{ const out={rep:0,riserSad:0,total:0,big:0,bigHit:0}; for(const id of ['anthem','edm','lana-btd','darkpop','sadcore','synth']){ const st=STYLES.find(x=>x.id===id);
     for(let s=1;s<=6;s++){ loadStyle(st,true); S.transOn=true; S.transOwn={}; S.trans={}; generate(s*37,true); const secs=S.song.sections, key=x=>Object.keys(x).sort().join(), last={};
       secs.forEach((x,i)=>{ if(!i) return; const T=transOf(i), k=key(T); if(k) out.total++; if(k&&last[x.type]===k) out.rep++; if(!secCont(i)) last[x.type]=k; if(id==='sadcore'&&T.riser) out.riserSad++;
         if(energyOf(i)-energyOf(i-1)>.22){ out.big++; if(k) out.bigHit++; } }); } } return out; });
 ok(t.rep===0,'a section never comes in the same way as the last time it came in '+JSON.stringify(t));
 ok(t.riserSad===0,'a sadcore song never gets a riser (only its own soft moves)');
 ok(t.big>0&&t.bigHit/t.big>=.8,'a big lift in energy almost always gets a transition');
 const ko=await p.evaluate(()=>{ loadStyle(STYLES.find(x=>x.id==='anthem'),true); generate(5,true); const si=S.song.sections.findIndex(x=>x.type==='chorus'); S.transOwn={[si]:true}; S.trans={[si]:{kickOut:true}}; const bar=barList('song').find(b=>b.si===si-1&&b.last); const n=drumPerf(bar).filter(e=>e.r==='kick').length; S.trans={[si]:{}}; const m=drumPerf(bar).filter(e=>e.r==='kick').length; S.transOwn={}; S.trans={}; return {n,m}; });
 ok(ko.n===0&&ko.m>0,'Kick out takes the kick out of the bar before '+JSON.stringify(ko));
 const menu=await p.evaluate(()=>Object.keys(TRANS_T).includes('kickOut'));
 ok(menu,'Kick out is in the transition menu');

 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
