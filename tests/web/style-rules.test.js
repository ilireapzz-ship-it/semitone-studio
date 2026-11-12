// 12 Nov: a style rule I added was inserted in the middle of an existing one (".tb,#drTools .check,#drTools .dr-lk{...}"), so the
// piano roll's tool buttons lost their own style and took the new rule's. This guards the result: the tool buttons and the bar's
// checkboxes keep their 26 px inline-flex look, and none of the new helper styles reaches them.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ S.demo=false; shellDock('roll'); setLayer('melody'); const css=[...document.styleSheets].flatMap(s=>{ try{ return [...s.cssRules]; }catch(e){ return []; } }).map(x=>x.selectorText||'').filter(Boolean); const tb=document.querySelector('#drTools .tb')||document.querySelector('.tb'), cs=tb?getComputedStyle(tb):null, grid=$('#tlGridBtn').getBoundingClientRect(), tr=document.querySelector('.song-top.transport').getBoundingClientRect();
   return {rule:css.some(s=>/\.tb\b/.test(s)&&/#drTools \.dr-lk/.test(s)),stray:css.filter(s=>/\.tb\b/.test(s)&&/(roll-empty-hint|lane-go|sec-link|dr-link|chord-hint)/.test(s)),tb:cs?{h:cs.height,d:cs.display,pos:cs.position,op:cs.opacity}:null,gridTop:Math.round(grid.top),trBottom:Math.round(tr.bottom)}; });
 console.log(JSON.stringify(r));
 ok(r.rule,'the tool buttons, the bar’s checkboxes and Scale lock still share their one style rule');
 ok(r.stray.length===0,'none of the new helper styles is attached to the tool buttons ('+(r.stray.join(' | ')||'none')+')');
 ok(r.tb&&r.tb.h==='26px'&&/flex/.test(r.tb.d)&&r.tb.pos!=='absolute'&&r.tb.op==='1','a tool button is 26 px high, laid out in the bar, fully opaque ('+JSON.stringify(r.tb)+')');
 ok(r.gridTop>=r.trBottom,'the timeline’s Grid button sits below the transport bar, not under it ('+r.gridTop+' ≥ '+r.trBottom+')');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
