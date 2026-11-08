// The mixer with your DAW built in (docs/design/panels §3): every strip says what it plays through; Your DAW card; the DAW & MIDI tab is gone.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.removeItem('ss_mout'); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; spPut({big:true,fold:false}); shellDock('mix'); spFit(); });
 const t0=await p.evaluate(()=>({pts:[...document.querySelectorAll('#cMix .strip[data-ch]')].map(s=>[s.dataset.ch,(s.querySelector('.pt')||{}).textContent||'']),card:!!$('#mixSide .daw-card'),noTab:!DRAWER.sound.p.some(x=>x[0]==='daw')}));
 const parts=t0.pts.filter(x=>['lead','chords','bass','drums'].includes(x[0]));
 ok(parts.length===4&&parts.every(x=>/^Semitone · .+built-in$/.test(x[1])),'every part strip says what it plays through: '+parts.map(x=>x[1]).join(' | '));
 ok(t0.pts.filter(x=>x[0]==='master').every(x=>!x[1]),'the master strip has no “plays through”');
 ok(t0.card&&t0.noTab,'a Your DAW card sits beside the strips, and the DAW & MIDI tab is gone');
 await p.evaluate(()=>shellDock('daw')); ok(await p.evaluate(()=>!document.querySelector('#ss_dock .dock-pane[data-pane="mix"]').hidden),'old links to the DAW tab open the mixer');
 // connect (pretend), send the bass to the DAW
 await p.evaluate(()=>{ MOUT.on=true; Object.values(MOUT.map.song).forEach(r=>r.on=false); MOUT.mute=true; renderStrips('song'); });
 await p.click('#cMix .strip[data-ch="bass"] .pt'); await p.waitForTimeout(60);
 const m=await p.evaluate(()=>({open:!$('#ptMenu').hidden,opts:[...document.querySelectorAll('#ptMenu .pt-opt b')].map(x=>x.textContent)}));
 ok(m.open&&m.opts.includes('Built-in sounds')&&m.opts.includes('Send to your DAW'),'clicking it opens “plays through”: '+m.opts.join(', '));
 await p.selectOption('#ptMenu [data-ptch]','4'); await p.click('#ptMenu [data-ptx="daw"]'); await p.waitForTimeout(60);
 const t1=await p.evaluate(()=>{ const s=document.querySelector('#cMix .strip[data-ch="bass"]'); return {route:MOUT.map.song.bass,cls:s.classList.contains('pt-out'),txt:s.querySelector('.pt').textContent,daw:s.querySelector('.st-fadsec').dataset.daw,after:getComputedStyle(s.querySelector('.st-fadsec'),'::after').content,sw:document.querySelector('[data-dawp="bass"]').checked,other:document.querySelector('#cMix .strip[data-ch="chords"]').classList.contains('pt-out')}; });
 ok(t1.route.on&&t1.route.ch===4&&/→ .+ · Ch 5/.test(t1.txt),'Send to your DAW on Ch 5: '+t1.txt);
 ok(t1.cls&&/mix this part in/.test(t1.after),'the strip is outlined and its fader area says '+t1.after);
 ok(t1.sw&&!t1.other,'the Your DAW card’s switch for Bass is on (shared with the strip); Chords is untouched');
 // the card's switch works the other way too
 await p.click('.daw-row:has([data-dawp="chords"])'); await p.waitForTimeout(60);
 ok(await p.evaluate(()=>MOUT.map.song.chords.on&&document.querySelector('#cMix .strip[data-ch="chords"]').classList.contains('pt-out')),'switching Chords on in the card sends the chords strip to the DAW');
 await p.evaluate(()=>{ MOUT.mute=false; renderStrips('song'); }); ok(await p.evaluate(()=>/built-in · also → Ch/.test(document.querySelector('#cMix .strip[data-ch="chords"] .pt').textContent)&&!document.querySelector('#cMix .strip[data-ch="chords"]').classList.contains('pt-out')),'with “Mute Semitone’s sound” off a part plays here and also goes out');
 // back to built-in from the menu
 await p.evaluate(()=>{ MOUT.mute=true; renderStrips('song'); }); await p.click('#cMix .strip[data-ch="bass"] .pt'); await p.click('#ptMenu [data-ptx="built"]'); await p.waitForTimeout(60);
 ok(await p.evaluate(()=>!MOUT.map.song.bass.on),'choosing Built-in sounds takes it back from the DAW');
 // set-up guide holds the old tab's steps
 await p.evaluate(()=>shellDock('mix')); await p.click('#mixSide [data-dawguide]'); await p.waitForTimeout(60);
 ok(await p.evaluate(()=>{ const g=$('#dawGuide'); return !g.hidden&&!!g.closest('#mixSide')&&/loopMIDI|IAC/.test(g.textContent); }),'Set-up guide opens the FL / loopMIDI / IAC steps beside the mixer');
 // Jamie's mixer features from 8 Nov stay: sidechain and plug-in effects on strips, no Room knob
 const kept=await p.evaluate(()=>({duck:document.querySelectorAll('#cMix .strip [data-duck], #cMix .strip .duck-sel, #cMix .strip select[aria-label*="idechain"]').length,room:document.querySelectorAll('#cMix [data-mx="room"]').length,fx:document.querySelectorAll('#cMix .strip [data-fxslot]').length}));
 ok(kept.duck>0&&kept.room===0&&kept.fx>0,'sidechain menus and effect slots still on the strips, no Room knob '+JSON.stringify(kept));
 await p.evaluate(()=>{ $('#dawGuide [data-dawguide]').click(); MOUT.map.song.chords.on=true; renderStrips('song'); spFit(); mixFit(); }); await p.waitForTimeout(150); await p.screenshot({path:'mixer-daw.png'});
 ok(!errs.length,'no page errors '+errs.slice(0,3).join(' | '));
 await b.close(); if(fails.length){ console.log(fails.length+' failed'); process.exit(1); } })();
