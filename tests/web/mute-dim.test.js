// 11 Nov: in the app Melody and Chords were muted and the timeline looked the same; only a small M showed it.
// A part you won't hear (muted, or left out by a solo) is dimmed in the timeline, from the header or the mixer.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; arrRender(); });
 const dim=()=>p.evaluate(()=>Object.fromEntries([...document.querySelectorAll('#arrRows .arr-row.track[data-track]')].map(r=>[r.dataset.track,+getComputedStyle(r.querySelector('.arr-cells')).opacity<.5])));
 let d=await dim(); ok(!Object.values(d).some(Boolean),'nothing is dimmed while everything plays '+JSON.stringify(d));
 await p.evaluate(s=>document.querySelector(s).click(),'#arrRows .arr-row.track[data-track="lead"] [data-trms="m"]'); await p.waitForTimeout(150); d=await dim();
 ok(d.lead&&!d.chords,'muting Melody from its header dims its row '+JSON.stringify(d));
 await p.evaluate(s=>document.querySelector(s).click(),'#arrRows .arr-row.track[data-track="lead"] [data-trms="m"]'); await p.waitForTimeout(150);
 await p.evaluate(()=>stripMS('chords','m')); await p.waitForTimeout(150); d=await dim(); ok(d.chords&&!d.lead,'muting from the mixer dims the row too '+JSON.stringify(d)); await p.evaluate(()=>stripMS('chords','m'));
 await p.evaluate(s=>document.querySelector(s).click(),'#arrRows .arr-row.track[data-track="drums"] [data-trms="s"]'); await p.waitForTimeout(150); d=await dim();
 ok(!d.drums&&d.lead&&d.chords&&d.bass,'soloing Drums dims the others '+JSON.stringify(d));
 await p.evaluate(s=>document.querySelector(s).click(),'#arrRows .arr-row.track[data-track="drums"] [data-trms="s"]'); await p.waitForTimeout(150); d=await dim();
 ok(!Object.values(d).some(Boolean),'unsolo brings them all back '+JSON.stringify(d));
 // loading songs must not add page-wide click listeners again (after two loads a mixer M toggled twice and did nothing in the app)
 const once=await p.evaluate(()=>{ const added=[]; const a0=document.addEventListener; document.addEventListener=function(t,f,c){ added.push(t); return a0.call(this,t,f,c); }; const d=projectData(); applyProject(JSON.parse(JSON.stringify(d))); applyProject(JSON.parse(JSON.stringify(d))); document.addEventListener=a0; renderStrips('song'); const was=S.parts.lead.on; document.querySelector('#cMix [data-ch="lead"] [data-ms="m"]').click(); const now=S.parts.lead.on; stripMS('lead','m'); return {added:added.length,was,now}; });
 ok(!once.added&&once.was!==once.now,'after loading songs, one click on a mixer M mutes (it toggled twice) '+JSON.stringify(once));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
