// Smart EQ, Calibrate, Fix what I hear, numbered channels and level zones (docs/design/mixing/README.md §1–3, docs/design/panels/README.md §3–4).
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; spPut({big:true,fold:false}); shellDock('mix'); spFit(); });
 // the brain is in the page
 const d=await p.evaluate(()=>{ const M=mxM(), ids=new Set((M.engineers||[]).map(e=>e.id)); return {n:(M.engineers||[]).length,probs:(M.problems||[]).length,bad:Object.entries(M.bestFor||{}).filter(([k,v])=>!ids.has(v.mix)||!ids.has(v.master)).map(x=>x[0]),low:(M.engineers||[]).filter(e=>e.c==='low').every(e=>/low confidence/.test(e.name))}; });
 ok(d.n===12&&d.probs===22,`mix-master.json is embedded: ${d.n} engineers, ${d.probs} problems`);
 ok(!d.bad.length&&d.low,'every engineer named in bestFor exists, and low-confidence profiles say so '+d.bad.join(','));
 const words=await p.evaluate(()=>(mxM().problems||[]).flatMap(pr=>(pr.words||[]).filter(w=>(mxM().problems||[]).filter(q=>(q.words||[]).includes(w)).length===1).map(w=>[pr.id,w,(fixMatch('it sounds '+w)||{}).p]).filter(x=>!x[2]||x[2].id!==x[0]).map(x=>x[0]+':'+x[1]+'→'+(x[2]&&x[2].id))));
 ok(!words.length,'every problem’s words map to it '+words.slice(0,4).join(' | '));
 // numbered channels
 const nums=await p.evaluate(()=>[...document.querySelectorAll('#cMix .strip[data-ch]')].map(s=>[s.dataset.ch,(s.querySelector('.st-num')||{}).textContent||'']));
 ok(nums.filter(x=>x[0]!=='master').every((x,i)=>x[1]===String(i+1).padStart(2,'0'))&&!nums.find(x=>x[0]==='master')[1],'strips are numbered 01, 02… in order; the master has none: '+nums.map(x=>x[1]).join(' '));
 const n2=await p.evaluate(()=>{ const was=H.past.length; strip('song','chords').num=9; renderStrips('song'); const nine=document.querySelector('#cMix .strip[data-ch="chords"] .st-num').textContent; chRenumber(); return {nine,after:document.querySelector('#cMix .strip[data-ch="chords"] .st-num').textContent,steps:H.past.length-was}; });
 ok(n2.nine==='09'&&n2.after==='02'&&n2.steps>=1,'numbers stay put until Renumber, which closes the gaps as an undo step '+JSON.stringify(n2));
 // level zones and the verdict line
 const lz=await p.evaluate(()=>{ const s=document.querySelector('#cMix .strip[data-ch="chords"]'), z=s.querySelector('.lz'); MT['song:chords']=MT['song:chords']||{p:[-100,-100],r:[-100,-100],h:[-100,-100],ht:[0,0],max:-100,clip:false}; LZ['song:chords']={avg:-30,n:40}; lzPaint(); const v=s.querySelector('.st-verdict'), t=v.textContent, vol=S.parts.chords.vol, was=H.past.length; v.click(); return {zone:!!z,range:lzRange('chords'),lead:lzRange('lead'),t,cls:v.className,vol0:vol,vol1:S.parts.chords.vol,steps:H.past.length-was}; });
 ok(lz.zone&&lz.lead[0]===-12&&lz.range[0]===-18,'each part strip shows its ideal zone from the role (melody −12…−6, chords −18…−12)');
 ok(/A little quiet: \+15 dB/.test(lz.t)&&lz.vol1>lz.vol0&&lz.steps===1,'the verdict says how far off and clicking it fixes the level in one undo step: '+lz.t);
 const clip=await p.evaluate(()=>{ MT['song:bass']={p:[0,0],r:[-3,-3],h:[0,0],ht:[0,0],max:2.2,clip:true}; lzPaint(); const s=document.querySelector('#cMix .strip[data-ch="bass"]'); return [s.querySelector('.st-verdict').textContent,s.classList.contains('lz-clip')]; });
 ok(/Clipping: lower 4 dB/.test(clip[0])&&clip[1],'a clipping part says “Clipping: lower …” and gets a red hairline: '+clip[0]);
 await p.evaluate(()=>{ delete MT['song:bass']; delete LZ['song:chords']; });
 // Smart EQ
 await p.evaluate(()=>{ renderStrips('song'); document.querySelector('#cMix .strip[data-ch="chords"] .eq-mini').click(); });
 ok(await p.evaluate(()=>!$('#eqPanel').hidden&&/Chords/.test($('#eqPanel .eq-head').textContent)),'the EQ curve on a strip opens the Smart EQ for that part');
 const cv=await p.evaluate(()=>{ const r=$('#eqCv').getBoundingClientRect(); return {x:r.left,y:r.top,w:r.width,h:r.height}; });
 const was=await p.evaluate(()=>H.past.length);
 await p.mouse.click(cv.x+cv.w*.5,cv.y+cv.h*.3); await p.waitForTimeout(60);
 const a1=await p.evaluate(()=>{ const B=S.mix.chords.eq.bands; return {n:B.length,b:B[0],sel:EQ.sel.size}; });
 ok(a1.n===1&&a1.b.t==='bell'&&a1.b.g>3&&a1.b.f>500&&a1.b.f<1500&&a1.sel===1,'clicking the curve adds a bell where you click '+JSON.stringify(a1.b));
 await p.evaluate(()=>{ const q=[...document.querySelectorAll('[data-eqkeep]')].find(x=>x.dataset.eqkeep==='all'); q.click(); });
 const a2=await p.evaluate(()=>({n:S.mix.chords.eq.bands.length,who:S.mix.chords.eq.bands.map(b=>b.who),sug:document.querySelectorAll('.eq-g').length,steps:H.past.length}));
 ok(a2.n>=3&&a2.who.includes('Smart'),'Keep all adds the suggested bands, marked Smart: '+a2.n+' bands');
 ok(a2.steps-was===2,'adding a band and keeping the suggestions are one undo step each');
 // multi-select relative moves keep spacing
 const mv=await p.evaluate(()=>{ const B=S.mix.chords.eq.bands.filter(b=>b.t==='bell'); EQ.sel=new Set(B.map(b=>b.id)); eqRender(); const f0=B.map(b=>b.f), g0=B.map(b=>b.g); document.querySelector('[data-eqrel="f:1"]').click(); document.querySelector('[data-eqrel="g:.5"]').click(); const B2=S.mix.chords.eq.bands.filter(b=>EQ.sel.has(b.id)); return {r0:f0[1]/f0[0],r1:B2[1].f/B2[0].f,dg:B2.map((b,i)=>+(b.g-g0[i]).toFixed(2)),panel:$('#eqPanel .eq-band b').textContent}; });
 ok(Math.abs(mv.r0-mv.r1)<.02&&mv.dg.every(x=>x===.5)&&/bands selected/.test(mv.panel),'several bands move together and keep their spacing '+JSON.stringify(mv));
 // the engine gets the same EQ
 const nat=await p.evaluate(()=>{ const x=nativeFx().find(f=>f.type==='peq'&&f.ch===nativeFxCh('chords')), B=S.mix.chords.eq.bands; return {x,n:B.length,f:B.map(b=>b.f)}; });
 ok(nat.x&&nat.x.p.n===nat.n&&nat.f.every((f,i)=>nat.x.p['f'+i]===f),'the plug-in engine receives the same bands (type, frequency, gain, Q, slope)');
 const resp=await p.evaluate(()=>{ const r=eqResponse([{t:'bell',f:1000,g:6,q:2}],new Float32Array([1000,100])); const c=eqResponse([{t:'lowcut',f:150,s:24}],new Float32Array([150,50])); return [r[0],r[1],c[0],c[1]]; });
 ok(Math.abs(resp[0]-6)<.05&&Math.abs(resp[1])<.1&&Math.abs(resp[2]+3)<.2&&resp[3]<-30,'the curve maths: a +6 dB bell, a 24 dB/oct low cut −3 dB at its frequency '+resp.map(x=>x.toFixed(1)).join(' '));
 // checks, I hear, lock, delete, Esc
 const ih=await p.evaluate(()=>{ document.querySelector('[data-eqhear="harsh"]').click(); const g=EQ.ghost[0]; return {g,shown:document.querySelector('.eq-g b').textContent}; });
 ok(ih.g&&ih.g.t==='bell'&&ih.g.g<0&&ih.g.f>=2500&&ih.g.f<=4500,'“I hear… Harsh” proposes a dip at 2.5–4.5 kHz: '+ih.shown);
 const lk=await p.evaluate(()=>{ const B=S.mix.chords.eq.bands, b=B[0]; EQ.sel=new Set([b.id]); eqRender(); document.querySelector('[data-eqx="lock"]').click(); const before=S.mix.chords.eq.bands.length; document.querySelector('[data-eqx="del"]').click(); return {locked:S.mix.chords.eq.bands[0].lock,kept:S.mix.chords.eq.bands.length===before}; });
 ok(lk.locked&&lk.kept,'a locked band can’t be deleted by accident');
 // A/B is level-matched and adds no history
 const ab=await p.evaluate(()=>{ const was=H.past.length; document.querySelector('[data-eqx="ab"]').click(); const on=EQ.bypass; histCheck(true); document.querySelector('[data-eqx="ab"]').click(); return {on,off:EQ.bypass,steps:H.past.length-was,avg:eqAvgGain(S.mix.chords.eq)}; });
 ok(ab.on&&!ab.off&&ab.steps===0,'A/B switches the EQ out and back without an undo step');
 const und=await p.evaluate(()=>{ histUndo(); const a=!S.mix.chords.eq.bands[0].lock&&!/🔒/.test(document.querySelector('#eqPanel .eq-card').textContent); const n=S.mix.chords.eq.bands.length; histUndo(); histUndo(); histUndo(); return [a,n,S.mix.chords.eq.bands.length,document.querySelectorAll('#eqPanel .eq-card').length]; });
 ok(und[0]&&und[2]<und[1]&&und[3]===und[2],'undo takes EQ changes back one at a time and the open panel follows '+JSON.stringify(und));
 await p.keyboard.press('Escape'); await p.keyboard.press('Escape'); ok(await p.evaluate(()=>$('#eqPanel').hidden),'Esc closes the Smart EQ');
 // Calibrate respects locks, is one undo step, and switching rows works
 await p.evaluate(()=>{ S.mix.chords.eq={on:true,bands:[{id:'lk',t:'bell',f:290,g:-1,q:1,on:true,lock:true,who:'You'}]}; S.mix.bass.eq={on:true,bands:[{id:'lk2',t:'bell',f:290,g:-1,q:1,on:true,lock:true,who:'You'}]}; histCheck(true); shellDock('cal'); });
 await p.evaluate(()=>calRun());
 const cal=await p.evaluate(()=>({plan:MX.plan.map(c=>({n:c.n,kind:c.kind,part:c.part,f:c.band&&c.band.f,g:c.band&&c.band.g,db:c.db})),rows:document.querySelectorAll('.cal-row').length,reads:document.querySelectorAll('.cal-read').length,locks:document.querySelector('.cal-r .qhint').textContent,steps:[...document.querySelectorAll('.cal-steps li')].length}));
 ok(cal.plan.length>0&&cal.rows===cal.plan.length&&cal.reads===4&&cal.steps===8,`Calibrate listens and lists ${cal.plan.length} changes with four readouts and eight steps`);
 ok(!cal.plan.some(c=>c.kind==='eq'&&(c.part==='chords'||c.part==='bass')&&c.f&&Math.abs(Math.log2(c.f/290))<.5)&&/Locked by you/.test(cal.locks),'Calibrate never touches locked bands and lists them');
 ok(cal.plan.every(c=>c.kind!=='eq'||Math.abs(c.g)<=6)&&cal.plan.every(c=>c.kind!=='vol'||Math.abs(c.db)<=6),'no change goes past ±6 dB in one pass');
 const ka=await p.evaluate(()=>{ const was=H.past.length, v0=JSON.stringify(S.parts), m0=JSON.stringify(histMix(S.mix)); document.querySelector('[data-calab="1"]').click(); const during=MX.ab, changed=JSON.stringify(histMix(S.mix))!==m0; histCheck(true); const mid=H.past.length; document.querySelector('[data-calab="0"]').click(); const back=JSON.stringify(S.parts)===v0&&JSON.stringify(histMix(S.mix))===m0;
   const off0=MX.off.size, first=document.querySelector('[data-calsw]:checked'); first.click(); const off=MX.off.size-off0; document.querySelector('[data-calkeep]').click(); return {during,changed,mid:mid-was,back,off,steps:H.past.length-was,label:H.past[H.past.length-1].label}; });
 ok(ka.during&&ka.changed&&ka.mid===0&&ka.back,'After plays the changes (level-matched) without touching history, Before puts everything back');
 ok(ka.off===1&&ka.steps===1&&/Calibrate: keep \d+ change/.test(ka.label),'switched-off rows are left out and Keep is one undo step: '+ka.label);
 await p.evaluate(()=>histUndo());
 // Fix what I hear: a synthetic muddy chords part is found, with three ways to fix it
 await p.evaluate(()=>{ S.mix.chords.eq={on:true,bands:[{id:'mud',t:'bell',f:350,g:14,q:1,on:true,who:'You'}]}; S.mix.bass.eq=null; histCheck(true); shellDock('fix'); });
 await p.evaluate(()=>fixRun('it sounds a bit muddy in the chorus, not sure where'));
 const fx=await p.evaluate(()=>{ const F=MX.fix.F; return {id:F.p.id,lo:F.lo,hi:F.hi,sis:F.sis.map(i=>S.song.sections[i].type),top:F.share.slice(0,2).map(x=>x[0]),share:JSON.stringify(F.share),gentle:MX.fix.ways[0].ch.map(c=>c.part),ways:MX.fix.ways.map(w=>w.h),what:(document.querySelector('.fix-what p')||{}).textContent,cv:!!$('#fixCv')}; });
 ok(fx.id==='muddy'&&fx.lo===250&&fx.hi===500&&fx.sis.length&&fx.sis.every(t=>t==='chorus'),'“muddy in the chorus” looks at 250–500 Hz in the choruses: '+fx.sis.join(','));
 ok(fx.top.includes('chords')&&fx.gentle.includes('chords'),'the boosted chords are found and the gentle fix dips them '+fx.share);
 ok(fx.ways.length>=2&&fx.ways[0]==='Gentle'&&fx.cv&&fx.what.length>40,'it explains what’s happening and offers ways to fix it: '+fx.ways.join(' · '));
 const fk=await p.evaluate(()=>{ const was=H.past.length, n=S.mix.chords.eq.bands.length; document.querySelector('[data-fixkeep="0"]').click(); return {steps:H.past.length-was,more:S.mix.chords.eq.bands.length>n}; });
 ok(fk.steps===1&&fk.more,'keeping a fix is one undo step and lands as a real EQ band');
 const ns=await p.evaluate(async()=>{ await fixRun('the stereo is way too wide'); return (MX.fix.F||{}).none; }); ok(ns===true,'problems the page can’t measure yet say so and give the advice from the data');
 // the master bus has its own Smart EQ too: curve on the strip, heard first on the master, sent to the plug-in on the master channel
 await p.evaluate(()=>{ eqClose&&eqClose(); shellDock('mix'); renderStrips('song'); const b=document.querySelector('#cMix .strip[data-ch="master"] .eq-mini'); if(b) b.click(); });
 ok(await p.evaluate(()=>!$('#eqPanel').hidden&&/Master/.test($('#eqPanel .eq-head').textContent)),'the master strip has an EQ curve that opens the Smart EQ for the master bus');
 const mcv=await p.evaluate(()=>{ const r=$('#eqCv').getBoundingClientRect(); return {x:r.left,y:r.top,w:r.width,h:r.height}; });
 await p.mouse.click(mcv.x+mcv.w*.6,mcv.y+mcv.h*.3); await p.waitForTimeout(60);
 const meq=await p.evaluate(()=>{ ensureAudio(); const m=newMaster('song'), n=(S.mix.master.eq||{bands:[]}).bands.length, nat=eqNative().filter(x=>x.ch===16&&x.type==='peq'); const r={n,inAudio:!!(m.mbus&&m.mbus.eq&&m.mbus.eq.L.length===n),nat:nat.length,natN:nat[0]&&nat[0].p.n}; try{ m.disconnect(); }catch(e){} return r; });
 ok(meq.n>=1&&meq.inAudio&&meq.nat===1&&meq.natN===meq.n,'a band on the master EQ is in the master sound and goes to the plug-in on the master channel '+JSON.stringify(meq));
 await p.evaluate(()=>{ shellDock('mix'); renderStrips('song'); }); await p.waitForTimeout(100); await p.screenshot({path:'mixing.png'});
 ok(!errs.length,'no page errors '+errs.slice(0,3).join(' | '));
 await b.close(); if(fails.length){ console.log(fails.length+' failed'); process.exit(1); } })();
