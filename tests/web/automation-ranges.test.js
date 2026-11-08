// Automation ranges and producer moves (docs/design/automation "Split, move, copy and delete ranges" and "Producers use all of this"):
// split keeps the value, move/copy/duplicate/paste land on bars, scope, delete returns to rest, cut/paste between lanes of the same unit,
// stretch, one undo step each; each producer's pass writes only lanes the grammar allows, makes its macro and respects the length rules.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; S.auto={}; S.autoGen={}; S.emoGen={}; S.smart=false; S.macros=[]; S.autoObj=[]; histCheck(true); shellDock('auto'); });
 await p.waitForTimeout(300);
 // two lanes with moves: chords brightness (a build into the first chorus) and drums volume
 const setup=await p.evaluate(()=>{ const secs=S.song.sections, B=autB(), c=secs.findIndex(s=>s.type==='chorus'); S.sel=c; autAddLane('chords','filter.cutoff'); const br=AUT.sel; autShape('build','section'); autAddLane('drums','vol'); autShape('swell','section'); AUT.sel=br; autRender(); return {c,B,br,a:secs[c].startBar*B,b:(secs[c].startBar+secs[c].bars)*B,end:autEnd()}; });
 const {B}=setup;
 // split keeps the value at the split point
 const t1=await p.evaluate(({a,B})=>{ AUT.scope='all'; AUT.tool='select'; const x=a+B*2+1, L=AUT.sel, v0=autValAt(L.ch,L.tg,x), d0=autValAt('drums','vol',x), h0=H.past.length; autSplit(x); return {v0,v1:autValAt(L.ch,L.tg,x),d0,d1:autValAt('drums','vol',x),steps:H.past.length-h0,pt:(autPts(autSecAt(x).si,L.ch,L.tg)||[]).some(p=>Math.abs(p.t-autSecAt(x).t)<1e-3)}; },setup);
 ok(Math.abs(t1.v0-t1.v1)<1e-3&&Math.abs(t1.d0-t1.d1)<1e-3&&t1.pt&&t1.steps===1,'split adds a point and keeps the value on every lane in scope, one undo step');
 // two splits make a range; the action bar shows
 const t2=await p.evaluate(({a,B})=>{ autSplit(a+B*3); const R=AUT.range; autActBar(); const bar=$('#autAct'); return {R,shown:bar&&!bar.hidden,txt:bar&&bar.textContent}; },setup);
 ok(t2.R&&Math.abs(t2.R.x1-t2.R.x0-(B-1))<1e-6&&t2.shown&&/Split/.test(t2.txt)&&/Copy to every/.test(t2.txt)&&/Back to rest/.test(t2.txt),'two splits make a range, and the action bar shows: '+(t2.txt||'').replace(/\s+/g,' ').slice(0,120));
 // scope: This lane leaves the drums alone when deleting; delete returns to rest
 const t3=await p.evaluate(({a,b,B})=>{ AUT.range={x0:a,x1:a+2*B}; AUT.scope='lane'; const L=AUT.sel, d0=autValAt('drums','vol',a+B), rest=autNorm(L.ch,L.tg,autRest(L.ch,L.tg)), h0=H.past.length; autRangeAct('del'); return {steps:H.past.length-h0, v:autValAt(L.ch,L.tg,a+B), rest, d0, d1:autValAt('drums','vol',a+B), after:autValAt(L.ch,L.tg,a+3*B)}; },setup);
 ok(Math.abs(t3.v-t3.rest)<1e-3&&t3.steps===1,'Delete: the lane returns to its resting value in the range, one undo step');
 ok(Math.abs(t3.d0-t3.d1)<1e-6,'scope "This lane": the drums lane is untouched');
 await p.evaluate(()=>{ histUndo(); });
 // move lands on bars; the old place goes back to rest
 const t4=await p.evaluate(({a,B,c})=>{ AUT.scope='all'; const L=AUT.sel; AUT.range={x0:a,x1:a+2*B}; const src=[.25,.75,1.5,1.9].map(k=>autValAt(L.ch,L.tg,a+k*B)), h0=H.past.length; autRangeMove({kind:'move',start:{...AUT.range},copy:false},{x0:a+4*B,x1:a+6*B}); const dst=[.25,.75,1.5,1.9].map(k=>autValAt(L.ch,L.tg,a+4*B+k*B)); return {src,dst,steps:H.past.length-h0,R:AUT.range,label:H.past[H.past.length-1].label}; },setup);
 ok(t4.steps===1&&t4.R.x0%B===0&&t4.src.every((v,i)=>Math.abs(v-t4.dst[i])<.02),'Move: the moves land 4 bars later on a bar line, the same shape ('+t4.label+')');
 await p.evaluate(()=>{ histUndo(); });
 // copy, paste at the playhead, duplicate: all on bars
 const t5=await p.evaluate(({a,B})=>{ const L=AUT.sel; AUT.range={x0:a,x1:a+2*B}; autRangeAct('copy'); const clip=AUT.clip.lanes.length; const h0=H.past.length; autRangeAct('dup'); const d1=H.past.length-h0, R=AUT.range; const same=[.5,1.5].every(k=>Math.abs(autValAt(L.ch,L.tg,a+k*B)-autValAt(L.ch,L.tg,R.x0+k*B))<.02); S.playFrom=0; S.sel=0; const h1=H.past.length; autRangeAct('paste'); const R2=AUT.range; return {clip,d1,dup:R.x0===a+2*B,same,p1:H.past.length-h1,pasteAt:R2.x0,pasteOk:R2.x0%B===0}; },setup);
 ok(t5.clip===2&&t5.d1===1&&t5.dup&&t5.same,'Copy then Duplicate: the copy lands right after, on the bar, one undo step');
 ok(t5.p1===1&&t5.pasteOk,'Paste at the playhead lands on a bar ('+t5.pasteAt+')');
 // cut/paste between lanes of the same unit (chords volume → bass volume)
 const t6=await p.evaluate(({a,B})=>{ autAddLane('chords','vol'); AUT.scope='lane'; const s=S.sel=autSecAt(a).si; autShape('fade','section'); AUT.range={x0:a,x1:a+2*B}; const v=autValAt('chords','vol',a+B*1.5); autRangeAct('cut'); autAddLane('bass','vol'); const h0=H.past.length; autRangeAct('paste',a+4*B); return {v,got:autValAt('bass','vol',a+4*B+B*1.5),steps:H.past.length-h0,lanes:AUT.clip.lanes.length,ch:AUT.clip.lanes[0].ch,cut:Math.abs(autValAt('chords','vol',a+B*1.5)-autNorm('chords','vol',autRest('chords','vol')))<1e-3}; },setup);
 ok(t6.lanes===1&&t6.cut&&Math.abs(t6.v-t6.got)<.02&&t6.steps===1,'Cut chords volume, paste onto bass volume (same unit), one undo step each');
 // stretch with ⌥: a 2-bar move becomes 4 bars
 const t7=await p.evaluate(({a,B})=>{ AUT.scope='lane'; AUT.sel={ch:'chords',tg:Object.keys(S.auto[autSecAt(a).si].chords).find(k=>/^fx/.test(k))}; const L=AUT.sel; AUT.range={x0:a,x1:a+2*B}; const v=autValAt(L.ch,L.tg,a+B), end=autValAt(L.ch,L.tg,a+2*B-.01), h0=H.past.length; autRangeMove({kind:'stretch',start:{...AUT.range}},{x0:a,x1:a+4*B}); return {v,v2:autValAt(L.ch,L.tg,a+2*B),end,end2:autValAt(L.ch,L.tg,a+4*B-.05),steps:H.past.length-h0,R:AUT.range}; },setup);
 ok(t7.steps===1&&Math.abs(t7.v-t7.v2)<.03&&Math.abs(t7.end-t7.end2)<.04&&t7.R.x1-t7.R.x0===4*B,'⌥-stretch: a 2-bar move becomes 4 bars, same shape at twice the length');
 // Copy to every chorus and Back to rest
 const t8=await p.evaluate(({a,B})=>{ AUT.scope='lane'; const L=AUT.sel; AUT.range={x0:a,x1:a+2*B}; const h0=H.past.length; autRangeAct('type'); const secs=S.song.sections, cs=secs.map((s,i)=>s.type==='chorus'?i:-1).filter(i=>i>=0), vals=cs.map(i=>autValAt(L.ch,L.tg,secs[i].startBar*B+B)); const h1=H.past.length; autRangeAct('rest'); const rest=autNorm(L.ch,L.tg,autRest(L.ch,L.tg)); return {n:cs.length,vals,s1:h1-h0,s2:H.past.length-h1,flat:Math.abs(autValAt(L.ch,L.tg,a+B)-rest)<1e-3}; },setup);
 ok(t8.n>1&&t8.vals.every(v=>Math.abs(v-t8.vals[0])<.02)&&t8.s1===1,'Copy to every Chorus repeats the range in each chorus, one undo step');
 ok(t8.flat&&t8.s2===1,'Back to rest flattens the range to the resting value, one undo step');
 // keys: E splits at the mouse, Escape clears the range
 const t9=await p.evaluate(({a,B})=>{ AUT.tool='select'; AUT.range=null; AUT.split=null; AUT.mx=a+B*5+1; const h0=H.past.length; autKeysRange(new KeyboardEvent('keydown',{key:'e'})); const s=H.past.length-h0; AUT.range={x0:a,x1:a+B}; const esc=new KeyboardEvent('keydown',{key:'Escape'}); autKeysRange(esc); return {s,split:AUT.split,cleared:AUT.range===null}; },setup);
 ok(t9.s===1&&t9.split===setup.a+B*5+1&&t9.cleared,'E splits at the mouse; Escape clears the range');
 // shift-click a section name selects whole sections
 const t10=await p.evaluate(()=>{ AUT.range=null; const rul=$('#autRul'), r=rul.getBoundingClientRect(), s=S.song.sections, B=autB(); const at=i=>autX(s[i].startBar*B+1,r.width)+r.left; const ev=(i)=>rul.dispatchEvent(new PointerEvent('pointerdown',{bubbles:true,button:0,shiftKey:true,clientX:at(i),clientY:r.top+5})); ev(1); ev(2); document.dispatchEvent(new PointerEvent('pointerup',{bubbles:true})); window.dispatchEvent(new PointerEvent('pointerup',{bubbles:true})); const R=AUT.range; return {R,want:[s[1].startBar*B,(s[2].startBar+s[2].bars)*B]}; });
 ok(t10.R&&t10.R.x0===t10.want[0]&&t10.R.x1===t10.want[1],'⇧-click section names selects whole sections');
 await p.screenshot({path:'automation-ranges.png'});
 // ---- producers: every pass writes only lanes the grammar allows, makes its macro, respects the length rules ----
 const allowed=['vol','pan','bpm'], fxOk=/^fx\d\.(width|mix|send|cutoff|drive|low|depth)$/;
 const prods=await p.evaluate(()=>TL.d.producers.map(q=>q.id));
 const D=await p.evaluate(()=>JSON.parse($('#autoMoves').textContent));
 for(const len of ['3min','1min']){
  for(const id of prods){
   const r=await p.evaluate(({id,len})=>{ const base=JSON.parse(JSON.stringify({...projectData(),auto:{},autoObj:undefined,macros:undefined,autoBy:undefined,autoMac:undefined})); if(len==='1min'){ const ty=base.sections.map(x=>x[0]); const keep=[ty.indexOf('verse'),ty.indexOf('pre'),ty.indexOf('chorus')].filter(i=>i>=0); base.sections=keep.map(i=>[base.sections[i][0],4,base.sections[i][2]]); base.energy=keep.map(i=>(base.energy||[])[i]??null); } passGo(base); histCheck(true); passOpen(id); const ms=passMoves().filter(m=>m.auto), on=ms.filter(m=>PASS.on[m.id]); const cls=autLenClass(); const lanes=[]; Object.values(S.auto||{}).forEach(A=>Object.entries(A||{}).forEach(([ch,T])=>Object.entries(T||{}).forEach(([tg,pts])=>{ if(pts&&pts.length) lanes.push(ch+'|'+tg); }))); const mac=(S.macros||[]).map(m=>m.name); const who=lanes.map(k=>autWho(...k.split('|'))).filter(w=>w.pass).length; const items=document.querySelectorAll('#passNotes .pv-move input[data-pm^="am-"]').length, rec=document.querySelectorAll('#passNotes .pv-rec').length; passDiscard(); passGo(base); return {cls,n:ms.length,on:on.length,ids:ms.map(m=>m.id),lanes:[...new Set(lanes)],mac,who,items,rec}; },{id,len});
   const P=D.producers[id]; const bad=r.lanes.filter(k=>{ const tg=k.split('|')[1]; return !(allowed.includes(tg)||fxOk.test(tg)||k.startsWith('macro|')); });
   ok(r.n>0&&!bad.length,`${id} (${r.cls}): ${r.n} automation moves, lanes ${r.lanes.length}${bad.length?' BAD '+bad.join(','):''}`);
   if(P.macro) ok(r.mac.includes(P.macro.name),`${id}: makes its ${P.macro.name} macro (${r.mac.join(',')||'none'})`); else ok(!r.mac.length,`${id}: no macro, on purpose`);
   ok(r.items===r.n,`${id}: every move is one switchable item (${r.items}/${r.n})`);
   if(len==='1min') ok(r.cls==='1min'&&r.n<=4&&!r.ids.some(i=>{ const m=P.moves.find(x=>'am-'+x.id===i); return m&&/\.\.|bars:(8|1\d)|@outro/.test(m.op); }),`${id}: 1-minute song keeps at most 3 moves and no long sweeps (${r.n})`);
   if(len==='3min'&&id==='antonoff') ok(r.who>0&&r.rec>0,'Antonoff-style: its lanes show as the pass (ghosted until Keep) and cards say "Record it yourself"');
  }
 }
 // keep: the pass's lanes stay, marked with the producer, each move one undo step
 const keep=await p.evaluate(()=>{ const base=JSON.parse(JSON.stringify({...projectData(),auto:{},autoObj:undefined,macros:undefined,autoBy:undefined})); passGo(base); histCheck(true); passOpen('antonoff'); const n=passActive().length; H.past.length=0; passKeep(); const h0=0; const by=Object.values(S.autoBy||{}); return {n,steps:H.past.length-h0,by,mac:(S.macros||[]).map(m=>m.name),obj:(S.autoObj||[]).length}; });
 ok(keep.steps>=3&&keep.by.some(w=>/Antonoff-style/.test(w))&&keep.mac.includes('Lift')&&keep.obj>0,`Keep: ${keep.steps} undo steps, lanes marked ${[...new Set(keep.by)].join(', ')}, shapes stay shapes (${keep.obj})`);
 ok(!errs.length,'no page errors '+errs.slice(0,3).join(' | '));
 await b.close(); if(fails.length){ console.log(fails.length+' failed'); process.exit(1); } })();
