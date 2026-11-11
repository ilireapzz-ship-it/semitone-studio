// 12 Nov: an echo's repeats must always die away. Measured before the fix: at 92% feedback the Digital and Ping-pong echoes grew by
// about 4 dB a second without end (+150 dB after 40 s), and the Tape and Analog tails never ended above about 55%.
// Settings that already worked (the 40% default) are untouched.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1200,height:800}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(async()=>{ const out={}; ensureAudio(); const real=ac;
   for(const [style,fb] of [['Digital',.92],['Ping-pong',.92],['Tape',.92],['Analog',.92],['Tape',.6],['Tape',.4],['Digital',.4]]){
     const sr=44100, secs=32, off=new OfflineAudioContext(2,sr*secs,sr); ac=off; let lv;
     try{ const fx=FX.echo.build({style,time:'1/8',fb,tone:3500,wob:.3,send:1,mix:1},{bpm:120,cl:[],t0:0});
       const src=off.createBufferSource(), buf=off.createBuffer(1,sr*.25,sr), d=buf.getChannelData(0); let s=7; for(let i=0;i<d.length;i++){ s=(s*16807)%2147483647; d[i]=(s/2147483647*2-1)*.5*(1-i/d.length); } src.buffer=buf; src.connect(fx.input); fx.output.connect(off.destination); src.start(0);
       const R=await off.startRendering(), L=R.getChannelData(0), Rr=R.getChannelData(1); lv=[2,31].map(t=>{ let q=0; for(let i=Math.floor((t-1)*sr);i<t*sr;i++) q+=L[i]*L[i]+Rr[i]*Rr[i]; return +(10*Math.log10(q/sr/2+1e-15)).toFixed(1); });
     }catch(e){ lv='err '+e.message; } ac=real; out[style+' '+Math.round(fb*100)+'%']=lv; }
   out.same=[echoFb(.4,'Tape'),echoFb(.4,'Digital'),echoFb(.7,'Digital'),echoFb(.92,'Digital'),echoFb(.92,'Tape'),echoFb(.6,'Tape')]; return out; });
 console.log(JSON.stringify(r));
 for(const k of ['Digital 92%','Ping-pong 92%','Tape 92%','Analog 92%','Tape 60%']){ const v=r[k]; ok(Array.isArray(v)&&v[1]<v[0]-20,k+': the tail is dying away after 30 seconds ('+(Array.isArray(v)?v[0]+' dB → '+v[1]+' dB':v)+')'); }
 ok(Array.isArray(r['Tape 40%'])&&r['Tape 40%'][1]<-100,'the default Tape echo is silent long before 30 s');
 ok(r.same[0]===.4&&r.same[1]===.4&&r.same[2]===.7,'feedback that already worked is left exactly as set (40% Tape, 40% and 70% Digital)');
 ok(r.same[3]<.92&&r.same[3]>.7&&r.same[4]<r.same[3]&&r.same[5]<.6,'only the top of the knob is held back ('+r.same.slice(3).map(x=>x.toFixed(3)).join(', ')+')');
 ok(r.same.every((x,i)=>x*(i%2||i===0&&0?1:1)*([2.18,1.26,1.26,1.26,2.18,2.18][i])<.981),'the loop never reaches 1');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
