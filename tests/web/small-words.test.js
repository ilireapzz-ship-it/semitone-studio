// 12 Nov, seen while writing a G dorian song in the real app: Home, Share and the loop search called it "G major" (dorian has a minor
// third), and the first bus you add was named "Bus 4".
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const k=await p.evaluate(()=>{ S.demo=false; const was=[S.mode,S.key], out={}; for(const m of Object.keys(MODES)){ S.mode=m; out[m]=keyMinor(); } S.mode='dorian'; S.key=7; homeShow(); const home=$('#startView').innerText; startClose(); S.mode=was[0]; S.key=was[1]; return {out,home:(/G[^·\n]{0,3}(major|minor|dorian)/.exec(home)||[''])[0]}; });
 console.log(JSON.stringify(k.out));
 ok(k.out.dorian===true&&k.out.aeolian===true&&k.out.phrygian!==false,'dorian, aeolian and phrygian count as minor keys');
 ok(k.out.ionian===false&&k.out.mixolydian===false&&k.out.lydian!==true,'ionian, mixolydian and lydian count as major keys');
 ok(!/major/.test(k.home),'Home no longer calls a G dorian song major (“'+k.home+'”)');
 const bus=await p.evaluate(()=>{ shellDock('mix'); const a=rtAddBus(true), b=rtAddBus(true); return (S.busAdd||[]).map(x=>x.n); });
 ok(bus[0]==='Bus 1'&&bus[1]==='Bus 2','the buses you add are Bus 1, Bus 2 ('+bus.join(', ')+')');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
