// Smart knob, the per-part Smart compressor and bypass keys (AGENTS.md step 18; docs/design/panels/README.md strip layout, docs/design/mixing/README.md §4, §4c, §5).
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; spPut({big:true,fold:false}); shellDock('mix'); spFit(); });
 // smart knob: bass never gets reverb or echo unless a sourced style exception allows it
 const bass=await p.evaluate(()=>{ const SK=mxM().smartKnob, prods=['',...Object.keys(SK.producer||{})], styles=[...new Set([...(SK.exceptions||[]).flatMap(x=>(x.when||{}).styles||[]),'pop','rock','lofi','house',S.style&&S.style.id].filter(Boolean))], bad=[], allowed=[]; const keep={st:S.style,rec:S.recipe,tags:S.tags};
   prods.forEach(pr=>styles.forEach(id=>{ S.recipe={...(keep.rec||{}),producer:pr}; S.tags=[]; S.style={...(keep.st||{}),id}; const k=knSmart('bass'); if(KN_SEND.includes(k.pick)) bad.push(pr+'/'+id+':'+k.pick); k.useful.filter(o=>KN_SEND.includes(o)).forEach(o=>{ const src=k.ex.find(x=>(x.allow||[]).includes(o)&&(x.src||[]).length); if(src) allowed.push(id+':'+o); else bad.push(pr+'/'+id+' useful '+o); }); }));
   S.style=keep.st; S.recipe=keep.rec; S.tags=keep.tags; return {bad,allowed:allowed.length}; });
 ok(!bass.bad.length&&bass.allowed>0,`bass never gets a reverb or echo pick; a sourced film/orchestral exception may offer one (${bass.allowed} cases) `+bass.bad.slice(0,4).join(' '));
 const kp=await p.evaluate(()=>[...document.querySelectorAll('#cMix .strip[data-ch]')].map(s=>[s.dataset.ch,!!s.querySelector('.st-kp'),!!s.querySelector('.pcb'),(s.querySelector('.kp-lab')||{}).textContent||'']));
 ok(kp.filter(x=>['lead','chords','bass','drums'].includes(x[0])).every(x=>x[1]&&x[2]&&/✦ .+ ▾/.test(x[3]))&&!kp.find(x=>x[0]==='master')[2],'every part strip has Width, a smart knob labelled “✦ Effect ▾” and a Comp bar; the master has none: '+kp.map(x=>x[0]+':'+x[3]).join(' '));
 const km=await p.evaluate(async()=>{ const was=H.past.length; document.querySelector('#cMix .strip[data-ch="bass"] .kp-lab').click(); await new Promise(r=>setTimeout(r,30)); const m=$('#knMenu'), txt=m.textContent; m.querySelector('[data-knpick="tone"]').click(); await new Promise(r=>setTimeout(r,30)); const after=knOf('bass'); const back=[...$('#knMenu').querySelectorAll('button')].find(x=>/Back to smart/.test(x.textContent)); const lab1=document.querySelector('#cMix .strip[data-ch="bass"] .kp-lab').textContent; if(back) back.click(); await new Promise(r=>setTimeout(r,30)); return {txt:/this knob controls/.test(txt)&&/Rare on/.test(txt),manual:after.manual&&after.id==='tone',lab1,smart:knOf('bass').id,steps:H.past.length-was}; });
 ok(km.txt&&km.manual&&!/✦/.test(km.lab1)&&km.smart==='drive'&&km.steps===2,'“this knob controls” lists useful and rare choices; a choice sticks, Back to smart returns it, one undo step each '+JSON.stringify(km));
 // 11 Nov: compressors start at 0 and off (Jamie); the checks below turn two up first, as a user would
 const off0=await p.evaluate(()=>[pcOf('lead').on,pcOf('lead').amt,pcActive('lead')]); ok(off0[0]===false&&off0[1]===0&&!off0[2],'a strip compressor starts at 0 and off (a blank start has none on) '+JSON.stringify(off0));
 await p.evaluate(()=>{ pcSet('lead',c=>{ c.amt=.4; },'Comp',true); pcSet('chords',c=>{ c.amt=.4; },'Comp',true); renderStrips('song'); });
 const on1=await p.evaluate(()=>pcOf('lead').on); ok(on1,'turning the amount up switches it on');
 // the design's knob column: Pan, Width, the smart knob, then the Comp bar between its rails, beside the fader and the meters; reduction fills the bar from the top
 const gm=await p.evaluate(()=>{ const s=document.querySelector('#cMix .strip[data-ch="lead"]'), g=s.querySelector('.st-kp .pcb [data-pcgr="lead"]'), fz=s.querySelector('.st-fz'), pc=s.querySelector('.st-kp .pcb'); if(!g||!fz||!pc) return {g:!!g}; const a=s.querySelector('.st-kp').getBoundingClientRect(), b=fz.getBoundingClientRect(), order=[...s.querySelectorAll('.st-kp .sk b, .st-kp .sk .kp-lab, .st-kp .pcb-h b')].map(x=>x.textContent.replace(/[✦▾ ]/g,'')); PCX.live['song:lead']={gr:6,t:performance.now()}; const pl=S.playing; S.playing=true; pcPaint(); const h=g.querySelector('i').style.height, db=s.querySelector('.pcb-db').textContent; S.playing=pl; delete PCX.live['song:lead']; pcPaint(); return {g:true,inBar:!!s.querySelector('.pcb-bar .pcb-gr'),order,gap:Math.round(b.left-a.right),sameH:Math.abs(a.height-b.height)<2,h,db}; });
 ok(gm.g&&gm.inBar&&/^Pan,Width,.+,Comp$/.test(gm.order.join())&&gm.gap>=0&&gm.gap<=8&&gm.sameH&&gm.h==='50%'&&/6\.0 dB/.test(gm.db),'the knob column (Pan, Width, smart knob, Comp) sits right beside the fader and meters, the same height, and the Comp bar fills from the top as it turns down '+JSON.stringify(gm));
 // the Comp bar and the window stay in sync
 await p.evaluate(()=>{ ensureAudio(); return pcLoad(ac); });
 const bar=await p.evaluate(()=>{ const r=document.querySelector('#cMix .pcb-bar[data-pcbar="lead"]').getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2,h:r.height,amt:pcOf('lead').amt,smart:pcOf('lead').smart.amt}; });
 const was=await p.evaluate(()=>H.past.length);
 await p.mouse.move(bar.x,bar.y); await p.mouse.down(); await p.mouse.move(bar.x,bar.y-30,{steps:4}); await p.mouse.up(); await p.waitForTimeout(60);
 const d1=await p.evaluate(w=>({amt:pcOf('lead').amt,steps:H.past.length-w}),was);
 ok(d1.amt>bar.amt&&d1.steps===1,`dragging the Comp bar up adds more (${bar.amt} → ${d1.amt}) in one undo step`);
 await p.mouse.dblclick(bar.x,bar.y); await p.waitForTimeout(80);
 const w1=await p.evaluate(()=>({open:!$('#pcWin').hidden,ch:PCX.win&&PCX.win.ch,amt:+$('#pcWin .pcw-amt b').textContent,tabs:[...$('#pcWin .pcw-tabs').children].map(x=>x.textContent.replace(' ✦','')).join(','),pill:$('#pcWin .pcw-pill').textContent}));
 ok(w1.open&&w1.ch==='lead'&&w1.amt===Math.round(d1.amt*100)&&w1.tabs==='Clean,Punch,Fast,Smooth,Glue'&&/On/.test(w1.pill),'double-clicking the bar opens the Smart compressor with the same amount and five characters '+JSON.stringify(w1));
 const w2=await p.evaluate(()=>{ const r=$('#pcWin .pcw-amt').getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2}; });
 await p.mouse.move(w2.x,w2.y); await p.mouse.down(); await p.mouse.move(w2.x,w2.y+40,{steps:4}); await p.mouse.up(); await p.waitForTimeout(60);
 const s2=await p.evaluate(()=>({amt:pcOf('lead').amt,bar:+document.querySelector('#cMix .pcb-bar[data-pcbar="lead"]').getAttribute('aria-valuenow')}));
 ok(s2.amt<d1.amt&&s2.bar===Math.round(s2.amt*100),'turning the Amount knob in the window moves the strip bar too '+JSON.stringify(s2));
 await p.keyboard.down('Alt'); await p.mouse.click(w2.x,w2.y); await p.keyboard.up('Alt'); await p.waitForTimeout(60);
 ok(await p.evaluate(sm=>Math.abs(pcOf('lead').amt-sm)<.001,bar.smart),'⌥-click resets the amount to the smart amount');
 const prm=await p.evaluate(async()=>{ const el=$('#pcWin [data-pcp="thr"]'); el.scrollIntoView({block:'center'}); const r=el.getBoundingClientRect(); return {x:r.left+20,y:r.top+12}; });
 await p.mouse.move(prm.x,prm.y); await p.mouse.down(); await p.mouse.move(prm.x,prm.y-24,{steps:3}); await p.mouse.up(); await p.waitForTimeout(40);
 const t1=await p.evaluate(()=>({mine:pcOf('lead').mine.thr,mark:/✦/.test($('#pcWin [data-pcp="thr"] small').textContent)}));
 await p.mouse.dblclick(prm.x,prm.y); await p.waitForTimeout(40);
 const t2=await p.evaluate(()=>({mine:pcOf('lead').mine.thr,mark:/✦/.test($('#pcWin [data-pcp="thr"] small').textContent)}));
 ok(t1.mine!=null&&!t1.mark&&t2.mine==null&&t2.mark,'touching Threshold makes it yours (✦ goes); double-click gives it back to Smart '+JSON.stringify([t1,t2]));
 await p.evaluate(()=>{ $('#pcWin [data-pcc="punch"]').click(); });
 ok(await p.evaluate(()=>pcOf('lead').c==='punch'&&$('#pcWin [data-pcc="punch"]').getAttribute('aria-selected')==='true'),'choosing a character changes the compressor');
 // B in the open window bypasses; the engine gets it too
 const nb=await p.evaluate(()=>H.past.length); await p.keyboard.press('b'); await p.waitForTimeout(40);
 const by=await p.evaluate(nb=>({on:pcOf('lead').on,pill:$('#pcWin .pcw-pill').textContent,status:($('#pcWin [role="status"]')||{}).textContent||'',steps:H.past.length-nb,bar:document.querySelector('#cMix .pcb[data-pcb="lead"]').classList.contains('off'),nat:pcNative().some(x=>x.type==='pcomp'&&x.ch===nativeFxCh('lead')&&x.p.on===0)}),nb);
 ok(!by.on&&/Bypassed/.test(by.pill)&&/at the same loudness/.test(by.status)&&by.steps===1&&by.bar&&by.nat,'B bypasses the open compressor: pill, status line, the strip turns red, one undo step, the plug-in gets a level-matched bypass too '+JSON.stringify(by));
 await p.keyboard.press('b'); await p.keyboard.press('Escape'); await p.waitForTimeout(40);
 ok(await p.evaluate(()=>pcOf('lead').on&&$('#pcWin').hidden),'B again turns it back on; Escape closes the window');
 // ⇧B on a part toggles everything on it; ⌥B the master
 const sb=await p.evaluate(async()=>{ document.querySelector('#cMix .pcb-bar[data-pcbar="chords"]').focus(); return 1; });
 await p.keyboard.press('Shift+B'); await p.waitForTimeout(40);
 const sb1=await p.evaluate(()=>({comp:pcOf('chords').on,fx:strip('song','chords').fx.filter(Boolean).map(f=>f.on)}));
 await p.keyboard.press('Shift+B'); await p.waitForTimeout(40);
 const sb2=await p.evaluate(()=>({comp:pcOf('chords').on,fx:strip('song','chords').fx.filter(Boolean).map(f=>f.on)}));
 ok(!sb1.comp&&sb1.fx.every(x=>!x)&&sb2.comp&&sb2.fx.every(x=>x),'⇧B bypasses every effect on the part and brings them all back '+JSON.stringify([sb1,sb2]));
 await p.evaluate(()=>document.activeElement.blur()); await p.keyboard.press('Alt+b'); await p.waitForTimeout(40);
 const ab=await p.evaluate(()=>strip('song','master').fx.filter(Boolean).map(f=>f.on)); await p.keyboard.press('Alt+b');
 ok(ab.length&&ab.every(x=>!x)&&await p.evaluate(()=>strip('song','master').fx.filter(Boolean).every(f=>f.on)),'⌥B bypasses everything on the master and back');
 // the sound: level-matched bypass with no click, and the compressor really turns down peaks
 const snd=await p.evaluate(async()=>{ const sr=48000, N=sr*1.2, mk=async(on,flip)=>{ const off=new OfflineAudioContext(2,N,sr); await pcLoad(off); const bf=off.createBuffer(1,N,sr), d=bf.getChannelData(0); for(let i=0;i<N;i++){ const a=(i%12000)<3000?.9:.12; d[i]=a*Math.sin(2*Math.PI*180*i/sr); } const s=off.createBufferSource(); s.buffer=bf; const n=new AudioWorkletNode(off,'semitone-pcomp',{numberOfInputs:1,numberOfOutputs:1,outputChannelCount:[2],processorOptions:{p:{amt:.6,c:1,bpm:120},on}}); s.connect(n); n.connect(off.destination); s.start(); if(flip) off.suspend(.6).then(()=>{ n.port.postMessage({on:!on}); off.resume(); }); return (await off.startRendering()).getChannelData(0); };
   const rms=(x,a,b)=>{ let s=0; for(let i=a;i<b;i++) s+=x[i]*x[i]; return Math.sqrt(s/(b-a)); }, on=await mk(true), byp=await mk(false), sw=await mk(true,true), loud=(x)=>20*Math.log10(rms(x,sr*.6,N)); let jump=0; for(let i=sr*.6-200;i<sr*.6+2000;i++) jump=Math.max(jump,Math.abs(sw[i]-sw[i-1]));
   const seg=(x,loud)=>{ let s=0,n=0; for(let i=sr*.6;i<N;i++){ const ph=i%12000; if(loud?(ph>960&&ph<3000):(ph>4000)){ s+=x[i]*x[i]; n++; } } return Math.sqrt(s/n); }, rng=x=>+(20*Math.log10(seg(x,1)/seg(x,0))).toFixed(1);
   return {diff:+(loud(byp)-loud(on)).toFixed(2),jump:+jump.toFixed(3),crest:rng(on),crestIn:rng(byp)}; });
 ok(Math.abs(snd.diff)<.5,`bypass is level-matched: ${snd.diff} dB between on and bypassed (within 0.5)`);
 ok(snd.jump<.08,`switching bypass makes no click (largest sample step ${snd.jump})`);
 ok(snd.crest<snd.crestIn,`the compressor evens the level: loud-to-quiet ${snd.crestIn} dB → ${snd.crest} dB`);
 // offline renders include the compressor
 const ren=await p.evaluate(async()=>{ const off=new OfflineAudioContext(2,4800,48000); await pcLoad(off); return !!off.__pc; });
 ok(ren,'offline renders (bounce, stems) load the compressor');
 // ⌥B on a Mac types ∫: the key's place counts. It also bypasses the master's Smart EQ
 const ab2=await p.evaluate(()=>{ const st=strip('song','master'); st.eq={on:true,bands:[{id:1,t:'bell',f:3000,g:2,q:1}]}; document.activeElement.blur(); const k=o=>document.dispatchEvent(new KeyboardEvent('keydown',{bubbles:true,...o})); k({key:'∫',code:'KeyB',altKey:true}); const r={eq:st.eq.on,fx:st.fx.filter(Boolean).map(f=>f.on)}; k({key:'∫',code:'KeyB',altKey:true}); r.back=st.eq.on; delete st.eq; return r; });
 ok(ab2.eq===false&&ab2.fx.every(x=>!x)&&ab2.back===true,'⌥B works on a Mac keyboard and bypasses the master EQ too '+JSON.stringify(ab2));
 // ⇧B takes the smart knob's effect with it
 const kb=await p.evaluate(()=>{ const st=strip('song','chords'); st.knob={amt:.6}; renderStrips('song'); const before=!!knNative('chords'); document.querySelector('#cMix .pcb-bar[data-pcbar="chords"]').focus(); document.dispatchEvent(new KeyboardEvent('keydown',{bubbles:true,key:'B',code:'KeyB',shiftKey:true})); const off=!!knNative('chords'); document.querySelector('#cMix .pcb-bar[data-pcbar="chords"]').focus(); document.dispatchEvent(new KeyboardEvent('keydown',{bubbles:true,key:'B',code:'KeyB',shiftKey:true})); return {before,off,back:!!knNative('chords')}; });
 ok(kb.before&&!kb.off&&kb.back,'⇧B bypasses the smart knob’s effect with the rest, in the page and the plug-in '+JSON.stringify(kb));
 // Duck from kick dips 3 dB at full, as the data says
 ok(await p.evaluate(()=>{ const st={}; knAmt(st,'bass','duck',1); return Math.abs(20*Math.log10(1-st.duck.depth)+3.1)<.2; }),'the smart knob’s Duck from kick dips about 3 dB at full');
 // a click on the Comp bar that doesn't move it leaves the smart setting smart, with no hidden change
 const ck=await p.evaluate(()=>{ delete strip('song','bass').comp; renderStrips('song'); const r=document.querySelector('#cMix .pcb-bar[data-pcbar="bass"]').getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2,n:H.past.length}; });
 await p.mouse.click(ck.x,ck.y); await p.waitForTimeout(60);
 ok(await p.evaluate(n=>!strip('song','bass').comp&&H.past.length===n,ck.n),'clicking the Comp bar without moving it changes nothing');
 // Width keeps a one-channel part in the middle at the same loudness
 const wd=await p.evaluate(async()=>{ ensureAudio(); const real=ac, out={}; for(const w of [1.0001,2]){ const off=new OfflineAudioContext(2,4800,48000); ac=off; try{ const c=off.createConstantSource(); c.offset.value=.5; const mn=G(1); monoSet(mn,true); c.connect(mn); const wn=widthNode(w,mn), pn=off.createStereoPanner(); wn.connect(pn); pn.connect(off.destination); c.start(); const b=await off.startRendering(); out[w>1.5?'wide':'asis']=[b.getChannelData(0)[4000],b.getChannelData(1)[4000]]; } finally { ac=real; } } return out; });
 ok(Math.abs(wd.wide[0]-wd.wide[1])<1e-4&&Math.abs(wd.wide[0]-wd.asis[0])<1e-3,'Width keeps a mono part centred and at the same level '+JSON.stringify(wd));
 // old compressors stop when the song plays again
 const rp=await p.evaluate(async()=>{ ensureAudio(); await pcLoad(ac); for(let i=0;i<3;i++){ play('song'); await new Promise(r=>setTimeout(r,300)); stopPlay(); } return {alive:PCX.nodes.length,mine:new Set(PCX.nodes.map(x=>x.m)).size===1}; });
 ok(rp.mine&&rp.alive<=8,'compressors from earlier plays are stopped; only the current mix’s run '+JSON.stringify(rp));
 // Knows your song: switches in the compressor, each one undo step, and the sentence says what they do
 const kn=await p.evaluate(async()=>{ pcOpen('lead'); await new Promise(r=>setTimeout(r,100)); const sw=[...document.querySelectorAll('#pcWin .pcw-sw')].map(b=>[b.dataset.pcsong,b.getAttribute('aria-checked'),b.disabled]); const n=H.past.length, before=pcSays('lead'); document.querySelector('#pcWin .pcw-sw[data-pcsong="follow"]').click(); await new Promise(r=>setTimeout(r,60)); const r={sw,steps:H.past.length-n,follow:pcSong('lead').follow,before,after:pcSays('lead'),room:document.querySelectorAll('#pcWin .pcw-sw[data-pcsong="room"]').length,more:(()=>{ document.querySelector('#pcWin [data-pcw="more"]').click(); return !!document.querySelector('#pcWin .pcw-pro'); })(),ws:document.querySelector('#pcWin').parentNode.classList.contains('eq-ws')}; pcClose(); return r; });
 ok(kn.sw.length>=4&&kn.steps===1&&kn.follow===false&&/chorus/.test(kn.before)&&!/chorus/.test(kn.after)&&kn.room===0&&kn.more&&kn.ws,'Knows your song: switches are one undo step each, the sentence follows them, Makes room is hidden until it does something, More opens the pro view, and it fills the Mix pane '+JSON.stringify(kn));
 ok(!errs.length,'no page errors '+errs.join(' | '));
 await b.close(); if(fails.length) process.exit(1); })();
