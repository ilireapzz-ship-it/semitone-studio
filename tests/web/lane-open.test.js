// 12 Nov (Jamie: "clicking on an automation on the timeline should open automations").
// The lanes drawn under a track on the timeline open that lane in Automation when clicked (or with Enter).
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; lensShow('mix'); arrRender(); }); await p.waitForTimeout(400);
 const rows=await p.evaluate(()=>[...document.querySelectorAll('.lane-go[data-lanep]')].map(r=>({p:r.dataset.lanep,tg:r.dataset.lanetg,title:r.title,h:r.getBoundingClientRect().height})));
 ok(rows.length>0&&rows.every(r=>r.h>0&&/click to open it in Automation/.test(r.title)),'in Mix every track shows its volume lane on the timeline, and each says it opens Automation ('+rows.length+' lanes)');
 const want=rows.find(r=>r.p==='chords')||rows[0];
 const svg=await p.evaluate(w=>{ const r=document.querySelector('.lane-go[data-lanep="'+w.p+'"][data-lanetg="'+w.tg+'"] svg').getBoundingClientRect(); return {x:r.left+r.width*.4,y:r.top+r.height/2}; },want);
 const before=await p.evaluate(()=>({sel:S.sel,at:JSON.stringify(S.songAt||null),past:H.past.length}));
 await p.mouse.click(svg.x,svg.y); await p.waitForTimeout(500);
 const st=await p.evaluate(()=>({sel:AUT.sel&&AUT.sel.ch+'|'+AUT.sel.tg,pane:(document.querySelector('#ss_dock .dock-pane[data-pane="auto"]')||{hidden:true}).hidden===false,secSel:S.sel,at:JSON.stringify(S.songAt||null),past:H.past.length}));
 ok(st.pane,'clicking a lane on the timeline opens Automation');
 ok(st.sel===want.p+'|'+want.tg,'on that very lane ('+st.sel+')');
 ok(st.secSel===before.sel&&st.at===before.at&&st.past===before.past,'without moving the play marker, the selection or the undo history');
 // the keyboard too
 const other=rows.find(r=>r.p!==want.p)||rows[0];
 await p.evaluate(()=>shellDock('mix')); await p.evaluate(o=>document.querySelector('.lane-go[data-lanep="'+o.p+'"]').focus(),other); await p.keyboard.press('Enter'); await p.waitForTimeout(400);
 ok(await p.evaluate(o=>AUT.sel&&AUT.sel.ch===o.p&&document.querySelector('#ss_dock .dock-pane[data-pane="auto"]').hidden===false,other),'Enter on a lane opens it too');
 ok(await p.evaluate(()=>!S.playing),'and nothing starts playing');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
