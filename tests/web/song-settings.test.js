// Song settings (docs/design/panels §1): cards that drive the same settings as the old controls; every change is one undo step.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; spPut({big:true,fold:false}); shellDock('style'); histCheck(true); });
 const t0=await p.evaluate(()=>({cards:document.querySelectorAll('#setPane .ss-card').length,structs:[...document.querySelectorAll('[data-setstruct]')].map(x=>x.dataset.setstruct),on:document.querySelector('.ss-struct.on')&&document.querySelector('.ss-struct.on').dataset.setstruct,cur:S.structure,tabs:DRAWER.sound.p.map(x=>x[1]),old:!!$('#setAll #keySel')}));
 ok(t0.cards===7,'seven cards: style, structure, key, time, feel, performance, energy ('+t0.cards+')');
 ok(t0.structs.join()==='loop,radio,prechorus,pop,hook,ballad,aaba,epic','eight structure cards: '+t0.structs.join(', '));
 ok(t0.on===t0.cur,'the current structure is ringed ('+t0.on+')');
 ok(t0.tabs.join()==='Instruments,Record,Loops','sound tabs: '+t0.tabs.join(' · ')+' (DAW & MIDI is gone)');
 ok(t0.old,'the old controls stay under “All settings”');
 const step=async(fn)=>p.evaluate(async src=>{ const h0=H.past.length; await eval(src)(); await new Promise(r=>setTimeout(r,60)); return {n:H.past.length-h0,label:H.past.length?H.past[H.past.length-1].label:''}; },fn);
 let r=await step("()=>document.querySelector('[data-setstruct=\"prechorus\"]').click()");
 const t1=await p.evaluate(()=>({st:S.structure,types:S.song.sections.map(s=>s.type).join(' ')}));
 ok(t1.st==='prechorus'&&/verse pre chorus/.test(t1.types)&&r.n===1,`Verse–pre–chorus: ${t1.types} · ${r.n} undo step (${r.label})`);
 await p.evaluate(()=>histUndo()); ok(await p.evaluate(()=>S.structure)!=='prechorus','undo puts the structure back');
 // key: a tonic asks up or down, nearest first
 const k0=await p.evaluate(()=>({key:S.key,mel:JSON.stringify(S.song.sections.map((s,i)=>(sectionView(i).melody||[]).map(n=>n.m)))}));
 const tgt=(k0.key+2)%12; await p.click(`[data-settonic="${tgt}"]`); await p.waitForTimeout(60);
 const ask=await p.evaluate(()=>[...document.querySelectorAll('[data-settrans]')].map(x=>[x.dataset.settrans,x.classList.contains('primary')]));
 ok(ask.length===2&&ask[0][0]==='2'&&ask[0][1],'a new tonic asks up or down, nearest (up 2) first: '+JSON.stringify(ask));
 r=await step("()=>document.querySelector('[data-settrans=\"2\"]').click()");
 const k1=await p.evaluate(()=>({key:S.key,mel:S.song.sections.map((s,i)=>(sectionView(i).melody||[]).map(n=>n.m))}));
 const m0=JSON.parse(k0.mel).flat(), m1=k1.mel.flat(); ok(k1.key===tgt&&m0.length&&m1.every((m,i)=>m===m0[i]+2)&&r.n===1,`key → ${k1.key}, melody up 2, one undo step (${r.label})`);
 r=await step("()=>document.querySelector('[data-setmm=\"ionian\"]').click()"); ok(await p.evaluate(()=>S.mode)==='ionian'&&r.n===1,'Major/Minor switch: major, one undo step');
 r=await step("()=>document.querySelector('[data-setmode=\"dorian\"]').click()"); ok(await p.evaluate(()=>S.mode)==='dorian'&&r.n===1,'mode buttons: Dorian, one undo step');
 r=await step("()=>document.querySelector('[data-setkey=\"9:aeolian\"]').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); ok(await p.evaluate(()=>S.key===9&&S.mode==='aeolian')&&r.n===1,'the circle of fifths: A minor (inner ring), one undo step');
 const sw=await p.evaluate(()=>({gold:!!document.querySelector('.ss-circle circle[fill="var(--st-accent)"]'),songs:setSongs().length,dots:document.querySelectorAll('.ss-tonic .ss-dot').length}));
 ok(sw.gold&&(sw.songs===0||sw.dots>0),`the current key is filled brass; green dots mark this era’s keys (${sw.dots} of ${sw.songs} songs)`);
 // time
 const b0=await p.evaluate(()=>S.bpm); r=await step("()=>document.querySelector('[data-setbpm=\"1\"]').click()"); ok(await p.evaluate(()=>S.bpm)===b0+1&&r.n===1,'＋ adds 1 BPM, one undo step');
 await p.evaluate(()=>{ S.bpm=120; syncControls(); histCheck(true); }); r=await step("()=>document.querySelector('[data-setbpmx=\".5\"]').click()"); ok(await p.evaluate(()=>S.bpm)===60,'½× halves the tempo');
 const tap=await p.evaluate(async()=>{ const b=document.querySelector('[data-set="tap"]'); for(let i=0;i<5;i++){ b.click(); await new Promise(r=>setTimeout(r,500)); } return S.bpm; }); ok(Math.abs(tap-120)<=4,'tap tempo at 2 taps a second → '+tap+' BPM');
 r=await step("()=>document.querySelector('[data-setmeter=\"3/4\"]').click()"); ok(await p.evaluate(()=>S.meter)==='3/4'&&r.n===1,'metre 3/4, one undo step');
 r=await step("()=>document.querySelector('[data-setlen=\"5min\"]').click()"); ok(await p.evaluate(()=>recipeOf().length)==='5min'&&r.n===1,'song length 5 min, one undo step');
 // feel
 r=await step("()=>document.querySelector('[data-setcomp=\"stabs\"]').click()"); ok(await p.evaluate(()=>S.comp)==='stabs'&&r.n===1,'chord rhythm cards: Stabs, one undo step');
 const d0=await p.evaluate(()=>S.density); r=await step("()=>{ const k=document.querySelector('[data-setk=\"density\"]'); k.focus(); k.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); }");
 const d1=await p.evaluate(()=>S.density); ok(d1>d0&&d1-d0<=.0501&&Math.abs(d1*20-Math.round(d1*20))<1e-6&&r.n===1,`↑ on the Melody knob: ${Math.round(d0*100)}% → ${Math.round(d1*100)}% (next 5% step), one undo step`);
 const kb=await p.locator('[data-setk="leap"]').boundingBox(), l0=await p.evaluate(()=>S.leap), h0=await p.evaluate(()=>H.past.length);
 await p.mouse.move(kb.x+kb.width/2,kb.y+kb.height/2); await p.mouse.down(); await p.mouse.move(kb.x+kb.width/2,kb.y-15,{steps:5}); await p.mouse.up(); await p.waitForTimeout(80);
 const l1=await p.evaluate(h0=>({v:S.leap,n:H.past.length-h0}),h0); ok(l1.v>l0+.1&&l1.n===1,`drag up on Leaps: ${l0.toFixed(2)} → ${l1.v.toFixed(2)}, one undo step`);
 r=await step("()=>{ for(let i=0;i<6;i++) document.querySelector('[data-setk=\"quirk\"]').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); }");
 ok(await p.evaluate(()=>S.qOn&&setQuirkAmt()>.2),'Quirks knob switches quirks on and sets how much');
 // performance
 const s0=await p.evaluate(()=>S.sevenths); r=await step("()=>document.querySelector('[data-setsw=\"sevenths\"]').click()"); ok(await p.evaluate(()=>S.sevenths)===!s0&&r.n===1,'seventh chords switch, one undo step');
 const v0=await p.evaluate(()=>S.perf.voice); r=await step("()=>document.querySelector('[data-setsw=\"voice\"]').click()"); ok(await p.evaluate(()=>S.perf.voice)===!v0,'voice-led chords switch');
 await p.evaluate(()=>{ const s=document.querySelector('[data-setbass]'); s.value='walk'; s.dispatchEvent(new Event('change',{bubbles:true})); }); ok(await p.evaluate(()=>S.perf.bass)==='walk','bass pattern ▾: Walking');
 // energy + lift
 r=await step("()=>document.querySelector('[data-setlift=\"2\"]').click()"); ok(await p.evaluate(()=>S.lift)===2&&r.n===1,'Final chorus lift +2, one undo step');
 ok(await p.evaluate(()=>/melody peak [A-G]/.test(document.querySelector('.ss-pk').textContent)),'energy card names the melody’s peak note');
 // examples + section colours
 const ex=await p.evaluate(()=>({shape:[setShapeOf({sections:{verse:'',chorus:''}}),setShapeOf({sections:{verse:'','pre-chorus':'',pre:'',chorus:''}}),setShapeOf({sections:{verse:'',chorus:'',bridge:''}}),setShapeOf({sections:{verse:''}}),setShapeOf({sections:{refrain:''}})].join(),
   keys:JSON.stringify(setParseKey('D minor / F major').concat(setParseKey('E mixolydian'))),dim:[...document.querySelectorAll('.ss-struct.dim')].every(x=>/none in this era/.test(x.textContent)),
   pill:getComputedStyle(document.querySelector('#timeline .sec')).boxShadow}));
 ok(ex.shape==='radio,prechorus,pop,loop,hook','songs are sorted into shapes by their section types: '+ex.shape);
 ok(ex.keys==='[{"pc":2,"mode":"aeolian","minor":true},{"pc":5,"mode":"ionian","minor":false},{"pc":4,"mode":"mixolydian","minor":false}]','“D minor / F major” counts as both; modal keys keep their mode');
 ok(ex.dim,'cards with no songs in this era dim and say so');
 ok(/inset/.test(ex.pill)&&!/rgba\(0, 0, 0, 0\)/.test(ex.pill),'the timeline’s section pills carry the section colour: '+ex.pill.slice(0,40));
 await p.evaluate(()=>{ shellDock('style'); spFit(); }); await p.waitForTimeout(200); await p.screenshot({path:'song-settings.png'});
 ok(!errs.length,'no page errors '+errs.slice(0,3).join(' | '));
 await b.close(); if(fails.length){ console.log(fails.length+' failed'); process.exit(1); } })();
