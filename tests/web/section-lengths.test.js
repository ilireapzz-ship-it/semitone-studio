// 12 Nov (Jamie: "where it says arrangement... when you click it it should highlight all the song structure... an editor should pop up where the
// user can edit the exact length they want for each section with a toggle for keeping each section type the same length... when they confirm the
// change, our smart daw should be able to adapt promptly").
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:950}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; });
 const info=await p.evaluate(()=>{ const ch=S.song.sections.map((s,i)=>[s.type,i]).filter(x=>x[0]==='chorus').map(x=>x[1]); return {ch,bars:S.song.sections.map(s=>s.bars)}; });
 ok(info.ch.length>=2,'the song has two choruses '+JSON.stringify(info));
 await p.click('#tlLabel'); await p.waitForTimeout(200);
 const open=await p.evaluate(()=>({shown:!$('#lenEd').hidden,lit:$('#timeline').classList.contains('sec-all'),rows:document.querySelectorAll('#lenEd [data-lenrow]').length,n:S.song.sections.length,same:$('#lenSame').checked,apply:$('#lenEd [data-lenok]').disabled}));
 ok(open.shown&&open.lit&&open.rows===open.n,'clicking Arrangement lights every section and opens the editor with a row each '+JSON.stringify(open));
 ok(open.same&&open.apply,'"Keep each section type the same length" starts on; Apply waits for a change');
 const [c1,c2]=info.ch;
 // with the toggle on, changing one chorus changes them all
 await p.click(`#lenEd [data-lenstep="1"][data-i="${c1}"]`); const both=await p.evaluate(([a,b])=>[+$(`#lenEd input[data-lenb="${a}"]`).value,+$(`#lenEd input[data-lenb="${b}"]`).value],[c1,c2]);
 ok(both[0]===info.bars[c1]+1&&both[1]===info.bars[c2]+1,'with it on, one more bar on Chorus 1 gives every chorus one more '+JSON.stringify(both));
 await p.click('#lenEd [data-lenreset]'); await p.uncheck('#lenSame');
 // the section's added-part and bass notes, a lane, before
 const before=await p.evaluate(c=>{ const B=METERS[S.meter].B, k=secKey(c); S.bassOwn=S.bassOwn||{}; S.bassOwn[k]=[{m:40,start:0,dur:1,vel:.9},{m:43,start:B,dur:1,vel:.9}]; S.auto[c]=S.auto[c]||{}; S.auto[c].lead={vol:[{t:0,v:.2},{t:S.song.sections[c].bars*B,v:.9}]}; return {bars:S.song.sections[c].bars,total:S.song.totalBars}; },c2);
 const tgt=before.bars*2; await p.fill(`#lenEd input[data-lenb="${c2}"]`,String(tgt)); await p.press(`#lenEd input[data-lenb="${c2}"]`,'Tab');
 const mid=await p.evaluate(([a,b])=>[+$(`#lenEd input[data-lenb="${a}"]`).value,+$(`#lenEd input[data-lenb="${b}"]`).value,S.song.sections[b].bars,$('#lenEd .len-total').textContent],[c1,c2]);
 ok(mid[0]===info.bars[c1]&&mid[1]===tgt&&mid[2]===before.bars,'with it off, Chorus 2 alone gets longer, and nothing changes before Apply '+JSON.stringify(mid));
 await p.click('#lenEd [data-lenok]'); await p.waitForTimeout(300);
 const after=await p.evaluate(([a,b])=>{ const B=METERS[S.meter].B, s=S.song.sections; let st=0, starts=s.every(x=>{ const ok=x.startBar===st; st+=x.bars; return ok; }); const v=sectionView(b), bass=S.bassOwn[secKey(b)], lane=S.auto[b].lead.vol;
  return {c1:s[a].bars,c2:s[b].bars,total:S.song.totalBars,starts,chords:v.chords.length,mel:v.melody.some(n=>n.start>=s[b].bars*B/2),bass:bass.length,laneEnd:lane[lane.length-1].t,closed:$('#lenEd').hidden,lit:$('#timeline').classList.contains('sec-all'),
   block:+$(`#timeline .sec[data-i="${b}"]`).style.flexGrow,label:H.undo&&H.undo.length?'':'' ,tl:(()=>{ const t=nativeTimeline(); return t.length; })()}; },[c1,c2]);
 ok(after.c1===info.bars[c1]&&after.c2===tgt&&after.total===before.total+before.bars&&after.starts,'Apply: Chorus 2 is twice as long, Chorus 1 is not, every section after it moves along '+JSON.stringify(after));
 ok(after.chords===tgt&&after.mel,'chords fill every bar and the melody carries on into the new bars');
 ok(after.bass===4,'your own bass notes repeat into the new bars '+after.bass);
 ok(Math.abs(after.laneEnd-tgt*4)<.01,'a lane drawn across the section stretches with it '+after.laneEnd);
 ok(after.closed&&!after.lit&&after.block===tgt,'the editor closes and the timeline block is the new length');
 await p.evaluate(()=>histUndo()); const und=await p.evaluate(c=>[S.song.sections[c].bars,S.song.totalBars],c2);
 ok(und[0]===before.bars&&und[1]===before.total,'one Undo puts it back '+JSON.stringify(und));
 await p.click('#tlLabel'); await p.keyboard.press('Escape'); const esc=await p.evaluate(()=>$('#lenEd').hidden&&!$('#timeline').classList.contains('sec-all')); ok(esc,'Escape closes it without changes');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
