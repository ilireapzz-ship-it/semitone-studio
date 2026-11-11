// 11 Nov (Jamie: "Part name selects it"): clicking a part's name selects it and opens its editor for the selected section, like clicking one
// of its blocks. The sound line under the name (Electric keys ▾) opens the sound list.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; spPut&&spPut({big:false,fold:false}); selectSection(1); arrRender(); }); await p.waitForTimeout(300);
 const state=()=>p.evaluate(()=>({part:SHELL.part,picked:TSEL.p,pane:[...document.querySelectorAll('#ss_dock .dock-pane')].filter(x=>!x.hidden).map(x=>x.dataset.pane)[0],layer:S.editLayer,pop:!!($('#recipePop')&&!$('#recipePop').hidden&&$('#recipePop').querySelector('#spFind'))}));
 const nameOf=pp=>`#arrRows [data-partpop="${pp}"] b`, soundOf=pp=>`#arrRows [data-partpop="${pp}"] small`;
 await p.click(nameOf('bass')); await p.waitForTimeout(250); let s=await state();
 ok(s.part==='bass'&&s.picked==='bass'&&s.pane==='roll'&&s.layer==='part'&&!s.pop,'clicking Bass selects it and opens its notes '+JSON.stringify(s));
 await p.click(nameOf('drums')); await p.waitForTimeout(250); s=await state(); ok(s.picked==='drums'&&s.pane==='beat'&&!s.pop,'clicking Drums opens its beat '+JSON.stringify(s));
 await p.click(nameOf('chords')); await p.waitForTimeout(250); s=await state(); ok(s.picked==='chords'&&s.pane==='roll'&&s.layer==='chords'&&!s.pop,'clicking Chords opens its chords '+JSON.stringify(s));
 await p.click(nameOf('lead')); await p.waitForTimeout(250); s=await state(); ok(s.picked==='lead'&&s.pane==='roll'&&s.layer==='melody'&&!s.pop,'clicking Melody opens the melody '+JSON.stringify(s));
 await p.click(soundOf('chords')); await p.waitForTimeout(300); s=await state(); ok(s.pop,'clicking the sound under the name opens the sound list '+JSON.stringify(s));
 // the sound button is easy to hit (Jamie: 'make it bigger'), and in the compact timeline a ▾ beside the name does it
 const big=await p.evaluate(()=>{ $('#recipePop').hidden=true; const r=document.querySelector('#arrRows [data-partpop="chords"] small').getBoundingClientRect(); return {h:Math.round(r.height),w:Math.round(r.width)}; }); ok(big.h>=20&&big.w>=60,'the sound under the name is a button you can hit '+JSON.stringify(big));
 await p.evaluate(()=>{ spPut({big:true,fold:false}); arrRender(); }); await p.waitForTimeout(300);
 const cp=await p.evaluate(()=>{ const n=document.querySelector('#arrRows [data-partpop="bass"]'), sm=n.querySelector('small').getBoundingClientRect(), b=n.querySelector('b').getBoundingClientRect(); return {w:Math.round(sm.width),h:Math.round(sm.height),beside:sm.left>=b.right-1&&Math.abs((sm.top+sm.height/2)-(b.top+b.height/2))<6,x:sm.left+sm.width/2,y:sm.top+sm.height/2,bx:b.left+4,by:b.top+b.height/2}; });
 ok(cp.w>=20&&cp.h>=18&&cp.beside,'compact timeline: a ▾ beside the name '+JSON.stringify(cp));
 await p.mouse.click(cp.x,cp.y); await p.waitForTimeout(300); s=await state(); ok(s.pop,'compact: the ▾ opens the sound list '+JSON.stringify(s)); await p.evaluate(()=>{ $('#recipePop').hidden=true; });
 await p.mouse.click(cp.bx,cp.by); await p.waitForTimeout(300); s=await state(); ok(s.picked==='bass'&&!s.pop,'compact: the name selects the part '+JSON.stringify(s));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
