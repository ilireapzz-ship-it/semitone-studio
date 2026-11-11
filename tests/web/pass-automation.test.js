// 12 Nov (Jamie: "improve the automations they'd draw"). The lanes a producer pass draws are closer to how an engineer plays them:
// a volume ride goes phrase by phrase and glides (it was a step on every bar line); a move drawn in several places grows through the
// song (the last one is the biggest); a build that opens a sound up lands in the next section instead of snapping shut on its downbeat.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; });
 // 1. the ride
 const ride=await p.evaluate(()=>{ const B=autB(), secs=autSecs(), v=secs.findIndex(s=>s.type==='verse'), x0=secs[v].startBar*B, x1=x0+secs[v].bars*B; const r=autRunMove('auto:lead:volume:ride{max:2dB}@verse','test'); const rest=autNorm('lead','vol',autRest('lead','vol')), db=x=>20*Math.log10(tV(VOLDEF,autValAt('lead','vol',x))/tV(VOLDEF,rest));
   const xs=[]; for(let x=x0;x<x1;x+=.125) xs.push(x); const d=xs.map(db), mel=autMelodyPitch(x0,x1), jumps=d.slice(1).map((q,i)=>Math.abs(q-d[i])), onBar=[], off=[]; for(let i=1;i<d.length;i++) if(Math.abs(d[i]-d[i-1])>1e-3) (Math.abs((xs[i]-x0)%B)<.13||Math.abs((xs[i]-x0)%B-B)<.13?onBar:off).push(xs[i]);
   const inGap=xs.filter(x=>!mel.some(n=>x>n[0]-1.1&&x<n[1]+1.1)).map(db);
   return {ran:!!r,max:+Math.max(...d).toFixed(2),min:+Math.min(...d).toFixed(2),bigJump:+Math.max(...jumps).toFixed(2),moves:off.length+onBar.length,offBar:off.length,gap:inGap.length?+Math.max(...inGap.map(Math.abs)).toFixed(2):0,notes:mel.length}; });
 console.log(JSON.stringify(ride));
 ok(ride.ran&&ride.max<=2.05&&ride.min>=-2.05&&(ride.max>.3||ride.min<-.3),'the ride moves the fader within its 2 dB limit ('+ride.min+' to '+ride.max+' dB)');
 ok(ride.bigJump<=.75,'it glides: no jump bigger than 0.75 dB in an eighth of a beat (the largest is '+ride.bigJump+' dB; it used to step on every bar line)');
 ok(ride.offBar>ride.moves*.5,'and it follows the phrases, not the bar lines ('+ride.offBar+' of '+ride.moves+' moving moments are off the bar line)');
 ok(ride.gap<.15,'between phrases the fader is back at rest ('+ride.gap+' dB)');
 // 2 and 3. a build drawn in every verse grows, and lands in the next section
 const bl=await p.evaluate(()=>{ const B=autB(), secs=autSecs(), vs=secs.map((s,i)=>s.type==='verse'?i:-1).filter(i=>i>=0); const tg=autTargetTg('chords','brightness'), rest=autNorm('chords',tg,autRest('chords',tg)); const r=autRunMove('auto:chords:brightness:build{from:600Hz,to:9kHz}@verse','test');
   const ends=vs.map(i=>(secs[i].startBar+secs[i].bars)*B), top=ends.map(x=>autValAt('chords',tg,x-.02)), after=ends.map(x=>[.05,B,2*B+.1].map(dx=>autValAt('chords',tg,x+dx)));
   return {ran:!!r,n:vs.length,rest:+rest.toFixed(3),top:top.map(v=>+v.toFixed(3)),after:after.map(a=>a.map(v=>+v.toFixed(3)))}; });
 console.log(JSON.stringify(bl));
 ok(bl.ran&&bl.n>=2&&bl.top[bl.top.length-1]>bl.top[0]+.02,'a build drawn in every verse reaches further each time (ends at '+bl.top.join(' then ')+')');
 // a build that goes past the part's resting brightness (here: the default build, which opens a quarter of the range above rest)
 const ld=await p.evaluate(()=>{ const B=autB(), secs=autSecs(), vs=secs.map((s,i)=>s.type==='verse'?i:-1).filter(i=>i>=0), tg=autTargetTg('chords','brightness'), rest=autNorm('chords',tg,autRest('chords',tg)); autRunMove('auto:chords:brightness:build@verse','test'); const x=(secs[vs[vs.length-1]].startBar+secs[vs[vs.length-1]].bars)*B; return {top:+autValAt('chords',tg,x-.02).toFixed(3),after:[.05,B,2*B+.1].map(dx=>+autValAt('chords',tg,x+dx).toFixed(3)),rest:+rest.toFixed(3)}; });
 console.log(JSON.stringify(ld)); bl.rest=ld.rest;
 const A=ld.after; ok(ld.top>ld.rest+.04&&A[0]>bl.rest+.03&&A[1]<A[0]&&A[1]>bl.rest-.01&&Math.abs(A[2]-bl.rest)<.02,'after the build the next section starts where the build arrived and eases back to rest within two bars ('+A.join(' → ')+', rest '+bl.rest+')');
 // a volume build does not land (the hit is the point) and one undo takes a whole pass move back
 const vol=await p.evaluate(()=>{ const B=autB(), secs=autSecs(), v=secs.findIndex(s=>s.type==='verse'), x1=(secs[v].startBar+secs[v].bars)*B, rest=autNorm('drums','vol',autRest('drums','vol')); autRunMove('auto:drums:volume:build{from:-6dB,to:0dB}@verse','test'); return {after:+autValAt('drums','vol',x1+.05).toFixed(3),rest:+rest.toFixed(3)}; });
 ok(Math.abs(vol.after-vol.rest)<.02,'a volume build is back at the fader’s own level on the downbeat ('+vol.after+')');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
