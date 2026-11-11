// 12 Nov, from the gap report ("the editor stays on what you are editing"): after playing the song, the editor stayed on whatever
// section the song had reached, so notes went into the wrong section. With Follow off (the default) it comes back to the section you
// were working on, with its play marker; Follow on keeps the old behaviour; picking a section yourself while it plays always wins.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); setPlayFrom(2); S.songAt={si:3,beat:0}; });
 const btn=await p.evaluate(()=>({there:!!$('#followBtn'),pressed:$('#followBtn')&&$('#followBtn').getAttribute('aria-pressed'),title:$('#followBtn')&&$('#followBtn').title}));
 ok(btn.there&&btn.pressed==='false'&&/comes back/.test(btn.title),'the Follow button is beside Play and starts off');
 // the song position is in section 4; you are working on section 2
 await p.evaluate(()=>{ S.songAt={si:3,beat:0}; }); await p.keyboard.press('Space'); await p.waitForTimeout(700);
 const playing=await p.evaluate(()=>({playing:S.playing,sel:S.sel}));
 ok(playing.playing&&playing.sel===3,'while the song plays the editor shows the section that is sounding (section '+(playing.sel+1)+')');
 await p.keyboard.press('Space'); await p.waitForTimeout(400);
 const back=await p.evaluate(()=>({playing:S.playing,sel:S.sel,pf:S.playFrom,at:S.songAt}));
 ok(!back.playing&&back.sel===1,'Stop brings the editor back to the section you were working on (section '+(back.sel+1)+')');
 ok(back.pf===2,'with its play marker where you left it (beat '+back.pf+')');
 ok(back.at&&back.at.si===3,'and the song position stays where it was');
 // picking a section yourself while it plays wins
 await p.keyboard.press('Space'); await p.waitForTimeout(500); await p.evaluate(()=>selectSection(2)); await p.waitForTimeout(200);
 const st=await p.evaluate(()=>S.playing); if(st){ await p.keyboard.press('Space'); await p.waitForTimeout(400); }
 ok(await p.evaluate(()=>S.sel)===2,'a section you pick yourself while it plays is where you stay');
 // Follow on: the old behaviour
 await p.evaluate(()=>{ selectSection(1); S.songAt={si:3,beat:0}; }); await p.click('#followBtn'); await p.waitForTimeout(200);
 const on=await p.evaluate(()=>({pressed:$('#followBtn').getAttribute('aria-pressed'),ls:localStorage.getItem('ss_follow')}));
 ok(on.pressed==='true'&&on.ls==='1','clicking the button turns Follow on and remembers it');
 await p.keyboard.press('Space'); await p.waitForTimeout(600); await p.keyboard.press('Space'); await p.waitForTimeout(400);
 ok(await p.evaluate(()=>S.sel)===3,'with Follow on the editor stays where the song stopped');
 await p.click('#followBtn'); ok(await p.evaluate(()=>$('#followBtn').getAttribute('aria-pressed'))==='false','and it turns off again');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
