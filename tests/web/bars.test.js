// 11 Nov (Jamie): octave up/down back next to ⋯; drag items from the ⋯ menu, the tool rail and the Tool Pool onto any bar (editor row, transport,
// top bar, mixer row); move a shortcut between bars or drag it off; Customise this bar re-orders its own sections (FL Studio style); kept between
// sessions; Slice is a blade; and the select tool works in the piano roll (box select from the strip above the notes, and on chords).
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ if(!sessionStorage.getItem('once')){ localStorage.clear(); sessionStorage.setItem('once','1'); } }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; shellDock('roll'); trPick(null); setLayer('melody'); selectSection(1); renderRoll(); }); await p.waitForTimeout(300);
 const box=async sel=>{ const r=await (await p.$(sel)).boundingBox(); return {x:r.x+r.width/2,y:r.y+r.height/2,r}; };
 const drag=async(from,to)=>{ await p.mouse.move(from.x,from.y); await p.mouse.down(); await p.mouse.move(from.x+12,from.y+12,{steps:3}); await p.mouse.move(to.x,to.y,{steps:10}); await p.mouse.up(); await p.waitForTimeout(150); };

 // octave buttons next to ⋯
 const oct=await p.evaluate(()=>{ const T=[...$('#drTools').children], s=T.findIndex(c=>c.classList.contains('sc-slot')), m=T.findIndex(c=>c.id==='rollMoreBtn'); const ks=[...document.querySelectorAll('#drTools .sc-slot .sc-b')].map(b=>b.dataset.sc); return {ks,before:s>=0&&s<m}; });
 ok(oct.ks.join()==='rt:up,rt:down'&&oct.before,'Octave up and Octave down sit on the editor row next to ⋯ '+JSON.stringify(oct));
 const m0=await p.evaluate(()=>{ S.selSet=new Set(rollArr()); return rollArr().map(n=>n.m); }); await p.click('#drTools [data-sc="rt:up"]'); await p.waitForTimeout(100);
 const m1=await p.evaluate(()=>rollArr().map(n=>n.m)); ok(m1.length&&m1.every((m,i)=>m===m0[i]+12),'Octave up moves the selected notes up 12');
 await p.click('#drTools [data-sc="rt:down"]'); await p.waitForTimeout(100); ok(await p.evaluate(m0=>rollArr().map(n=>n.m).every((m,i)=>m===m0[i]),m0),'Octave down brings them back');

 // a ⋯ menu item dragged onto the transport bar
 await p.click('#rollMoreBtn'); await p.waitForTimeout(100); const src=await box('#copyBtn'); const tr=await box('#trRight'); await drag(src,{x:tr.r.x-6,y:tr.y});
 const t1=await p.evaluate(()=>({tr:[...document.querySelectorAll('.transport .sc-slot .sc-b')].map(b=>b.dataset.sc),more:$('#rollMore').hidden,saved:JSON.parse(localStorage.getItem('ss_bars')||'{}')}));
 ok(t1.tr.includes('el:#copyBtn')&&t1.saved.slots&&t1.saved.slots.transport.includes('el:#copyBtn'),'Copy song chart dragged from ⋯ onto the transport bar, and saved '+JSON.stringify(t1.tr));
 const clicked=await p.evaluate(()=>{ let hit=0; const el=$('#copyBtn'), was=el.onclick; el.addEventListener('click',()=>hit++,{once:true}); document.querySelector('.transport [data-sc="el:#copyBtn"]').click(); return hit; });
 ok(clicked===1,'the shortcut does what the ⋯ item does');

 // a rail tool onto the top bar (it stays on the rail too)
 const er=await box('#rollRail [data-tool="erase"]'), hd=await box('.topbar .hd-right'); await drag(er,{x:hd.r.x+4,y:hd.y});
 const t2=await p.evaluate(()=>({hd:[...document.querySelectorAll('.hd-right .sc-slot .sc-b')].map(b=>b.dataset.sc),rail:!!document.querySelector('#rollRail [data-tool="erase"]')}));
 ok(t2.hd.includes('rt:erase')&&t2.rail,'the Erase tool dragged from the rail onto the top bar, and still on the rail '+JSON.stringify(t2));
 await p.click('.hd-right [data-sc="rt:erase"]'); ok(await p.evaluate(()=>S.rollTool==='erase'&&document.querySelector('.hd-right [data-sc="rt:erase"]').getAttribute('aria-pressed')==='true'),'clicking it picks Erase, and it shows as pressed');
 await p.evaluate(()=>rollTool('select'));

 // move between bars, then drag off to remove
 const sc=await box('.transport [data-sc="el:#copyBtn"]'), mo=await box('#rollMoreBtn'); await drag(sc,{x:mo.r.x-4,y:mo.y});
 const t3=await p.evaluate(()=>({tools:[...document.querySelectorAll('#drTools .sc-slot .sc-b')].map(b=>b.dataset.sc),tr:[...document.querySelectorAll('.transport .sc-slot .sc-b')].map(b=>b.dataset.sc)}));
 ok(t3.tools.includes('el:#copyBtn')&&!t3.tr.includes('el:#copyBtn'),'a shortcut moves from one bar to another '+JSON.stringify(t3));
 const sc2=await box('#drTools [data-sc="el:#copyBtn"]'); await drag(sc2,{x:700,y:600});
 ok(await p.evaluate(()=>!document.querySelector('[data-sc="el:#copyBtn"]')),'dragged off the bar, it is taken off');

 // Customise this bar: re-order the transport's own sections; kept after reopening
 await p.evaluate(()=>scCustomise('transport')); await p.waitForTimeout(100);
 const lcd=await box('#trLcd'), back=await box('#songBack'); await p.mouse.move(lcd.x,lcd.y); await p.mouse.down(); await p.mouse.move(back.r.x+2,back.y,{steps:8}); await p.mouse.up(); await p.waitForTimeout(100);
 const o1=await p.evaluate(()=>{ const k=[...document.querySelector('.transport').children].map(c=>c.id||c.className.split(' ')[0]); return {first:k[0],saved:(JSON.parse(localStorage.getItem('ss_bars')).order||{}).transport}; }); await p.evaluate(()=>scDone());
 ok(o1.first==='trLcd'&&Array.isArray(o1.saved),'Customise this bar: the time display moved to the front of the transport bar '+JSON.stringify(o1.first));
 await p.reload(); await p.waitForTimeout(1500);
 const o2=await p.evaluate(()=>({first:document.querySelector('.transport').firstElementChild.id,hd:[...document.querySelectorAll('.hd-right .sc-slot .sc-b')].map(b=>b.dataset.sc)}));
 ok(o2.first==='trLcd'&&o2.hd.includes('rt:erase'),'after reopening, the order and the shortcuts are still yours '+JSON.stringify(o2));
 await p.evaluate(()=>{ SCB=SCB_DEF(); scSave(); });

 // Slice is a blade
 ok(await p.evaluate(()=>!/circle/.test(RAIL_I.slice)),'Slice shows a blade, not scissors');

 // the select tool: box select from the strip above the notes, in the whole-track view and on chords
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; shellDock('roll'); });
 for(const part of ['lead','chords']){ await p.evaluate(pt=>{ trPick(pt,true); selectSection(1); rollTool('select'); S.selSet=new Set(); S.csel=new Set(); renderRoll(); },part); await p.waitForTimeout(250);
   const r=await p.evaluate(()=>{ const c=$('#roll').getBoundingClientRect(), k=c.width/$('#roll').width; return {x:c.left+ROLLW.x0[S.sel]*k+3,x1:c.left+(ROLLW.x0[S.sel]+ROLLW.w[S.sel])*k-3,y:c.top+4,y1:c.bottom-4,whole:rollWhole()}; });
   await p.mouse.move(r.x,r.y); await p.mouse.down(); await p.mouse.move((r.x+r.x1)/2,(r.y+r.y1)/2,{steps:6}); await p.mouse.move(r.x1,r.y1,{steps:6}); await p.mouse.up(); await p.waitForTimeout(150);
   const n=await p.evaluate(pt=>pt==='chords'?S.csel.size:S.selSet.size,part); ok(r.whole&&n>0,`select tool: a box from the top strip selects ${part} in the whole-track view (${n})`); }
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
