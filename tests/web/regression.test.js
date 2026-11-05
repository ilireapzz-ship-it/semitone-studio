const {chromium}=require('playwright');
(async()=>{ const b=await chromium.launch({args:['--autoplay-policy=no-user-gesture-required']}); const p=await b.newPage({viewport:{width:1300,height:900}}); const errs=[]; p.on('pageerror',e=>errs.push('PAGEERR '+e.message+' '+(e.stack||'').split('\n')[1])); p.on('console',m=>{ if(m.type()==='error'&&!/ERR_TUNNEL|Failed to load/.test(m.text())) errs.push('CON '+m.text()); });
 await p.goto('file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html')+''); await p.waitForTimeout(1500);
 const r=await p.evaluate(async()=>{ const out=[];
  for(const st of STYLES){ loadStyle(st,true); const v=S.song.sections.map((_,i)=>sectionView(i)); let bad=0; v.forEach(x=>x.melody.forEach(n=>{ if(!(n.m>=40&&n.m<=96)||!isFinite(n.start)) bad++; })); const m=buildMidi(); const xml=typeof buildMusicXML==='function'?buildMusicXML().length:0;
    const buf=await renderSongBuffer(20); const d=buf.getChannelData(0); let nan=0,pk=0; for(let i=0;i<d.length;i++){ if(!isFinite(d[i])) nan++; else pk=Math.max(pk,Math.abs(d[i])); }
    out.push(`${st.id}: bad=${bad} midi=${m.length} xml=${xml} nan=${nan} pk=${pk.toFixed(2)}`); }
  // project round trip
  S.lift=1; S.perf.bass='walk'; S.song.sections[1].energy=.2; S.polish='radio'; const d=JSON.parse(JSON.stringify(projectData())); S.lift=0; S.perf.bass='style'; applyProject(d);
  out.push(`roundtrip lift=${S.lift} bass=${S.perf.bass} e1=${S.song.sections[1].energy} polish=${S.polish}`);
  // orchestra render
  for(const k of ['heroic','noir']){ buildCue(k,{seed:3}); O.polish='gentle'; ensureAudio(); const realAc=ac, realM=master; const bars=oBars('cue').slice(0,6), TM=bars.map(oBarTiming), sr=22050, tot=TM.reduce((a,x)=>a+x.dur,0)+2; const off=new OfflineAudioContext(2,Math.ceil(tot*sr),sr); ac=off; master=newMaster('orch'); PLAY_T0=.1; let tc=.1; bars.forEach((bar,i)=>{ oScheduleBar(bar,tc,TM[i].spb); tc+=TM[i].dur; }); const buf=await off.startRendering(); ac=realAc; master=realM; PLAY_T0=null; const dd=buf.getChannelData(0); let nan=0,pk=0; for(let i=0;i<dd.length;i++){ if(!isFinite(dd[i])) nan++; else pk=Math.max(pk,Math.abs(dd[i])); } out.push(`orch ${k}: nan=${nan} pk=${pk.toFixed(2)}`); }
  return out.join('\n'); });
 console.log(r);
 // live playback smoke: song and orch
 await p.evaluate(()=>{ location.hash='#compose'; }); await p.click('#songBtn'); await p.waitForTimeout(2500); await p.click('#songBtn');
 await p.evaluate(()=>{ location.hash='#orch'; }); await p.waitForTimeout(400); await p.click('#oPlaySec').catch(e=>console.log('orch btn',e.message)); await p.waitForTimeout(2000); await p.click('#oPlaySec').catch(()=>{});
 // phone width
 await p.setViewportSize({width:390,height:844}); await p.evaluate(()=>{ location.hash='#compose'; }); await p.waitForTimeout(400);
 const ov=await p.evaluate(()=>document.documentElement.scrollWidth); console.log('phone scrollWidth',ov);
 for(const id of ['timeline','liveBox','finishBox']){ await p.$eval('#'+id,e=>e.scrollIntoView({block:'start'})); await p.waitForTimeout(150); await p.screenshot({path:'ph_'+id+'.png'}); }
 console.log(errs.slice(0,8)); await b.close(); })();
