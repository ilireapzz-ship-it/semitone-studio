// Audio tracks: Add a part → Vocals / Guitar / …, arm and record into the timeline from the play marker, takes cut per section,
// punch-in keeps the old take before it, each track on its own channel, smart pan by name, mono, mix check, mono check.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
// 30 s of a steady tone, mono, 48 kHz
const wav=`(()=>{ const sr=48000,n=sr*30,b=new ArrayBuffer(44+n*2),v=new DataView(b),w=(o,s)=>[...s].forEach((c,i)=>v.setUint8(o+i,c.charCodeAt(0))); w(0,'RIFF');v.setUint32(4,36+n*2,true);w(8,'WAVEfmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);v.setUint32(24,sr,true);v.setUint32(28,sr*2,true);v.setUint16(32,2,true);v.setUint16(34,16,true);w(36,'data');v.setUint32(40,n*2,true);for(let i=0;i<n;i++)v.setInt16(44+i*2,Math.sin(i*.05)*12000,true);return b; })()`;
(async()=>{ const b=await chromium.launch({args:['--autoplay-policy=no-user-gesture-required']}); const errs=[], fails=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const p=await b.newPage({viewport:{width:1440,height:900}}); p.on('pageerror',e=>errs.push(e.message+' '+(e.stack||'').split('\n')[1]));
 await p.addInitScript(`window.__noStart=1; window.__nlog=[];const L={};window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}};window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o));const _f=window.fetch;window.fetch=(u,o)=>String(u).startsWith('/takes/')?Promise.resolve({arrayBuffer:async()=>${wav}}):_f(u,o);`);
 await p.goto(url); await p.waitForTimeout(1200); await p.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:'',recent:[]})); await p.waitForTimeout(800);

 // 1. Add a part → Vocals: a track on channel 16, armed, centred, mono, high-passed; one undo
 const a=await p.evaluate(async()=>{ S.demo=false; LIVE.countIn=false; const e=$('#structSel'); e.value='pop'; e.dispatchEvent(new Event('change',{bubbles:true})); await new Promise(f=>setTimeout(f,100)); const before=H.past.length;
   $('#addPartBtn').click(); document.querySelector('[data-xau="lead"]').click(); await new Promise(f=>setTimeout(f,50));
   const v=auGet('vox'), st=strip('song','vox'), row=document.querySelector('.arr-row[data-au="vox"]');
   return {v, st:{pan:st.pan,mono:st.mono,hp:hasHP(st)}, armed:AU.arm, row:!!row, rowArmed:row&&row.classList.contains('armed'), steps:H.past.length-before, btn:$('#recOpen').textContent, mixer:stripChannels('song').some(x=>x[0]==='vox'&&x[1]==='Vocals'), monoBtn:!!document.querySelector('#cMix .strip[data-ch="vox"] [data-stmore]')&&!!strip('song','vox').mono}; });
 ok(a.v&&a.v.ch===15&&a.v.input==='in1'&&a.v.kind==='lead','Vocals is a track on channel 16, mono input 1');
 ok(a.st.pan===0&&a.st.mono===true&&a.st.hp,'the lead vocal starts centred, mono and high-passed');
 ok(a.armed==='vox'&&a.row&&a.rowArmed&&/Rec · Vocals/.test(a.btn),'it is armed: its row is marked and Record says where it goes '+JSON.stringify([a.armed,a.row,a.rowArmed,a.btn]));
 ok(a.steps===1&&a.mixer&&a.monoBtn,'one undo step, and it has a mixer strip with Mono on (in its ⋯ menu)');

 // 2. smart placement: guitar off to the lighter side; a double puts the pair hard left and right; harmony opposite the lean
 const g=await p.evaluate(()=>{ const x=auAdd('guitar',{quiet:true}), pan1=strip('song',x.id).pan; auDouble(x.id); const d=auTracks().find(a=>a.of===x.id); const bv=auAdd('bv',{quiet:true});
   return {ch:x.ch, pan1, gl:strip('song',x.id).pan, gr:strip('song',d.id).pan, dname:d.name, dArm:AU.arm===bv.id, bvPan:strip('song',bv.id).pan, lean:mixLean(bv.id), chans:new Set(auTracks().map(a=>a.ch)).size===auTracks().length}; });
 ok(Math.abs(g.pan1)===.4,'one guitar sits 40% to one side '+g.pan1);
 ok(g.gl===-.9&&g.gr===.9&&/Guitar/.test(g.dname),'a guitar double: the two takes hard left and right');
 ok(Math.abs(g.bvPan)===.5&&g.chans,'backing vocals spread 50%, and every audio track has its own channel');
 const pl=await p.evaluate(()=>[['Kick',''],['808 sub',''],['Hi-hat',''],['Lead vocal',''],['Strings pad',''],['Shaker','']].map(([n])=>{ const q=mixPlace(n,'',''); return [n,q.pan,q.mono,q.stereo]; }));
 ok(pl[0][1]===0&&pl[0][2]&&pl[1][2]&&pl[3][1]===0&&pl[3][2]&&pl[4][3]&&Math.abs(pl[2][1])===.25&&Math.abs(pl[5][1])===.45,'placement by name: kick, sub and lead vocal centre mono; hats and shaker to the side; pads stereo '+JSON.stringify(pl));

 // 3. a recorded take crossing three sections is cut into three takes on that track; the engine gets them on channel 16
 const r=await p.evaluate(async()=>{ AU.arm='vox'; ensureAudio(); const B=METERS[S.meter].B, spb=60/S.bpm, d0=S.song.sections[0].bars*B*spb, d1=S.song.sections[1].bars*B*spb;
   const ab=await (await fetch('/takes/a.wav')).arrayBuffer(); const T={tr:'vox',si:0,beat0:0,start:10,playT0:10.5,marks:{[S.song.sections[0].bars]:{si:1,t:10.5+d0},[S.song.sections[0].bars+S.song.sections[1].bars]:{si:2,t:10.5+d0+d1}},stopAt:10.5+d0+d1+2};
   const made=await auFinish(T,ab,0,'Take A.wav'); const info=nativeSoundInfo(); const cells=document.querySelectorAll('.arr-row[data-au="vox"] .arr-cell.clip').length;
   return {n:made.length, sis:made.map(t=>t.si), offs:made.map(t=>+t.off.toFixed(2)), want:[.5,+(d0+.5).toFixed(2),+(d0+d1+.5).toFixed(2)], tr:made.every(t=>t.tr==='vox'&&t.file==='Take A.wav'), same:made[0].buf===made[2].buf, takes:info.takes, takeCh:info.takeCh, cells, wave:!!document.querySelector('.arr-row[data-au="vox"] .arr-cell.clip svg rect')}; });
 ok(r.n===3&&r.sis.join()==='0,1,2'&&JSON.stringify(r.offs)===JSON.stringify(r.want),'one recording, cut at each section: '+JSON.stringify(r.offs));
 ok(r.tr&&r.same,'the takes share the one file and decode it once');
 ok(r.takes.length===3&&r.takes.every(t=>t.ch===15&&t.file==='Take A.wav')&&r.takeCh.some(c=>c.ch===15&&c.pan===0),'the engine plays them on the Vocals channel, with its pan and level');
 ok(r.cells===3&&r.wave,'the timeline shows the takes as waveforms in the Vocals row');

 // 4. punch-in: recording again from bar 3 of section 1 keeps bars 1–2 of the old take, bar by bar
 const q=await p.evaluate(async()=>{ const B=METERS[S.meter].B, spb=60/S.bpm, old=TAKES.find(t=>t.tr==='vox'&&t.si===1), ab=await (await fetch('/takes/b.wav')).arrayBuffer(); const T={tr:'vox',si:1,beat0:2*B,start:0,playT0:0,marks:{},stopAt:(S.song.sections[1].bars-2)*B*spb+.4};
   const [nt]=await auFinish(T,ab,0,'Take B.wav'); const C=auComp(1,'vox'), segs=nativeSoundInfo().takes.filter(t=>t.file==='Take B.wav'); return {C, old:old.id, nt:nt.id, b0:nt.b0, n:segs.length, bars:S.song.sections[1].bars}; });
 ok(q.C&&q.C[0]===q.old&&q.C[1]===q.old&&q.C[2]===q.nt&&q.C[q.bars-1]===q.nt&&q.b0===2,'punch-in: the bars before keep the old take, the rest use the new one');
 ok(q.n===q.bars-2,'the engine plays the new take only from where it came in');

 // 5. recording from the transport: armed track, whole song from the marker, input sent to the app, drawn live
 const live=await p.evaluate(async()=>{ __nlog.length=0; S.sel=0; S.playFrom=0; $('#recOpen').click(); const start=__nlog.find(x=>x[0]==='st_take_start'); __fire('st_take_started',{ok:true,inputs:2}); await new Promise(f=>setTimeout(f,400)); for(let i=0;i<6;i++){ __fire('st_peak',{l:0,r:0,ch:[],in:.6}); await new Promise(f=>setTimeout(f,40)); }
   const mode=S.playMode, btn=$('#recOpen').textContent, cv=document.querySelector('.arr-row[data-au="vox"] canvas.au-live'), peaks=LIVE.take&&LIVE.take.peaks.length; $('#recOpen').click(); await new Promise(f=>setTimeout(f,80)); const stop=__nlog.some(x=>x[0]==='st_take_stop'); __fire('st_take_done',{file:'Take C.wav',frames:48000,dropped:0,inLat:0}); await new Promise(f=>setTimeout(f,400));
   return {input:start&&start[1].input, mode, btn, cv:!!cv, peaks, stop, after:$('#recOpen').textContent, n:TAKES.filter(t=>t.file==='Take C.wav').length}; });
 ok(live.input==='in1'&&live.mode==='song'&&live.btn==='Stop','Record with a track armed: the app records input 1 and the whole song plays from the marker');
 ok(live.cv&&live.peaks>0,'the take is drawn into the track while it records');
 ok(live.stop&&live.n>=1&&/Rec · Vocals/.test(live.after),'Stop keeps it as a take on that track');

 // 6. mix check: low end mono and centred, high-pass, then one undo puts it back
 const m=await p.evaluate(()=>{ strip('song','bass').mono=false; strip('song','bass').pan=.3; histCheck(true); const L=mixCheck(), keys=L.map(x=>x.k); const before=H.past.length; mixFix(L.filter(x=>x.k==='mono:bass'||x.k==='cen:bass'),'fix'); const after={mono:strip('song','bass').mono,pan:strip('song','bass').pan,steps:H.past.length-before,eng:nativeSoundInfo().mono}; histUndo(); return {keys,after,undone:{mono:!!strip('song','bass').mono,pan:strip('song','bass').pan}}; });
 ok(m.keys.includes('mono:bass')&&m.keys.includes('cen:bass'),'mix check finds a stereo, off-centre bass');
 ok(m.after.mono&&m.after.pan===0&&m.after.steps===1&&m.after.eng.length>=1,'the fix makes it mono and centred in one step, and the engine hears it '+JSON.stringify(m.after));
 ok(!m.undone.mono&&m.undone.pan===.3,'one undo puts it back');
 const mud=await p.evaluate(()=>{ const ids=[]; ['pad','harmony','arp'].forEach(r=>{ const x=xAdd(r); if(x) ids.push(x.id); }); const L=mixCheck(), m=L.find(x=>x.k==='mud'); return {m:!!m, why:m&&m.why, places:ids.map(id=>strip('song',id).pan)}; });
 ok(mud.m&&/200–500 Hz/.test(mud.why),'stacked low-mid parts get a mud warning');
 ok(mud.places.some(x=>x!==0),'parts added by Add a part are placed by what they are '+JSON.stringify(mud.places));

 // 7. mono check: heard, sent to the engine, never saved
 const mc=await p.evaluate(()=>{ mixMonoToggle(); const on={eng:nativeSoundInfo().mono.includes(16), saved:JSON.stringify(projectData()).includes('monoCheck')}; mixMonoToggle(); return {...on, off:!nativeSoundInfo().mono.includes(16)}; });
 ok(mc.eng&&!mc.saved&&mc.off,'Mono check sums the whole mix in the engine, and is never saved');

 // 8. saved and loaded: the tracks, their takes and comps come back
 const sv=await p.evaluate(()=>{ const d=projectData(); return {audio:(d.audio||[]).map(a=>a.id), takes:(d.takes||[]).filter(t=>t.tr==='vox').length, comp:Object.keys(d.takeComp||{}).length}; });
 ok(sv.audio.includes('vox')&&sv.audio.length>=4&&sv.takes>=4&&sv.comp>=1,'saved: tracks, takes on them, comps '+JSON.stringify(sv));

 // 9. delete a track: one undo brings it back
 const del=await p.evaluate(()=>{ const id=auTracks().find(a=>a.kind==='bv').id; trackDelete(id); const gone=!auGet(id)&&!document.querySelector(`.arr-row[data-au="${id}"]`); histUndo(); return {gone, back:!!auGet(id)}; });
 ok(del.gone&&del.back,'delete a track, and undo brings it back');

 // 10. songs from before: takes with no track show as a Vocals row on channel 16
 const legacy=await p.evaluate(async()=>{ S.audio=[]; TAKES.length=0; const ab=await (await fetch('/takes/x.wav')).arrayBuffer(); await addTake(ab,'Chorus take 1',1,0,0,false,undefined,{file:'Old.wav'}); arrRender(); return {row:!!document.querySelector('.arr-row[data-au="vox"]'), ch:nativeSoundInfo().takes.every(t=>t.ch===15)}; });
 ok(legacy.row&&legacy.ch,'an older song’s takes show as a Vocals track and play on channel 16, as before');

 ok(!errs.length,'no page errors '+errs.join(' | '));
 await b.close(); console.log(fails.length?`${fails.length} failed`:'all passed'); process.exit(fails.length?1:0); })();
