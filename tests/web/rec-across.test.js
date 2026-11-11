// 11 Nov (Jamie: "Record across sections"): a take started in one section keeps recording as the song plays on into the next ones,
// like FL. Each section gets the notes played in it, the timeline marks where the take has been, and the whole take is one undo step.
// With Loop on, Record still loops the one section for several tries.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); S.bpm=240; S.loopOn=false; LIVE.countIn=false; selectSection(1); shellDock('roll'); trPick(null); setLayer('melody'); renderRoll(); });
 const secs=await p.evaluate(()=>S.song.sections.slice(0,3).map(s=>[s.name,s.bars])); const barMs=4*60/240*1000;
 await p.waitForTimeout(400); const undo0=await p.evaluate(()=>{ H.quietUntil=0; histCheck(true); return H.past.length; });
 await p.click('#recOpen'); await p.waitForTimeout(150);
 const tap=async(k,ms=200)=>{ await p.keyboard.down(k); await p.waitForTimeout(ms); await p.keyboard.up(k); await p.waitForTimeout(50); };
 await tap('g'); await tap('h');
 // wait until the playhead is in the next section
 await p.waitForFunction(()=>S.playing&&S.sel===2,null,{timeout:secs[1][1]*barMs+4000}); await p.waitForTimeout(150);
 const mid=await p.evaluate(()=>({rec:LIVE.rec,sel:S.sel,pill:$('#rollRec').textContent,got:[...document.querySelectorAll('#arrRows .rec-got')].map(c=>c.dataset.arrsec),now:[...document.querySelectorAll('#arrRows .rec-now')].map(c=>c.dataset.arrsec)}));
 ok(mid.rec&&mid.sel===2&&mid.pill.includes(secs[2][0]),'the take plays on into the next section and keeps recording '+JSON.stringify(mid));
 ok(mid.got.includes('1')&&mid.now.includes('2'),'the timeline marks where the take has written and where it is now');
 await tap('k'); await tap('j'); await p.keyboard.press(' '); await p.waitForTimeout(500);
 const res=await p.evaluate(()=>({v:matOf(S.song.sections[1]).melody.map(n=>n.m),pc:matOf(S.song.sections[2]).melody.map(n=>n.m),label:H.past[H.past.length-1].label,steps:H.past.length,labels:H.past.map(h=>h.label),marks:document.querySelectorAll('#arrRows .rec-got,#arrRows .rec-now').length,blocks:document.querySelectorAll('#arrRows [data-arrpart="lead"][data-arrsec="2"] rect').length}));
 ok(res.v.join()==='67,69'&&res.pc.join()==='72,71','each section keeps the notes played in it '+JSON.stringify(res));
 ok(res.steps===undo0+1&&res.label===`Played Melody into ${secs[1][0]} and ${secs[2][0]}`&&!res.marks&&res.blocks>0,'one undo step for the whole take, the marks clear and the blocks show the notes '+JSON.stringify({undo0,steps:res.steps,labels:res.labels}));
 await p.evaluate(()=>histUndo()); await p.waitForTimeout(200); const un=await p.evaluate(()=>[matOf(S.song.sections[1]).melody.length,matOf(S.song.sections[2]).melody.length]); ok(un.join()==='0,0','Undo takes the whole take back '+un);
 // bass across sections
 await p.evaluate(()=>{ selectSection(1); trPick('bass',true); renderRoll(); }); await p.click('#recOpen'); await p.waitForTimeout(150); await tap('a');
 await p.waitForFunction(()=>S.playing&&S.sel===2,null,{timeout:secs[1][1]*barMs+4000}); await p.waitForTimeout(150); await tap('f'); await p.keyboard.press(' '); await p.waitForTimeout(500);
 const bs=await p.evaluate(()=>({v:((S.bassOwn||{})[secKey(1)]||[]).length,pc:((S.bassOwn||{})[secKey(2)]||[]).map(n=>n.m),label:H.past[H.past.length-1].label}));
 ok(bs.v>=1&&bs.pc.length===1&&bs.label===`Played Bass into ${secs[1][0]} and ${secs[2][0]}`,'bass records across sections too '+JSON.stringify(bs));
 // Loop on: the section loops, nothing goes into the next one
 await p.evaluate(()=>{ S.loopOn=true; selectSection(1); trPick(null); setLayer('melody'); renderRoll(); }); await p.click('#recOpen'); await p.waitForTimeout(150);
 const lp=await p.evaluate(()=>({mode:S.playMode,across:LIVE.recAcross})); await p.keyboard.press(' '); await p.waitForTimeout(300); await p.evaluate(()=>{ S.loopOn=false; });
 ok(lp.mode==='section'&&!lp.across,'with Loop on, Record loops the one section as before '+JSON.stringify(lp));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
