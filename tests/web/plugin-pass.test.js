// Producer passes and your plug-ins: moves find controls by name, never swap a plug-in out, and every control goes back afterwards.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
const synth=(on=0)=>[{i:1,name:'Filter 1 On',v:on},{i:2,name:'Filter 1 Freq',v:.5},{i:3,name:'Filter 1 Drive',v:0},{i:4,name:'Main Vol',v:.5},{i:5,name:'LFO 1 Delay',v:0}];
const fx=[{i:1,name:'Reverb: On/Off',v:1},{i:2,name:'Reverb: Dry/Wet',v:.5},{i:3,name:'Delay: On/Off',v:0},{i:4,name:'Delay: Delay Time',v:.2},{i:5,name:'Delay: Dry/Wet',v:.5},{i:6,name:'Master Volume',v:.8},{i:7,name:'Input Volume',v:.8},{i:8,name:'Filter Bypass',v:0},{i:9,name:'Filter Cutoff BL',v:.5},{i:10,name:'Overdrive: On/Off',v:1},{i:11,name:'Overdrive: Drive',v:.5},{i:12,name:'Keyb 1 Stop 2 Reverb Mix',v:.5}];
const plain=[{i:0,name:'Lowpass Cutoff',v:1},{i:1,name:'Highpass Cutoff',v:0},{i:2,name:'Filter Cutoff Spread',v:.1},{i:3,name:'Distortion',v:0},{i:4,name:'Master Gain',v:.78},{i:5,name:'reverb mix',v:.13},{i:6,name:'delay mix',v:.19},{i:7,name:'reverb low-cut',v:.1}];
(async()=>{ const b=await chromium.launch(); const errs=[], fails=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const p=await b.newPage({viewport:{width:1300,height:900}}); p.on('pageerror',e=>errs.push(e.message+' '+(e.stack||'').split('\n')[1]));
 await p.addInitScript(()=>{ window.__noStart=1; window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); });
 await p.goto(url); await p.waitForTimeout(1200); await p.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:'',recent:[]})); await p.waitForTimeout(600);
 await p.evaluate(()=>{ const e=$('#structSel'); e.value='pop'; e.dispatchEvent(new Event('change',{bubbles:true})); }); await p.waitForTimeout(1900);
 const put=(part,list,name='Test Synth')=>p.evaluate(([part,list,name])=>{ S.parts[part].sound='h:Test-1'; const ch=hostCh(part); HOSTS.on[ch]='Test-1'; HOSTS.names[ch]=name; HOSTS.params[ch]=list; },[part,list,name]);

 // finding controls by name
 const f=await p.evaluate(([a,c,d])=>{ const n=(L,i)=>{ const r=plugFind(L,i); return r?r.q.name+(r.off?' (off)':''):null; }; return {vol:n(a,'volume'),fil:n(a,'filter'),filOn:n(a.map(x=>x.i===1?{...x,v:1}:x),'filter'),del:n(a,'delay'),v2:n(c,'volume'),rev:n(c,'reverb'),del2:n(c,'delay'),fil2:n(c,'filter'),dr:n(c,'drive'),rev3:n(d,'reverb'),fil3:n(d,'filter'),dr3:n(d,'drive'),v3:n(d,'volume')}; },[synth(0),fx,plain]);
 console.log(JSON.stringify(f));
 ok(f.vol==='Main Vol'&&f.v2==='Master Volume'&&f.v3==='Master Gain','volume finds the main control, not the input or a part of it');
 ok(f.fil==='Filter 1 Freq (off)'&&f.filOn==='Filter 1 Freq','a filter that is switched off is noticed'); ok(f.del===null,'an LFO delay is not mistaken for an echo');
 ok(f.rev==='Reverb: Dry/Wet','reverb finds the amount, not an organ stop'); ok(f.del2==='Delay: Dry/Wet (off)','a delay switched off is noticed'); ok(f.fil2==='Filter Cutoff BL','the filter with a bypass reads as on'); ok(f.dr==='Overdrive: Drive','drive found');
 ok(f.rev3==='reverb mix'&&f.fil3==='Lowpass Cutoff'&&f.dr3==='Distortion','other naming styles work');

 // without a plug-in, nothing changes
 const none=await p.evaluate(()=>{ passOpen('nigro'); const a=passMoves().some(m=>m.id==='plug-climb'); passClose(); return a; }); ok(!none,'plug-in moves stay hidden when no plug-in is on the part');

 // a plug-in is never swapped out
 await put('chords',synth(1)); await put('bass',plain,'Bass Synth');
 const keep=await p.evaluate(()=>{ passOpen('rubin'); PASS.level=2; PASS.on['acoustic-version']=true; passSwitch('B',true); const m=passMoves().find(x=>x.id==='acoustic-version'), s=S.parts.chords.sound, html=$('#passView').innerText; passPick(TL.d.producers.find(q=>q.id==='antonoff')); PASS.level=2; passSwitch('B',true); const s2=[S.parts.chords.sound,S.parts.bass.sound,S.parts.lead.sound]; passPick(TL.d.producers.find(q=>q.id==='mikedean')); PASS.level=2; passSwitch('B',true); const t=passMoves().find(x=>x.id==='throb-bass'); const r={kept:m.kept.length,live:m.fn.length,s,s2,bass:S.parts.bass.sound,throb:t.plug[0]&&t.plug[0].text,note:/stays on chords/.test(html)}; passDiscard(); return r; });
 console.log(JSON.stringify(keep));
 ok(keep.s==='h:Test-1'&&keep.kept===1&&keep.live>0&&keep.note,'Rick Rubin keeps your plug-in on chords and says so, and still does the rest of the move');
 ok(keep.s2[0]==='h:Test-1'&&keep.s2[1]==='h:Test-1'&&/^i:/.test(keep.s2[2]),'the sound-family move only changes parts without a plug-in');
 ok(keep.bass==='h:Test-1'&&/Lowpass Cutoff throbs/.test(keep.throb),'Mike Dean throbs your bass filter instead of replacing it');

 // producers use the same control differently
 const look=await p.evaluate(()=>{ const out={}; TL.d.producers.forEach(q=>{ passOpen(q.id); PASS.level=2; PASS.q.pass.moves.forEach(m=>{ if(/plug:/.test(m.op)) PASS.on[m.id]=true; }); passSwitch('B',true); out[q.id]=passMoves().flatMap(m=>(m.plug||[]).filter(x=>x.fn).map(x=>x.text)); passDiscard(); }); return out; });
 console.log(JSON.stringify(look,null,0).slice(0,900));
 ok(Object.values(look).every(x=>x.length>0),'every producer has something to do with a plug-in'); ok(new Set(Object.values(look).map(x=>x.sort().join('|'))).size===8,'no two producers do the same thing');

 // A/B reaches the engine, and every control comes back
 await put('lead',plain,'Lead Synth');
 const sent=async()=>{ await p.waitForTimeout(900); return p.evaluate(()=>{ const t=__nlog.filter(x=>x[0]==='st_timeline').pop(); return t?(t[1].sound||{}).plugLanes:null; }); };
 await p.evaluate(()=>{ __nlog.length=0; passOpen('antonoff'); }); let B=await sent();
 ok(B&&B.length>=1,'the pass sends its lanes to the engine'); 
 ok(B.every(l=>l.pts.every((v,i)=>i%2||i<2||v>=l.pts[i-2])),'lane points always run forward, across joined sections');
 await p.evaluate(()=>{ __nlog.length=0; passSwitch('A',true); }); let A=await sent();
 ok(A&&A.length===B.length&&A.every(l=>new Set(l.pts.filter((_,i)=>i%2)).size===1),'on your version, every control sits still at its resting value');
 const back=await p.evaluate(()=>{ passSwitch('B',true); const d=JSON.stringify(S.auto); passDiscard(); return {had:/plug\./.test(d),now:/plug\./.test(JSON.stringify(S.auto))}; }); ok(back.had&&!back.now,'Discard removes the pass’s lanes');

 // your own lane stays, keeping is one undo per move, lengths match
 const own=await p.evaluate(()=>{ const si=projectData().sections.findIndex(x=>x[0]==='chorus'); console.log;  S.auto[si]=S.auto[si]||{}; S.auto[si].chords={'plug.4':[{t:0,v:.123},{t:4,v:.123}]}; histCheck(true); passOpen('antonoff'); const line=passMoves().find(m=>m.id==='pulse-pads').plug.map(x=>x.text).join(' '); const v=S.auto[si].chords['plug.4'][0].v; passDiscard(); delete S.auto[si].chords; histCheck(true); return {line,v}; });
 console.log(JSON.stringify(own)); ok(own.v===.123&&/your own lane stays/.test(own.line),'a lane you drew is left alone');
 const k=await p.evaluate(()=>{ const n0=H.past.length; passOpen('finneas'); PASS.level=0; PASS.q.pass.moves.forEach(m=>{ PASS.on[m.id]=m.id==='little-reverb'; }); passSwitch('B',true); passKeep(); const kept=/plug\./.test(JSON.stringify(S.auto)), n1=H.past.length; histUndo(); return {kept,steps:n1-n0,after:/plug\./.test(JSON.stringify(S.auto))}; }).catch(e=>({err:e.message}));
 console.log(JSON.stringify(k)); ok(k.kept&&k.steps===1&&!k.after,'keeping a plug-in move is one undo step');
 const len=await p.evaluate(()=>{ passOpen('maxmartin'); PASS.level=1; PASS.q.pass.moves.forEach(m=>{ PASS.on[m.id]=true; }); passSwitch('B',true); const B0=METERS[S.meter].B, bad=S.song.sections.filter((sc,si)=>Object.values((S.auto[si]||{}).chords||{}).some(L=>L[L.length-1].t>sc.bars*B0+.01)).length; passDiscard(); return bad; }); ok(len===0,'lanes fit sections a pass has shortened');

  // plug-ins Semitone knows
 const sm=[{i:1,name:'Filter 1 On',v:0},{i:2,name:'Filter 1 Freq',v:.5},{i:3,name:'Filter 1 Drive',v:0},{i:4,name:'Main Vol',v:.5},{i:6,name:'Filter 1 Res',v:.1}];
 await put('chords',sm,'Serum 2'); await put('bass',plain,'Bass Synth');
 const kn=await p.evaluate(()=>{ passOpen('redone'); PASS.level=1; const mv=passMoves(), dark=mv.find(m=>m.id==='plug-dark-verse'), gtr=mv.find(m=>m.id==='synth-as-guitar'), ok=[dark.plug[0].text,gtr.plug[0].text], si=projectData().sections.findIndex(x=>x[0]==='verse'), ci=projectData().sections.findIndex(x=>x[0]==='chorus'); const v=S.auto[si].chords, c=S.auto[ci].chords; const opts=targetOpts('song','chords',''); passDiscard(); return {ok,on:v['plug.1'][0].v,freq:v['plug.2'][0].v,drive:c['plug.3'][0].v,dfreq:c['plug.2'][0].v,opts:/Serum 2: main controls/.test(opts)&&/Brightness \(Filter 1\)/.test(opts),other:/Serum 2: everything else/.test(opts)}; });
 console.log(JSON.stringify(kn));
 ok(kn.on===1&&Math.abs(kn.freq-.57)<.01,'a known filter that is off gets turned on for its sections, from an open start');
 ok(kn.drive===.12&&kn.dfreq===.95,'drive on a known plug-in holds the filter open and stays small');
 ok(kn.opts&&kn.other,'Automation lists the main controls first, in plain words');
 const up=await p.evaluate(()=>{ S.parts.chords.sound='h:Test-1'; HOSTS.names[hostCh('chords')]='FM8'; HOSTS.params[hostCh('chords')]=[{i:1,name:'Reverb: On/Off',v:0},{i:2,name:'Reverb: Dry/Wet',v:.5},{i:3,name:'Delay: On/Off',v:0},{i:4,name:'Delay: Dry/Wet',v:.5},{i:5,name:'Master Volume',v:.8}]; passOpen('rubin'); const dry=passMoves().find(m=>m.id==='dry-mix').plug.filter(x=>x.p==='chords').map(x=>x.text); passDiscard(); passOpen('antonoff'); const si=projectData().sections.findIndex((x,i,a)=>a[i+1]&&a[i+1][0]==='chorus'&&x[0]!=='chorus'), L=(S.auto[si].chords||{})['plug.4']; const r={dry,rise:L&&[L[0].v,L[L.length-1].v],on:(S.auto[si].chords||{})['plug.3']}; passDiscard(); return r; });
 console.log(JSON.stringify(up)); ok(up.dry.every(x=>/already off/.test(x)),'turning down an effect that is already off does nothing'); ok(up.rise&&up.rise[0]===0&&up.rise[1]>.4&&up.on,'a rising delay on a known plug-in turns the delay on and starts from nothing');
 await put('chords',synth(1));

 // values are read fresh when a pass opens
 const fresh=await p.evaluate(()=>{ __nlog.length=0; const ch=hostCh('chords'); passOpen('redone'); const asked=__nlog.some(x=>x[0]==='st_host_params'&&x[1].ch===ch); __fire('st_host_paramlist',{ch,list:[{i:1,name:'Filter 1 On',v:0},{i:2,name:'Filter 1 Freq',v:.7},{i:4,name:'Main Vol',v:.5}]}); const line=passMoves().find(m=>m.id==='plug-dark-verse').plug[0].text; passDiscard(); return {asked,line}; });
 ok(fresh.asked&&/switched off in this patch/.test(fresh.line),'the pass reads the patch as it is now and warns about a switched-off filter');
 await p.evaluate(()=>{ passOpen('redone'); }); await p.waitForTimeout(150); await p.screenshot({path:process.env.SHOT||'pass-plug.png'});
 console.log(fails.length?fails.length+' FAILED':'all passed'); console.log(errs.slice(0,6)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
