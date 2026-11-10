// 11 Nov (Jamie): the compressor shows as a chip in the strip's chain and can be dragged to a new place, with the inserts; lanes follow their effect;
// compressors start off; a + after the strips adds a blank bus; the Transitions strip takes plug-ins; Fill in has room for its words; a new EQ starts flat with five points.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:1000}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; shellDock('mix'); });
 // compressors start off; two inserts on the chords
 const c0=await p.evaluate(()=>{ const st=strip('song','chords'); st.fx=[{type:'echo',on:true,p:{}},{type:'chorus',on:true,p:{}},null].map(f=>f&&FX[f.type]?{...f,p:Object.fromEntries(FX[f.type].ps.map(d=>[d.k,d.v]))}:null); histCheck(true); renderStrips('song'); const chips=[...document.querySelectorAll('#cMix .strip[data-ch="chords"] .st-fx [data-chainitem]')].map(x=>x.dataset.chainitem); return {chips,on:pcOf('chords').on,txt:document.querySelector('#cMix .strip[data-ch="chords"] .comp-chip').textContent}; });
 ok(c0.chips.join()==='c,0,1'&&!c0.on&&/off/.test(c0.txt),'the compressor is a chip in the chain, first after the EQ, and shows off at 0 '+JSON.stringify(c0));
 // drag the compressor below the second insert
 const pos=await p.evaluate(()=>{ const q=s=>document.querySelector('#cMix .strip[data-ch="chords"] '+s).getBoundingClientRect(); const a=q('[data-chainitem="c"]'), z=q('[data-chainitem="1"]'); return {x:a.x+a.width/2,y:a.y+a.height/2,y2:z.y+z.height+4}; });
 await p.evaluate(()=>{ autoSec('song',0,true); const A=autoSec('song',0,true); A.chords={'fx1.mix':[{t:0,v:.5}]}; histCheck(true); });
 const h0=await p.evaluate(()=>H.past.length);
 await p.mouse.move(pos.x,pos.y); await p.mouse.down(); await p.mouse.move(pos.x,pos.y+8,{steps:3}); await p.mouse.move(pos.x,pos.y2,{steps:8}); await p.mouse.up(); await p.waitForTimeout(150);
 const c1=await p.evaluate(h0=>({chips:[...document.querySelectorAll('#cMix .strip[data-ch="chords"] .st-fx [data-chainitem]')].map(x=>x.dataset.chainitem),at:strip('song','chords').compAt,steps:H.past.length-h0,label:H.past[H.past.length-1].label}),h0);
 ok(c1.chips.join()==='0,1,c'&&c1.at===2&&c1.steps===1,'dragged below the second insert, the compressor runs after both, one undo step '+JSON.stringify(c1));
 // move an insert above the other: lanes follow
 const pos2=await p.evaluate(()=>{ const q=s=>document.querySelector('#cMix .strip[data-ch="chords"] '+s).getBoundingClientRect(); const a=q('[data-chainitem="1"]'), z=q('[data-chainitem="0"]'); return {x:a.x+a.width/2,y:a.y+a.height/2,y2:z.y+2}; });
 await p.mouse.move(pos2.x,pos2.y); await p.mouse.down(); await p.mouse.move(pos2.x,pos2.y-8,{steps:3}); await p.mouse.move(pos2.x,pos2.y2,{steps:8}); await p.mouse.up(); await p.waitForTimeout(150);
 const c2=await p.evaluate(()=>({fx:strip('song','chords').fx.map(f=>f&&f.type),lanes:Object.keys((autoSec('song',0,false)||{}).chords||{})}));
 ok(c2.fx[0]==='chorus'&&c2.fx[1]==='echo'&&c2.lanes.includes('fx0.mix'),'moving an insert reorders the chain and its automation lane follows it '+JSON.stringify(c2));
 // the page audio and the engine both get the order
 const nat=await p.evaluate(()=>{ pcSet('chords',c=>{ c.amt=.4; },'Comp',true); const d=nativeFx().find(f=>f.type==='pcomp'&&f.ch===nativeFxCh('chords')); return d&&d.p.at; });
 ok(nat===2,'the engine is told where the compressor sits ('+nat+')');
 // undo puts the chain back
 await p.evaluate(()=>{ histUndo(); histUndo(); histUndo(); renderStrips('song'); });
 ok(await p.evaluate(()=>!strip('song','chords').compAt),'undo puts the compressor back in its place');
 // clicking the chip opens the compressor
 await p.evaluate(()=>renderStrips('song')); await p.click('#cMix .strip[data-ch="chords"] .comp-chip'); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>!!(PCX.win&&PCX.win.ch==='chords')),'clicking the Comp chip opens that compressor');
 await p.evaluate(()=>{ if(typeof pcClose==='function') pcClose(); });
 // + adds a blank bus
 const nb=await p.evaluate(()=>{ renderStrips('song'); const n=rtList().filter(b=>b.k==='bus').length; document.querySelector('#cMix [data-rtaddbus]').click(); return {before:n,after:rtList().filter(b=>b.k==='bus').length}; });
 ok(nb.after===nb.before+1,'+ New bus after the strips adds a blank bus '+JSON.stringify(nb));
 // Transitions take plug-ins (in the app)
 ok(await p.evaluate(()=>pfxOK('fx')&&hostFxKeys().includes('fx')&&hostFxTarget('fx')===transCh()),'the Transitions strip can take plug-in effects like the others');
 // Fill in has room
 const fw=await p.evaluate(()=>{ shellDock('roll'); const e=$('#drFill'); return e.scrollWidth<=e.clientWidth+1&&e.getBoundingClientRect().width>40; });
 ok(fw,'Fill in has room for its words');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
