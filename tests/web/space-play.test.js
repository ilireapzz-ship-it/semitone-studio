// 12 Nov (Jamie: "when i click on loop and press spacebar to play... it gets stuck toggling loop on and off instead of playing"): Space always
// plays and stops, like FL, even when the last thing clicked was a button. Typing in a box still types.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('roll'); trPick(null); setLayer('melody'); renderRoll(); });
 const loopBtn=p.locator('label.check:has(#loopOn)').filter({visible:true}).first();
 await loopBtn.click(); await p.waitForTimeout(150); const l0=await p.evaluate(()=>!!S.loopOn);
 await p.keyboard.press('Space'); await p.waitForTimeout(300); const a=await p.evaluate(()=>({playing:S.playing,loop:!!S.loopOn}));
 ok(a.playing&&a.loop===l0,'after clicking Loop, Space plays and leaves Loop as it was '+JSON.stringify({l0,a}));
 await p.keyboard.press('Space'); await p.waitForTimeout(200); const c=await p.evaluate(()=>({playing:S.playing,loop:!!S.loopOn}));
 ok(!c.playing&&c.loop===l0,'Space again stops '+JSON.stringify(c));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
