// 12 Nov (Jamie: "a right click and drag on the timeline meter to select a looping area.... and that highlights red exactly how it does on FL
// studio. should be executable from both the piano roll and the timeline"). Right-drag marks whole bars in red; Space plays them round and round;
// a right-click without a drag clears it.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('roll'); trPick(null); setLayer('melody'); renderRoll(); arrRender(); });
 await p.waitForTimeout(300);
 // the timeline ruler: right-drag from the start of Verse 1 to the middle of the Chorus
 const pts=await p.evaluate(()=>{ const r=$('#tlRuler').getBoundingClientRect(), c1=document.querySelector('#timeline [data-i="1"]').getBoundingClientRect(), c2=document.querySelector('#timeline [data-i="2"]').getBoundingClientRect(); return {y:r.top+r.height/2,x0:c1.left+3,x1:c2.left+c2.width*.5}; });
 await p.mouse.move(pts.x0,pts.y); await p.mouse.down({button:'right'}); await p.mouse.move((pts.x0+pts.x1)/2,pts.y,{steps:4}); await p.mouse.move(pts.x1,pts.y,{steps:4}); await p.mouse.up({button:'right'}); await p.waitForTimeout(300);
 const sel=await p.evaluate(()=>{ const el=$('#lselTl'), cs=el&&getComputedStyle(el); return {L:S.loopSel,want:[S.song.sections[1].startBar,S.song.sections[2].startBar+Math.ceil(S.song.sections[2].bars/2)],shown:!!el&&!el.hidden&&el.getBoundingClientRect().width>20,red:cs&&/229, 72, 77|e5484d/i.test(cs.borderLeftColor),menu:!!$('#ctxMenu')&&!$('#ctxMenu').hidden}; });
 ok(sel.L&&sel.L.a===sel.want[0]&&Math.abs(sel.L.b-sel.want[1])<=1&&sel.shown&&sel.red&&!sel.menu,'right-dragging the ruler marks those bars in red (no menu pops up) '+JSON.stringify(sel));
 // Space plays the loop area round and round
 await p.evaluate(()=>document.activeElement&&document.activeElement.blur()); await p.keyboard.press('Space'); await p.waitForTimeout(300);
 const pl=await p.evaluate(()=>{ const L=S.loopSel, P=S.PT, n=L.b-L.a, first=P.barAt(0), wrap=P.barAt(n), all=barList('song'); return {mode:S.playMode,firstSi:first.si,firstB:first.b,want:[all[L.a].si,all[L.a].b],wrapSame:wrap.si===first.si&&wrap.b===first.b}; });
 ok(pl.mode==='region'&&pl.firstSi===pl.want[0]&&pl.firstB===pl.want[1]&&pl.wrapSame,'Space plays from the start of the loop area and goes round again '+JSON.stringify(pl));
 await p.keyboard.press('Space'); await p.waitForTimeout(200);
 // the strip above the roll does it too
 const rp=await p.evaluate(()=>{ const cv=$('#roll'), r=cv.getBoundingClientRect(), c=$('#cLane').getBoundingClientRect(); return {y:c.top+c.height/2,x0:r.left+S.rl.W*.02,x1:r.left+S.rl.W*.55}; });
 await p.mouse.move(rp.x0,rp.y); await p.mouse.down({button:'right'}); await p.mouse.move(rp.x1,rp.y,{steps:6}); await p.mouse.up({button:'right'}); await p.waitForTimeout(300);
 const rs=await p.evaluate(()=>{ const el=$('#lselRoll'); return {L:S.loopSel,sec:[S.song.sections[S.sel].startBar,S.song.sections[S.sel].bars],shown:!!el&&!el.hidden&&el.getBoundingClientRect().width>20}; });
 ok(rs.L&&rs.L.a===rs.sec[0]&&rs.L.b>rs.L.a&&rs.L.b<=rs.sec[0]+rs.sec[1]&&rs.shown,'right-dragging above the piano roll marks a loop area there too '+JSON.stringify(rs));
 // a plain right-click on the ruler clears it
 await p.mouse.click(pts.x0,pts.y,{button:'right'}); await p.waitForTimeout(300);
 const cl=await p.evaluate(()=>({L:S.loopSel,hidden:!$('#lselTl')||$('#lselTl').hidden}));
 ok(!cl.L&&cl.hidden,'a right-click without dragging clears it '+JSON.stringify(cl));
 await p.evaluate(()=>document.activeElement&&document.activeElement.blur()); await p.keyboard.press('Space'); await p.waitForTimeout(250); const back=await p.evaluate(()=>S.playMode); await p.keyboard.press('Space');
 ok(back==='song','then Space plays the song again '+back);
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
