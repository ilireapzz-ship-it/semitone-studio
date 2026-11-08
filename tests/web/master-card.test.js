// The mixer's master card (docs/design/panels/README.md §3 "Master" and "Master card"; destinations in data/mix-master.json):
// knob, Polish, Limiter, Listen on, For, then Loudness and Peak against the destination. One undo step per change.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; spPut({big:true,fold:false}); shellDock('mix'); spFit(); });
 const c0=await p.evaluate(()=>{ const c=document.querySelector('#mixSide .mc-card'); if(!c) return null; const r=c.getBoundingClientRect(), d=document.querySelector('#mixSide .daw-card').getBoundingClientRect(); return {above:r.bottom<=d.top+1,seen:r.width>150&&r.top<900,rows:[...c.querySelectorAll('.mcc-row')].map(x=>x.textContent.trim()),key:c.querySelector('.mc-key').textContent,st:c.querySelector('.mc-st').textContent}; });
 ok(c0&&c0.above&&c0.seen,'the master card sits at the top of the mixer’s side column, above Your DAW');
 ok(c0&&/Polish/.test(c0.rows[0])&&/Limiter −1 dBTP/.test(c0.rows[1])&&/Listen on: Studio/.test(c0.rows[2])&&/For: Streaming/.test(c0.rows[3])&&/green: ideal for streaming/.test(c0.key)&&/red: clips/.test(c0.key)&&/play to measure/.test(c0.st),'it shows Polish, Limiter −1 dBTP, Listen on, For: Streaming and the key line '+JSON.stringify(c0&&c0.rows));
 // the knob is the master volume: drag is one undo step and moves the strip and the transport too
 const k=await p.evaluate(()=>{ const r=document.querySelector('#mixSide [data-mckn]').getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2,n:H.past.length,v:stripVol('song','master')}; });
 await p.mouse.move(k.x,k.y); await p.mouse.down(); await p.mouse.move(k.x,k.y+24,{steps:4}); await p.mouse.up(); await p.waitForTimeout(60);
 const k1=await p.evaluate(n=>({v:stripVol('song','master'),steps:H.past.length-n,strip:+document.querySelector('#cMix .strip[data-ch="master"] [data-mx="vol"]').value,tr:+$('#trVol').value,txt:$('#mixSide [data-mckn]').getAttribute('aria-valuetext')}),k.n);
 ok(k1.v<k.v&&k1.steps===1&&Math.abs(k1.strip-k1.v)<.011&&Math.abs(k1.tr-k1.v)<.011&&/−6\.0 dB/.test(k1.txt),'dragging the master knob down 6 dB is one undo step and moves the master strip and the top bar '+JSON.stringify(k1));
 await p.mouse.dblclick(k.x,k.y); await p.waitForTimeout(40);
 ok(await p.evaluate(()=>Math.abs(stripVol('song','master')-1)<1e-6),'double-click puts the master back to 0 dB');
 // Polish from the card: one step, the Finishing box follows
 const pl=await p.evaluate(async()=>{ const n=H.past.length; document.querySelector('#mixSide [data-mcmenu="polish"]').click(); await new Promise(r=>setTimeout(r,30)); const items=[...document.querySelectorAll('#ctxMenu [data-ci]')].map(x=>x.textContent); [...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>/^Radio/.test(x.textContent.replace('● ',''))).click(); await new Promise(r=>setTimeout(r,30)); return {items,polish:S.polish,steps:H.past.length-n,sel:($('#polishSel')||{}).value,row:document.querySelector('#mixSide [data-mcmenu="polish"]').textContent}; });
 ok(pl.items.length===3&&pl.polish==='radio'&&pl.steps===1&&pl.sel==='radio'&&/radio/.test(pl.row),'Polish from the card: Off, Gentle or Radio, one undo step, and the Finishing box follows '+JSON.stringify(pl));
 const off=await p.evaluate(()=>{ mcPolish('off'); const t=document.querySelector('#mixSide .mcc-row:nth-child(2)').textContent; mcPolish('gentle'); return t; });
 ok(/Limiter off/.test(off),'with Polish off the Limiter row says it is off: '+off);
 // For: the destination moves the green zone and the key line, and saves with the song
 const de=await p.evaluate(async()=>{ const z0=document.querySelector('#mixSide [data-mcbar="lufs"] .mc-z').style.left; document.querySelector('#mixSide [data-mcmenu="dest"]').click(); await new Promise(r=>setTimeout(r,30)); const items=[...document.querySelectorAll('#ctxMenu [data-ci]')]; items.find(x=>/Club/.test(x.textContent)).click(); await new Promise(r=>setTimeout(r,30)); const z1=document.querySelector('#mixSide [data-mcbar="lufs"] .mc-z').style.left; const saved=projectData().mixDest; return {n:items.length,z0,z1,key:document.querySelector('#mixSide .mc-key').textContent,saved,dest:S.mixDest}; });
 ok(de.n===7&&de.z1!==de.z0&&/club/i.test(de.key)&&de.saved==='club','For: seven destinations; Club moves the green zone and the key line, and saves with the song '+JSON.stringify(de));
 ok(await p.evaluate(()=>{ const d=projectData(); d.mixDest='film'; applyProject(d); renderStrips('song'); return S.mixDest==='film'&&/Film/.test(document.querySelector('#mixSide [data-mcmenu="dest"]').textContent); }),'opening a song brings back its destination');
 await p.evaluate(()=>{ S.mixDest='stream'; renderStrips('song'); });
 // the verdict, from what it hears (streaming: −16…−9 LUFS, peaks under −1)
 const vd=await p.evaluate(()=>{ const d=mcDest(); return [mcVerdict({lu:-11,pk:-3},d),mcVerdict({lu:-20,pk:-6},d),mcVerdict({lu:-7,pk:-2},d),mcVerdict({lu:-11,pk:-.3},d)].map(v=>v.c+':'+v.t); });
 ok(/^good:.*on target/.test(vd[0])&&/^warn:.*4\.0 dB under/.test(vd[1])&&/^warn:.*2\.0 dB over/.test(vd[2])&&/^bad:.*peaks over/.test(vd[3]),'the header says on target, how far under or over, or that the peaks are too high '+vd.join(' | '));
 // inside the app or FL the engine sends its loudness; the bars and header follow it
 const nt=await p.evaluate(async()=>{ const k=Math.pow(10,(-12+.691)/10); for(let i=0;i<10;i++) mcNative(k,.5); mcPaint(); const c=document.querySelector('#mixSide .mc-card'); return {st:c.querySelector('.mc-st').textContent,lu:c.querySelector('[data-mcbar="lufs"] .mc-o').textContent,pk:c.querySelector('[data-mcbar="peak"] .mc-o').textContent,w:parseFloat(c.querySelector('[data-mcbar="lufs"] .mc-v').style.width)}; });
 ok(/−12\.0 LUFS · on target/.test(nt.st)&&nt.lu==='−12.0'&&nt.pk==='−6.0'&&Math.abs(nt.w-66.7)<1,'the engine’s loudness drives the bars and the header '+JSON.stringify(nt));
 // the page measures its own playback the same way
 const own=await p.evaluate(async()=>{ ensureAudio(); MC.nat=0; play('song'); await new Promise(r=>setTimeout(r,2200)); const t=document.querySelector('#mixSide .mc-st').textContent, v=parseFloat(document.querySelector('#mixSide [data-mcbar="lufs"] .mc-v').style.width); stopPlay(); return {t,v}; });
 ok(/LUFS/.test(own.t)&&own.v>0,'playing the song measures it: '+own.t);
 // Calibrate's “Then master it” opens the card for the chosen destination
 const cm=await p.evaluate(async()=>{ MX.dest='podcast'; const b=document.createElement('button'); b.dataset.calmaster=''; document.querySelector('#calPane').append(b); b.click(); b.remove(); await new Promise(r=>setTimeout(r,150)); return {dest:S.mixDest,flash:!!document.querySelector('#mixSide .mc-card.mc-flash'),row:document.querySelector('#mixSide [data-mcmenu="dest"]').textContent}; });
 ok(cm.dest==='podcast'&&cm.flash&&/Podcast/.test(cm.row),'Calibrate’s “Then master it” opens the master card set for its destination '+JSON.stringify(cm));
 // keyboard: arrows on the knob are one undo step
 const kb=await p.evaluate(async()=>{ S.mixDest='stream'; renderStrips('song'); const n=H.past.length, el=document.querySelector('#mixSide [data-mckn]'); el.focus(); for(let i=0;i<4;i++) el.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowDown',bubbles:true})); await new Promise(r=>setTimeout(r,600)); return {steps:H.past.length-n,db:+(20*Math.log10(stripVol('song','master'))).toFixed(1)}; });
 ok(kb.steps===1&&kb.db===-2,'four arrow presses on the knob: −2 dB in one undo step '+JSON.stringify(kb));
 await p.screenshot({path:'master-card.png',clip:{x:870,y:270,width:290,height:260}});
 ok(!errs.length,'no page errors '+errs.join(' | '));
 console.log(fails.length?fails.length+' failed':'all passed'); await b.close(); process.exit(fails.length?1:0); })();
