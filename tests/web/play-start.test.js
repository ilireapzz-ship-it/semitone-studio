// 11 Nov (Jamie: "Play from the start"): Play no longer starts at whichever section you last clicked to edit. The song position starts at
// the top; the ruler and ⏮ move it; Space and ▶ play from it and Stop comes back there (like FL). ⇧Space still plays the selected section.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; arrRender(); });
 const startSi=()=>p.evaluate(()=>S.playing&&S.PT?{si:S.PT.barAt(0).si,mode:S.playMode}:null);
 const space=async(shift)=>{ await p.evaluate(()=>document.activeElement&&document.activeElement.blur()); await p.keyboard.press(shift?'Shift+Space':'Space'); await p.waitForTimeout(250); };
 // click a block in the Outro to edit it, then Space
 const last=await p.evaluate(()=>S.song.sections.length-1); await p.click(`#arrRows [data-arrpart="lead"][data-arrsec="${last}"]`); await p.waitForTimeout(200);
 await space(); let s=await startSi(); ok(s&&s.si===0&&s.mode==='song','after clicking the Outro to edit it, Space plays the song from the start '+JSON.stringify(s));
 await space(); const after=await p.evaluate(()=>({playing:S.playing,pos:songPos()})); ok(!after.playing&&after.pos.si===0&&after.pos.beat===0,'Stop goes back to where play started '+JSON.stringify(after));
 // ⇧Space plays the selected section
 await p.click(`#arrRows [data-arrpart="lead"][data-arrsec="2"]`); await p.waitForTimeout(150); await space(true); s=await startSi(); ok(s&&s.mode==='section'&&s.si===2,'Shift+Space plays just the selected section '+JSON.stringify(s)); await space();
 // the ruler moves the song position
 const rb=await p.evaluate(()=>{ const c=document.querySelector('#timeline [data-i="3"]').getBoundingClientRect(), r=$('#tlRuler').getBoundingClientRect(); return {x:c.left+4,y:r.top+r.height/2}; });
 await p.mouse.click(rb.x,rb.y); await p.waitForTimeout(400); const at=await p.evaluate(()=>songPos()); await space(); s=await startSi(); await space();
 ok(at.si===3&&s&&s.si===3,'click the ruler, then Space plays from there '+JSON.stringify({at,s}));
 await p.click(`#arrRows [data-arrpart="lead"][data-arrsec="1"]`); await p.waitForTimeout(150); await space(); s=await startSi(); await space(); ok(s&&s.si===3,'clicking a block to edit keeps the song position '+JSON.stringify(s));
 await p.click('#songBack'); await p.waitForTimeout(150); await space(); s=await startSi(); await space(); ok(s&&s.si===0,'⏮ goes back to the start '+JSON.stringify(s));
 // the stopped marker shows the song position, not the selected section
 await p.click(`#arrRows [data-arrpart="lead"][data-arrsec="${last}"]`); await p.waitForTimeout(300); const mk=await p.evaluate(()=>{ const m=$('#arrMark'), c=document.querySelector('#timeline [data-i="0"]').getBoundingClientRect(), a=$('#arr').getBoundingClientRect(); return {left:parseFloat(m.style.left),start:c.left-a.left+$('#arr').scrollLeft,disp:m.style.display}; });
 ok(mk.disp!=='none'&&Math.abs(mk.left-mk.start)<3,'the marker stays at the song position while you edit elsewhere '+JSON.stringify(mk));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
