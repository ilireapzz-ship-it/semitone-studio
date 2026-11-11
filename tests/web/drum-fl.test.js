// 12 Nov (Jamie: "a better drum pad and drum piano roll with all the functionality FL Studio's has"): the step grid's strip under the steps
// (strength, pan, pitch, length, nudge, roll), right-click a step (soft, flam, roll, nudge), a beat of 1, 2 or 4 bars, each drum's menu (fill every
// 2/4/8, shift, random, clear, swing, tune, length, repeats every), M and S and a pan knob per drum, the closed hat cutting the open hat, toms as
// drums of their own, Pads (play, record, note repeat), and the drum piano roll's lanes, hit tools and ghosts. Everything is undoable, saved,
// in the MIDI where MIDI can carry it, and sent to the app's engine.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m,x)=>{ console.log((c?'ok   ':'FAIL ')+m+(c||x===undefined?'':' '+JSON.stringify(x))); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} window.__nlog=[]; const L={}; window.__JUCE__={backend:{addEventListener:(e,f)=>{(L[e]=L[e]||[]).push(f); return [e,f];},removeEventListener(){},emitEvent:(e,d)=>__nlog.push([e,d])},initialisationData:{}}; window.__fire=(e,d)=>(L[e]||[]).forEach(f=>f(d)); });
 await p.goto(url); await p.waitForTimeout(1800); await p.evaluate(()=>{ __fire('st_init',{plugin:false,host:'',drumHit:true}); }); await p.waitForTimeout(600);
 // a plain, repeatable beat: no human feel, no fills, every section at full energy
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; const rb=$('#restoreBar'); if(rb) rb.hidden=true; S.demo=false; S.perf.human=false; S.fill=false; S.energyOn=false; S.transOn=false; S.bpm=120; S.swing=50; S.parts.drums.on=true; S.kit='808'; S.groove='backbeat';
   window.SI=S.song.sections.findIndex(s=>s.type==='verse'); selectSection(SI); const n=stepsFor(S.meter), z=()=>Array(n).fill(0); S.beats.verse={kick:z(),snare:z(),clap:z(),hat:z(),open:z(),rim:z(),crash:z()}; S.beats.verse.kick[0]=2; S.beats.verse.kick[8]=1; S.beats.verse.snare[4]=2; S.beats.verse.snare[12]=2; [0,2,4,6,8,10,12,14].forEach(i=>S.beats.verse.hat[i]=1); S.beatVel=undefined; S.beatOff=undefined; S.beatX=undefined; S.rowSet=undefined; S.beatSel='verse'; shellDock('beat'); renderSeq(); histCheck(true);
   window.BAR=k=>barList('song').filter(x=>x.si===SI)[k]; window.EV=(k,r)=>drumPerf(BAR(k)).filter(e=>!r||e.r===r); window.undo1=()=>histUndo?histUndo():null; });
 const ev=(k,r)=>p.evaluate(([k,r])=>EV(k,r).map(e=>({r:e.r,i:e.i,x:+e.x.toFixed(4),vel:+e.vel.toFixed(3),pan:e.pan,tune:e.tune,len:e.len,gr:e.gr||0})),[k,r]);
 const undo=async()=>{ await p.keyboard.press('Control+z'); await p.waitForTimeout(250); };
 const lastLabel=()=>p.evaluate(()=>H.past.length?H.past[H.past.length-1].label:'');
 const box=sel=>p.evaluate(s=>{ const q=document.querySelector(s).getBoundingClientRect(); return {x:q.left,y:q.top,w:q.width,h:q.height}; },sel);

 // ---- the look: every drum has a volume knob, a pan knob, M and S; the strip is under the grid; the header is still one row
 const ui=await p.evaluate(()=>({rows:document.querySelectorAll('#seq .seq-row[data-row]').length,rp:document.querySelectorAll('#seq [data-rp]').length,m:document.querySelectorAll('#seq [data-rm]').length,s:document.querySelectorAll('#seq [data-rs]').length,graph:!!$('#beatGraph'),cols:document.querySelectorAll('#beatGraph .gcol').length,bars:[...document.querySelectorAll('#beatBars [data-bb]')].map(x=>x.textContent+':'+x.getAttribute('aria-pressed')).join(),headH:$('#beatPanel .beat-top').getBoundingClientRect().height,rpW:$('#seq [data-rp]').getBoundingClientRect().width}));
 ok(ui.rows===7&&ui.rp===7&&ui.m===7&&ui.s===7&&ui.graph&&ui.cols===16&&/1 bar:true/.test(ui.bars)&&ui.headH<70&&ui.rpW<20,'every drum has pan, M and S; the strip sits under the grid; the header is one row',ui);

 // ---- the strip under the grid (FL's graph editor)
 await p.click('[data-rowsel="kick"]'); await p.keyboard.press('Escape'); await p.waitForTimeout(100);
 ok(await p.evaluate(()=>BEATUI.row==='kick'&&$('#beatGraph .g-of').textContent==='Kick'&&document.querySelectorAll('#beatGraph .gcol:not(.none)').length===2),'clicking a drum picks it for the strip, which shows its two hits');
 let c=await box('#beatGraph .gcol[data-gi="8"]'); await p.mouse.move(c.x+c.w/2,c.y+4); await p.mouse.down(); await p.mouse.move(c.x+c.w/2,c.y+c.h-6,{steps:4}); await p.mouse.up(); await p.waitForTimeout(200);
 const gv=await p.evaluate(()=>S.beatVel.verse.kick[8]); ok(gv>0&&gv<.3&&/Draw strength on Kick/.test(await lastLabel()),'dragging a bar down makes that hit softer, as one undo step',gv);
 ok((await ev(0,'kick')).find(e=>e.i===8).vel<.3,'and the song plays it that soft');
 await undo(); ok(await p.evaluate(()=>!S.beatVel),'undo puts the strength back');
 await p.selectOption('#graphProp','rep'); await p.waitForTimeout(100); c=await box('#beatGraph .gcol[data-gi="8"]'); await p.mouse.click(c.x+c.w/2,c.y+2); await p.waitForTimeout(200);
 const roll=await ev(0,'kick'); ok(await p.evaluate(()=>S.beatX.verse.kick[8].n===4)&&roll.filter(e=>e.i===8).length===4&&roll.filter(e=>e.i===8).map(e=>e.x).join()==='2,2.0625,2.125,2.1875','Roll in the strip: four hits inside the step',roll);
 await p.selectOption('#graphProp','pan'); c=await box('#beatGraph .gcol[data-gi="0"]'); await p.mouse.click(c.x+c.w/2,c.y+2); await p.waitForTimeout(150);
 await p.selectOption('#graphProp','tune'); c=await box('#beatGraph .gcol[data-gi="0"]'); await p.mouse.click(c.x+c.w/2,c.y+c.h-2); await p.waitForTimeout(150);
 await p.selectOption('#graphProp','len'); c=await box('#beatGraph .gcol[data-gi="0"]'); await p.mouse.click(c.x+c.w/2,c.y+c.h/2); await p.waitForTimeout(150);
 await p.selectOption('#graphProp','off'); c=await box('#beatGraph .gcol[data-gi="0"]'); await p.mouse.click(c.x+c.w/2,c.y+2); await p.waitForTimeout(150);
 const k0=(await ev(0,'kick')).find(e=>e.i===0&&!e.gr); ok(k0.pan===1&&k0.tune===-12&&Math.abs(k0.len-.5)<.06&&Math.abs(k0.x-.12)<.011,'pan, pitch, length and nudge drawn on one hit all reach the song',k0);
 await p.dblclick('#beatGraph .gcol[data-gi="0"]'); await p.waitForTimeout(200); ok(await p.evaluate(()=>!(S.beatOff||{}).verse),'double-click puts a bar back');
 await p.evaluate(()=>{ S.beatX=undefined; S.beatOff=undefined; BEATUI.prop='vel'; renderSeq(); histCheck(true); });

 // ---- right-click a step
 await p.click('#seq .step[data-r="snare"][data-i="4"]',{button:'right'}); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>!$('#stepPop').hidden&&/Snare/.test($('#stepPop .rm-h').textContent)),'right-click opens the step menu');
 await p.click('#stepPop [data-sd="flam"]'); await p.waitForTimeout(150); let sn=await ev(0,'snare');
 ok(sn.filter(e=>e.i===4).length===2&&sn.find(e=>e.i===4&&e.gr).x<1&&sn.find(e=>e.i===4&&e.gr).x>.9&&sn.find(e=>e.i===4&&e.gr).vel<sn.find(e=>e.i===4&&!e.gr).vel,'Flam: a softer hit just before the main one',sn);
 await p.click('#stepPop [data-sd="late"]'); await p.click('#stepPop [data-sd="late"]'); await p.waitForTimeout(150); sn=await ev(0,'snare'); ok(Math.abs(sn.find(e=>e.i===4&&!e.gr).x-1.06)<.001,'Nudge late moves it behind the grid',sn);
 await p.click('#stepPop [data-sd="soft"]'); await p.waitForTimeout(120); ok(await p.evaluate(()=>S.beatVel.verse.snare[4]===.3&&$('#stepPop [data-sd="soft"]').getAttribute('aria-pressed')==='true'),'Soft makes a ghost note');
 await p.click('#stepPop [data-sd="rep3"]'); await p.waitForTimeout(120); ok((await ev(0,'snare')).filter(e=>e.i===4).length===4,'Roll ×3 with a flam: three hits and the grace hit');
 await p.click('#stepPop [data-sd="reset"]'); await p.waitForTimeout(120); ok(await p.evaluate(()=>!S.beatX&&!S.beatOff&&!S.beatVel&&S.beats.verse.snare[4]===1),'Back to a plain hit clears it all');
 await p.keyboard.press('Escape'); await p.evaluate(()=>{ S.beats.verse.snare[4]=2; renderSeq(); histCheck(true); });
 ok(await p.evaluate(()=>$('#stepPop').hidden),'Escape closes the step menu');
 const marks=await p.evaluate(()=>{ hitXSet('verse','snare',12,{n:2,f:1}); mapSet('beatOff','verse','snare',12,-.03); renderSeq(); const c=document.querySelector('#seq .step[data-r="snare"][data-i="12"]'); const o={mk:c.dataset.mk,early:c.classList.contains('early')}; hitXSet('verse','snare',12,null); mapSet('beatOff','verse','snare',12,null); renderSeq(); histCheck(true); return o; });
 ok(marks.mk==='×2ƒ'&&marks.early,'a step shows its roll, flam and nudge',marks);

 // ---- the beat can be 2 or 4 bars
 await p.click('#beatBars [data-bb="2"]'); await p.waitForTimeout(250);
 const two=await p.evaluate(()=>({len:S.beats.verse.kick.length,same:JSON.stringify(S.beats.verse.kick.slice(0,16))===JSON.stringify(S.beats.verse.kick.slice(16)),steps:document.querySelectorAll('#seq .step[data-r="kick"]').length,label:H.past[H.past.length-1].label}));
 ok(two.len===32&&two.same&&two.steps===32&&/2 bars long/.test(two.label),'2 bars: the grid doubles and bar 2 starts as a copy',two);
 await p.click('#seq .step[data-r="kick"][data-g="22"]'); await p.waitForTimeout(150);
 const bars=await p.evaluate(()=>[0,1,2,3].map(k=>EV(k,'kick').map(e=>e.i).join()));
 ok(bars[0]==='0,8'&&bars[1]==='0,6,8'&&bars[2]==='0,8'&&bars[3]==='0,6,8','a hit added in bar 2 plays in every second bar',bars);
 const rollTB=await p.evaluate(async()=>{ drumRollOpen(); await new Promise(r=>setTimeout(r,250)); const o={TB:S.rl.TB,rects:S.rects.length}; shellDock('beat'); return o; }); ok(rollTB.TB===8&&rollTB.rects===25,'the drum piano roll shows both bars',rollTB);
 await p.click('#beatBars [data-bb="1"]'); await p.waitForTimeout(200); ok(await p.evaluate(()=>S.beats.verse.kick.length===16&&patBars('verse')===1),'back to 1 bar keeps the first bar');
 await undo(); await undo(); await undo(); ok(await p.evaluate(()=>S.beats.verse.kick.length===16&&!S.beats.verse.kick[6]),'undo steps back through all of it');

 // ---- each drum's menu
 await p.click('[data-rowsel="hat"]'); await p.waitForTimeout(150);
 await p.click('#rowPop [data-rowdo="fill4"]'); await p.waitForTimeout(120); ok(await p.evaluate(()=>S.beats.verse.hat.map((v,i)=>v?i:-1).filter(i=>i>=0).join()==='0,4,8,12'&&!$('#rowPop').hidden),'Fill every 4 steps, and the menu stays open');
 await p.click('#rowPop [data-rowdo="rotr"]'); await p.waitForTimeout(120); ok(await p.evaluate(()=>S.beats.verse.hat.map((v,i)=>v?i:-1).filter(i=>i>=0).join()==='1,5,9,13'),'Shift moves every hit a step later');
 await p.click('#rowPop [data-rowdo="rotl"]'); await p.waitForTimeout(120);
 await p.selectOption('#rowPop [data-rowset="loop"]','6'); await p.waitForTimeout(200);
 const poly=await p.evaluate(()=>({len:S.beats.verse.hat.length,b0:EV(0,'hat').map(e=>e.i).join(),b1:EV(1,'hat').map(e=>e.i).join(),ghosts:document.querySelectorAll('#seq .step.lp[data-r="hat"]').length,tag:!!document.querySelector('[data-rowsel="hat"] .rl-lp')}));
 ok(poly.len===6&&poly.b0==='0,4,6,10,12'&&poly.b1==='0,2,6,8,12,14'&&poly.ghosts===10&&poly.tag,'Repeats every 6 steps: the hat cycles against the 16-step beat, its repeats shown faded',poly);
 await p.selectOption('#rowPop [data-rowset="loop"]','0'); await p.waitForTimeout(150); ok(await p.evaluate(()=>S.beats.verse.hat.length===16),'the whole beat again');
 await p.click('#rowPop [data-rowdo="rand"]'); await p.waitForTimeout(120); const r1=await p.evaluate(()=>S.beats.verse.hat.join('')); ok(r1.length===16&&/[12]/.test(r1),'Random writes a pattern',r1);
 await p.click('#rowPop [data-rowdo="clr"]'); await p.waitForTimeout(120); ok(await p.evaluate(()=>S.beats.verse.hat.every(v=>!v)),'Clear empties the drum');
 await p.click('#rowPop [data-rowdo="fill2"]'); await p.waitForTimeout(120);
 const slide=(k,v)=>p.evaluate(([k,v])=>{ const el=document.querySelector(`#rowPop [data-rowset="${k}"]`); el.value=v; el.dispatchEvent(new Event('input',{bubbles:true})); el.dispatchEvent(new Event('change',{bubbles:true})); },[k,v]);
 await p.evaluate(()=>{ S.swing=62; }); await slide('swing',0); await slide('tune',-5); await slide('len',50); await p.waitForTimeout(200);
 const hat=await ev(0,'hat'), kick=await p.evaluate(()=>swingT(.5));
 ok(await p.evaluate(()=>JSON.stringify(S.rowSet.hat)==='{"swing":0,"tune":-5,"len":0.5}')&&hat.find(e=>e.i===2).x===.5&&kick===.62&&hat.every(e=>e.tune===-5&&e.len===.5),'Swing 0 keeps the hat straight while the song swings; Tune and Length reach every hat hit',hat.slice(0,2));
 ok(/Closed hat length 50%/.test(await lastLabel()),'each slider is one labelled undo step',await lastLabel());
 await p.keyboard.press('Escape'); await p.evaluate(()=>{ S.swing=50; S.rowSet=undefined; renderSeq(); histCheck(true); });

 // ---- M, S and the pan knob
 await p.click('[data-rm="kick"]'); await p.waitForTimeout(150); ok((await ev(0,'kick')).length===0&&(await ev(0,'snare')).length===2&&await p.evaluate(()=>document.querySelector('.seq-row[data-row="kick"]').classList.contains('off')),'M mutes one drum');
 await p.click('[data-rm="kick"]'); await p.click('[data-rs="snare"]'); await p.waitForTimeout(150); ok((await ev(0)).every(e=>e.r==='snare')&&(await ev(0)).length===2,'S plays only that drum');
 await p.click('[data-rs="snare"]'); await p.waitForTimeout(150); ok(await p.evaluate(()=>!S.rowSet)&&(await ev(0)).length===12,'and off again');
 c=await box('[data-rp="hat"]'); await p.mouse.move(c.x+c.w/2,c.y+c.h/2); await p.mouse.down(); await p.mouse.move(c.x+c.w/2,c.y+c.h/2-45,{steps:5}); await p.mouse.up(); await p.waitForTimeout(250);
 ok(await p.evaluate(()=>S.rowSet.hat.pan===.5)&&(await ev(0,'hat')).every(e=>e.pan===.5)&&/Closed hat pan 50 right/.test(await lastLabel()),'the pan knob: drag up for right, one undo step',await lastLabel());
 const law=await p.evaluate(async()=>{ ensureAudio(); if(!master) master=newMaster('song'); delete master.buses['song:drums']; getBus('song','drums'); const used=!!master.buses['song:drums'].lawPan;
   const real=ac, rm=master, off=new OfflineAudioContext(2,4410,44100); ac=off; master=off.createGain(); master.connect(off.destination); const src=off.createConstantSource(); src.offset.value=1; const q=lawPan(src,.5); q.node.connect(master); src.start(); const buf=await off.startRendering(); ac=real; master=rm; return {used,L:+buf.getChannelData(0)[2000].toFixed(3),R:+buf.getChannelData(1)[2000].toFixed(3)}; });
 ok(law.used&&Math.abs(law.L-.383)<.004&&Math.abs(law.R-.924)<.004,'with a panned drum the drum strip pans left and right by the same law as the engine',law);
 await p.dblclick('[data-rp="hat"]'); await p.waitForTimeout(200); ok(await p.evaluate(()=>!S.rowSet),'double-click centres it');

 // ---- the closed hat cuts the open hat short
 await p.evaluate(()=>{ S.beats.verse.open[2]=1; S.beats.verse.hat=S.beats.verse.hat.map((v,i)=>i===3?1:i===2?0:v); renderSeq(); histCheck(true); });
 const ch1=(await ev(0,'open'))[0]; await p.click('[data-rowsel="open"]'); await p.waitForTimeout(120); await p.click('#rowPop [data-rowset="choke"]'); await p.waitForTimeout(200); const ch2=(await ev(0,'open'))[0];
 ok(ch1.len<.6&&ch1.len>.3&&ch2.len===1&&await p.evaluate(()=>S.rowSet.open.choke===false),'an open hat is cut short by the next closed hat; the tick box lets it ring',[ch1.len,ch2.len]);
 await p.click('#rowPop [data-rowset="choke"]'); await p.keyboard.press('Escape'); await p.waitForTimeout(150);

 // ---- MIDI and the app's engine get the same hits
 const eng=await p.evaluate(async()=>{ rowSetPut('hat',{pan:-.5}); hitXSet('verse','kick',8,{n:3,t:7}); hitXSet('verse','snare',4,{f:1}); histCheck(true); renderSeq(); NATIVE.lastTl=''; nativeSync(); await new Promise(r=>setTimeout(r,80));
   const tl=__nlog.filter(x=>x[0]==='st_timeline').pop()[1], B=METERS[S.meter].B, a=S.song.sections[SI].startBar*B, notes=[], ctl=[]; for(let i=0;i<tl.notes.length;i+=5) if(tl.notes[i+2]===9&&tl.notes[i]>=a-1e-6&&tl.notes[i]<a+B-1e-6) notes.push([+(tl.notes[i]-a).toFixed(3),tl.notes[i+3]]); for(let i=0;i<tl.ev.length;i+=4) if((tl.ev[i+1]&0xF0)===0xB0&&tl.ev[i]>=a-1e-6&&tl.ev[i]<a+B-1e-6) ctl.push([+(tl.ev[i]-a).toFixed(3),tl.ev[i+1],tl.ev[i+2],tl.ev[i+3]]);
   const perf=drumEventsForBar(BAR(0)).length; NATIVE.drumHit=false; NATIVE.lastTl=''; nativeSync(); await new Promise(r=>setTimeout(r,80)); const tl2=__nlog.filter(x=>x[0]==='st_timeline').pop()[1]; let old=0; for(let i=0;i<tl2.ev.length;i+=4) if((tl2.ev[i+1]&0xF0)===0xB0) old++; NATIVE.drumHit=true; return {notes,ctl,perf,old}; });
 const kk=eng.notes.filter(n=>n[1]===36).map(n=>n[0]).join(), hatC=eng.ctl.filter(q=>q[2]===85&&q[3]===42).length, panC=eng.ctl.filter(q=>q[2]===86), tuneC=eng.ctl.filter(q=>q[2]===87), lenC=eng.ctl.filter(q=>q[2]===88);
 ok(eng.notes.length===eng.perf&&kk==='0,2,2.083,2.167'&&eng.notes.filter(n=>n[1]===38).length===3,'the MIDI and the engine get the roll and the flam as real notes',eng.notes);
 ok(hatC===8&&panC.length===8&&panC.every(q=>q[1]===0xB9&&q[3]===33)&&tuneC.length===3&&tuneC.every(q=>q[3]===92)&&lenC.length===1&&eng.old===0,'the engine is told each hit’s pan, pitch and length just before its note; an older app gets none of it',{hatC,pan:panC[0],tune:tuneC[0],len:lenC[0],old:eng.old});
 const order=await p.evaluate(()=>{ const tl=__nlog.filter(x=>x[0]==='st_timeline').slice(-2)[0][1]; for(let i=0;i<tl.ev.length;i+=4){ if((tl.ev[i+1]&0xF0)!==0xB0||tl.ev[i+2]!==85) continue; const at=tl.ev[i], key=tl.ev[i+3]; let hit=false; for(let j=0;j<tl.notes.length;j+=5) if(tl.notes[j+2]===9&&tl.notes[j+3]===key&&Math.abs(tl.notes[j]-at)<1e-4){ hit=true; break; } if(!hit) return [at,key]; } return null; });
 ok(order===null,'every one of those messages lands exactly on its note',order);
 await p.evaluate(()=>{ S.beatX=undefined; S.rowSet=undefined; renderSeq(); histCheck(true); });

 // ---- toms are drums of their own
 await p.click('[data-rowback="tomH"]'); await p.waitForTimeout(200);
 const tom=await p.evaluate(()=>{ S.beats.verse.tomH[15]=1; renderSeq(); histCheck(true); const R=readMidi(buildMidi()); return {rows:ROWS.includes('tomH'),name:rowInfo('tomH')[0],key:padKeyOf('tomH'),perf:EV(0,'tomH').length,gm:gmOf('tomH')}; });
 ok(tom.rows&&tom.name==='High tom'&&tom.key==='W'&&tom.perf===1&&tom.gm===50,'＋ High tom adds a tom row that plays (MIDI note 50, key W)',tom);

 // ---- Pads
 await p.evaluate(()=>{ window.__hits=[]; const d0=drumHit; drumHit=function(k,t,v,o){ if(!(o&&o.bus)) __hits.push([k,+v.toFixed(2)]); return d0.apply(this,arguments); }; });
 await p.click('#beatView [data-bv="pads"]'); await p.waitForTimeout(200);
 const pads=await p.evaluate(()=>({n:document.querySelectorAll('#beatPads .pad[data-pad]').length,add:document.querySelectorAll('#beatPads .pad.add').length,toms:!!document.querySelector('#beatPads .pad[data-pad="tomL"]'),steps:$('#beatSteps').hidden,first:document.querySelector('#beatPads .pad').textContent,rep:document.querySelectorAll('#padRep [data-prep]').length}));
 ok(pads.n===10&&pads.add===2&&pads.toms&&pads.steps&&/Kick/.test(pads.first)&&pads.rep===5,'Pads shows a pad per drum with its key, the toms, empty pads for your own sounds, and note repeat',pads);
 c=await box('#beatPads .pad[data-pad="snare"]'); await p.mouse.move(c.x+c.w/2,c.y+c.h-3); await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(80); await p.mouse.move(c.x+c.w/2,c.y+3); await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(80);
 await p.keyboard.down('a'); await p.keyboard.up('a'); await p.waitForTimeout(80);
 const hits=await p.evaluate(()=>({h:__hits.slice(),tool:S.rollTool||'select'})); ok(hits.h.length===3&&hits.h[0][0]==='snare'&&hits.h[0][1]>hits.h[1][1]&&hits.h[2][0]==='kick'&&hits.tool==='select','a pad plays, harder nearer the bottom; the A key plays the kick and changes no tool',hits);
 // record a take from the pads
 await p.evaluate(()=>{ LIVE.countIn=false; S.bpm=150; S.loopOn=true; }); await p.click('#padRec'); await p.waitForTimeout(350);
 const tap=async r=>{ const q=await box(`#beatPads .pad[data-pad="${r}"]`); await p.mouse.move(q.x+q.w/2,q.y+q.h/2); await p.mouse.down(); await p.waitForTimeout(60); await p.mouse.up(); await p.waitForTimeout(140); };
 for(const r of ['kick','hat','snare','hat','kick','hat','snare','hat']) await tap(r);
 const during=await p.evaluate(()=>({rec:LIVE.rec,n:LIVE.recNotes.length,btn:$('#padRec')?$('#padRec').textContent:''})); await p.click('#padRec'); await p.waitForTimeout(500);
 const took=await p.evaluate(()=>({kick:S.beats.verse.kick.filter(Boolean).length,snare:S.beats.verse.snare.filter(Boolean).length,hat:S.beats.verse.hat.filter(Boolean).length,vel:!!(S.beatVel&&S.beatVel.verse),label:H.past[H.past.length-1].label,tom:S.beats.verse.tomH[15]}));
 ok(during.rec&&during.n>=7&&took.kick>=1&&took.kick<=2&&took.snare>=1&&took.snare<=2&&took.hat>=2&&took.vel&&/Played Drums/.test(took.label)&&took.tom===1,'Record from the pads writes the beat (with how hard you hit) in one undo step and leaves the drums you didn’t touch',{during,took});
 // note repeat: hold a pad and it rolls in time
 await p.evaluate(()=>{ __hits=[]; }); await p.click('#padRep [data-prep="0.125"]'); await p.click('#padRec'); await p.waitForTimeout(350);
 c=await box('#beatPads .pad[data-pad="hat"]'); await p.mouse.move(c.x+c.w/2,c.y+c.h/2); await p.mouse.down(); await p.waitForTimeout(650); await p.mouse.up(); await p.waitForTimeout(100); await p.click('#padRec'); await p.waitForTimeout(500);
 const rep=await p.evaluate(()=>({played:__hits.filter(h=>h[0]==='hat').length,steps:S.beats.verse.hat.filter(Boolean).length,rolls:Object.values(((S.beatX||{}).verse||{}).hat||{}).filter(x=>x.n>1).length})); ok(rep.played>=6&&rep.steps>=3&&rep.rolls>=2,'Note repeat at 1/32: holding a pad rolls, and the take keeps the roll inside each step',rep);
 await p.click('#padRep [data-prep="0"]'); await p.click('#beatView [data-bv="steps"]'); await p.waitForTimeout(150); ok(await p.evaluate(()=>!$('#beatSteps').hidden&&$('#beatPads').hidden),'back to Steps');

 // ---- the drum piano roll: lanes, hit tools, ghosts
 await p.evaluate(()=>{ S.bpm=120; S.beatX=undefined; S.beatVel=undefined; S.beatOff=undefined; const n=16, z=()=>Array(n).fill(0); ROWS.forEach(r=>S.beats.verse[r]=z()); S.beats.verse.kick[0]=2; S.beats.verse.snare[4]=2; S.beats.verse.hat[8]=1; histCheck(true); drumRollOpen(); }); await p.waitForTimeout(350);
 const bar=await p.evaluate(()=>({lane:!!$('#dLaneSel'),tools:!!document.querySelector('[data-dr="tools"]'),fits:(()=>{ const q=$('#dLaneSel').getBoundingClientRect(), t=document.querySelector('[data-dr="tools"]').getBoundingClientRect(), L=$('.roll-lane').getBoundingClientRect(); return q.width>80&&document.elementFromPoint(q.left+q.width/2,q.top+q.height/2)===$('#dLaneSel')&&t.right<=L.right&&q.top>=L.top&&t.bottom<=L.bottom; })(),grid:!!document.querySelector('[data-pb="steps"]'),rows:S.rl.hi-S.rl.lo+1,n:ROWS.length}));
 ok(bar.lane&&bar.tools&&bar.fits&&bar.grid&&bar.rows===bar.n,'the drum roll has a row per drum, a lane menu and Drum tools',bar);
 await p.selectOption('#dLaneSel','pan'); await p.waitForTimeout(150);
 const lx=await p.evaluate(()=>{ const cv=$('#rollLaneC'), R=cv.getBoundingClientRect(), n=rollArr().find(q=>drumRow(q.m)==='snare'); return {x:R.left+VEL.X(n.start),t:R.top,h:R.height}; });
 await p.mouse.move(lx.x,lx.t+lx.h-4); await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(250);
 ok(await p.evaluate(()=>S.beatX.verse.snare[4].p<-.8)&&/pan/.test(await lastLabel())&&(await ev(0,'snare'))[0].pan<-.8,'the Pan lane: a click low in the lane sends that hit left, in the song too',await lastLabel());
 await p.selectOption('#dLaneSel','tune'); await p.mouse.move(lx.x,lx.t+18); await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(200);
 await p.selectOption('#dLaneSel','len'); await p.mouse.move(lx.x,lx.t+lx.h/2+8); await p.mouse.down(); await p.mouse.up(); await p.waitForTimeout(200);
 const sx=await p.evaluate(()=>S.beatX.verse.snare[4]); ok(sx.t>6&&sx.l>.2&&sx.l<.8&&sx.p<-.8,'Pitch and Length lanes change the same hit',sx);
 await p.selectOption('#dLaneSel','vel'); await p.waitForTimeout(100);
 const cell=(row,i)=>p.evaluate(([row,i])=>{ const R=$('#roll').getBoundingClientRect(), L=S.rl; return {x:R.left+(i+.5)*.25/L.TB*L.W,y:R.top+L.top+(ROWS.indexOf(row)+.5)*L.rh}; },[row,i]);
 c=await cell('kick',0); await p.mouse.click(c.x,c.y); await p.waitForTimeout(150); await p.click('[data-dr="tools"]'); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>!$('#drumPop').hidden&&/1 hit picked/.test($('#drumPop').textContent)),'Hit tools opens for the picked hit');
 await p.click('#drumPop [data-dt="flam"]'); await p.click('#drumPop [data-dt="rep2"]'); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>JSON.stringify(S.beatX.verse.kick[0])==='{"n":2,"f":1}')&&(await ev(0,'kick')).length===3,'Flam and Roll from the drum roll land on the hit');
 await p.click('#drumPop [data-dt="st:e8"]'); await p.waitForTimeout(250);
 ok(await p.evaluate(()=>S.beats.verse.kick.map((v,i)=>v?i:-1).filter(i=>i>=0).join()==='0,2,4,6,8,10,12,14'&&$('#drumPop').hidden&&S.rects.length===10)&&/Kick on eighths/.test(await lastLabel()),'Stamp writes a ready-made rhythm on that drum',await lastLabel());
 await p.click('[data-dr="tools"]'); await p.waitForTimeout(150); const g0=await p.evaluate(()=>DRUMUI.ghost); await p.click('#drumPop [data-dr="ghost"]'); await p.waitForTimeout(100); ok(g0===true&&await p.evaluate(()=>DRUMUI.ghost===false),'Ghosts (where the melody and bass land) is on to start and can be turned off');
 await p.click('#drumPop [data-dr="ghost"]'); await p.keyboard.press('Escape');

 // ---- saved with the song, and a new song starts clean
 const saved=await p.evaluate(()=>{ rowSetPut('kick',{pan:.3,mute:true}); hitXSet('verse','hat',8,{n:3}); const d=JSON.parse(JSON.stringify(projectData())), a=JSON.stringify([S.beatX,S.rowSet]); S.beatX=undefined; S.rowSet=undefined; applyProject(d); return {same:JSON.stringify([S.beatX,S.rowSet])===a,x:!!d.beatX,r:!!d.rowSet}; });
 ok(saved.same&&saved.x&&saved.r,'the hits’ settings and each drum’s settings are saved with the song',saved);
 ok(!errs.length,'no page errors',errs);
 await b.close(); console.log(fails.length?fails.length+' failed':'all passed'); console.log(JSON.stringify(errs)); process.exit(fails.length?1:0); })();
