// 12 Nov, from the gap report: a chord could not be made longer (dragging its edge did nothing) and an empty chord lane gave no hint.
// Dragging a chord's right edge carries it into the bars you cross (one undo step); an empty chord lane says how to start;
// dragging the middle of a chord note still moves its pitch.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); const si=S.song.sections.findIndex(s=>s.type==='verse'); selectSection(si); shellDock('roll'); setLayer('chords'); renderRoll(); }); await p.waitForTimeout(300);
 const hint=()=>p.evaluate(()=>{ const e=$('#chordHint'); return e?{shown:!e.hidden&&e.getBoundingClientRect().height>0,t:e.textContent}:null; });
 const h0=await hint(); ok(h0&&h0.shown&&/Double-click a row/.test(h0.t)&&/right edge/.test(h0.t),'an empty chord lane says how to start: “'+(h0?h0.t.slice(0,70):'')+'…”');
 const at=(m,beat)=>p.evaluate(([m,beat])=>{ const L=S.rl, r=$('#roll').getBoundingClientRect(); return {x:r.left+beat*(L.W/L.TB)+5,y:r.top+L.top+(L.hi-m)*L.rh+L.rh/2}; },[m,beat]);
 for(const m of [55,58,62]){ const q=await at(m,.5); await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(280); }
 const syms=()=>p.evaluate(()=>matOfSel().chords.map(c=>c.rest?'-':c.sym).join(' '));
 const h1=await hint(); ok(h1&&!h1.shown,'the hint goes once there is a chord'); ok(/^Gm - - -/.test(await syms()),'a G minor chord is in bar 1 ('+await syms()+')');
 // the right edge of the chord
 const edge=await p.evaluate(()=>{ const R=$('#roll').getBoundingClientRect(), q=S.crects.find(r=>r.b===0&&r.m===58); return {x:R.left+q.x+q.w-3,y:R.top+q.y+q.h/2,w:q.w,mid:R.left+q.x+q.w/2}; });
 await p.mouse.move(edge.x,edge.y); await p.waitForTimeout(80);
 ok(await p.evaluate(()=>$('#roll').style.cursor)==='ew-resize','the pointer shows the chord’s right edge can be dragged');
 const past0=await p.evaluate(()=>H.past.length);
 await p.mouse.down(); await p.mouse.move(edge.x+edge.w*1.2,edge.y,{steps:4}); await p.mouse.move(edge.x+edge.w*2.9,edge.y,{steps:5}); await p.mouse.up(); await p.waitForTimeout(400);
 const s2=await syms(), st=await p.evaluate(()=>({past:H.past.length,label:H.past[H.past.length-1].label,pads:matOfSel().chords.slice(0,5).map(c=>(c.pad||[]).join(','))}));
 ok(/^Gm Gm Gm Gm -/.test(s2),'dragging the edge across three more bars carries the chord into them ('+s2+')');
 ok(st.pads[1]===st.pads[0]&&st.pads[3]===st.pads[0],'with the same notes in each bar');
 ok(st.past===past0+1&&/Gm carried over 4 bars/.test(st.label),'as one undo step (“'+st.label+'”)');
 await p.keyboard.press('Control+z'); await p.waitForTimeout(350); ok(/^Gm - - -/.test(await syms()),'Undo takes the whole carry back ('+await syms()+')');
 // the middle of a chord note still moves its pitch
 const mid=await p.evaluate(()=>{ const R=$('#roll').getBoundingClientRect(), q=S.crects.find(r=>r.b===0&&r.m===62); return {x:R.left+q.x+q.w/2,y:R.top+q.y+q.h/2,rh:S.rl.rh}; });
 await p.mouse.move(mid.x,mid.y); await p.mouse.down(); await p.mouse.move(mid.x,mid.y-mid.rh*1.2,{steps:4}); await p.mouse.move(mid.x,mid.y-mid.rh*2.1,{steps:4}); await p.mouse.up(); await p.waitForTimeout(350);
 const pad=await p.evaluate(()=>matOfSel().chords[0].pad.join(','));
 ok(pad==='55,58,64'&&(await syms()).split(' ')[1]==='-','dragging the middle of a note still moves that note, and carries nothing ('+pad+')');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
