// 12 Nov (Jamie: "make sure our EQ is accurate with the spectrum reading and calibrated right... the visuals are still not calibrated right").
// The spectrum is drawn on the EQ's own dB scale: a +12 dB bell lifts the after-EQ line above the before line by the same pixels as the curve,
// at the band's frequency; shelf handles sit on the curve (at half their gain).
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('mix'); eqOpen('chords','song'); const eq=eqOf('song','chords'); eq.on=true; eq.bands=[{id:'t1',t:'bell',f:1000,g:12,q:1.4,s:12,on:true},{id:'t2',t:'highshelf',f:6000,g:-8,q:.7,s:12,on:true}]; eqLive('song','chords'); });
 await p.evaluate(()=>{ S.songAt={si:1,beat:0}; songPlay(); }); await p.waitForTimeout(3500);
 const m=await p.evaluate(()=>{ const D=EQ.cache['chords']; if(!D) return null; const cv=$('#eqCv'), W=cv.clientWidth, H=cv.clientHeight, i1=EQF120.findIndex(f=>f>=1000), sy=db=>eqY(db-EQ.ref,H); let s=0,n=0; const near=f=>Math.abs(Math.log2(f/1000))<.15; EQF120.forEach((f,i)=>{ if(near(f)){ s+=D.post[i]-D.pre[i]; n++; } }); const diff=n?s/n:0, resp=eqResponse(eqGet('song','chords').bands,new Float32Array([1000]))[0]; return {src:D.src,diffDb:+diff.toFixed(1),curveDb:+resp.toFixed(1),pxSpec:+(sy(D.pre[i1])-sy(D.post[i1])).toFixed(1),pxCurve:+(eqY(0,H)-eqY(resp,H)).toFixed(1),ref:EQ.ref}; });
 await p.evaluate(()=>stopPlay());
 ok(m&&m.src==='live'&&Math.abs(m.diffDb-m.curveDb)<2,'live: the spectrum after the EQ is 12 dB above before at 1 kHz, as the curve says '+JSON.stringify(m));
 ok(m&&Math.abs(m.pxSpec-m.pxCurve)<m.pxCurve*.2,'and it is drawn the same height as the curve (one dB scale) '+JSON.stringify(m));
 const sh=await p.evaluate(()=>{ const cv=$('#eqCv'), W=cv.clientWidth, H=cv.clientHeight, b=eqGet('song','chords').bands[1], [x,y]=eqBandXY(b,W,H), curve=eqResponse(eqGet('song','chords').bands,new Float32Array([b.f]))[0]; return {dotDb:+eqGy(y,H).toFixed(1),curveDb:+curve.toFixed(1)}; });
 ok(Math.abs(sh.dotDb-sh.curveDb)<1.5,'the shelf handle sits on the curve '+JSON.stringify(sh));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
