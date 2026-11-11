// 12 Nov (Jamie: "make the bus relationship to the mixer and how the routing works more like FL studios because it allows users to plug one mixer
// into multiple other mixers triggering a side-chain into it... keep ours smart"). Click a strip to route from it; ⤷ on other strips sends there
// (several at once, each with a level), Route here only, and Sidechain to this track (ducks on the source's notes, page and app engine).
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const ids=await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; const m=[...document.querySelectorAll('button')].find(x=>x.textContent.trim()==='Mix'&&x.offsetParent&&x.getBoundingClientRect().top<60); if(m) m.click(); const a=rtAddBus(true), c=rtAddBus(true); renderStrips('song'); return [a,c]; }); await p.waitForTimeout(300);
 // pick Chords as the source by clicking an empty part of its strip
 const pick=await p.evaluate(()=>{ const s=document.querySelector('#cMix .strip[data-ch="chords"]'), r=s.getBoundingClientRect(); const pts=[[.5,.35],[.5,.6],[.2,.5],[.8,.45]]; for(const [fx,fy] of pts){ const x=r.left+r.width*fx, y=r.top+r.height*fy, e=document.elementFromPoint(x,y); if(e&&s.contains(e)&&!e.closest('button,input,select,textarea,[role=slider],.sf,.knob,[data-mckn],[data-pkn],canvas')) return {x,y}; } return {x:r.left+r.width/2,y:r.top+8}; });
 await p.mouse.click(pick.x,pick.y); await p.waitForTimeout(250);
 const sel=await p.evaluate(ids=>({src:MXR.src,arrows:ids.map(i=>!!document.querySelector(`#cMix .strip[data-ch="${i}"] [data-mxr]`)),bassSc:!!document.querySelector('#cMix .strip[data-ch="bass"] [data-mxr].sc'),none:!document.querySelector('#cMix .strip[data-ch="chords"] [data-mxr]')}),ids);
 ok(sel.src==='chords'&&sel.arrows.every(Boolean)&&sel.bassSc&&sel.none,'clicking the Chords strip picks it; the other strips show route arrows '+JSON.stringify(sel));
 // send to both buses
 for(const i of ids){ await p.click(`#cMix .strip[data-ch="${i}"] [data-mxr]`); await p.waitForTimeout(250); }
 const two=await p.evaluate(ids=>{ const r=rtOf('chords'), N=rtNative(), L=rtList(), c=N.ch.find(x=>x.ch===nativeFxCh('chords')); return {sends:ids.map(i=>r.sends[i]),out:r.out,nat:ids.map(i=>!!(c&&c.sends.some(s=>s.b===L.findIndex(b=>b.id===i))))}; },ids);
 ok(two.sends.every(x=>x===0)&&two.nat.every(Boolean),'Chords sends to two buses at once, in the app engine too '+JSON.stringify(two));
 // drag the first send's arrow down for its level
 const bx=await (await p.$(`#cMix .strip[data-ch="${ids[0]}"] [data-mxr]`)).boundingBox();
 await p.mouse.move(bx.x+bx.width/2,bx.y+bx.height/2); await p.mouse.down(); await p.mouse.move(bx.x+bx.width/2,bx.y+bx.height/2+40,{steps:6}); await p.mouse.up(); await p.waitForTimeout(300);
 const lv=await p.evaluate(ids=>({db:rtOf('chords').sends[ids[0]],label:H.past[H.past.length-1].label,txt:document.querySelector(`#cMix .strip[data-ch="${ids[0]}"] [data-mxr]`).textContent}),ids);
 ok(lv.db===-10&&/send to/.test(lv.label)&&/10/.test(lv.txt),'dragging the arrow sets the send level, one undo step '+JSON.stringify(lv));
 // right-click: route to the second bus only
 await p.evaluate(i=>{ const b=document.querySelector(`#cMix .strip[data-ch="${i}"] [data-mxr]`), r=b.getBoundingClientRect(); b.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+4,clientY:r.top+4})); },ids[1]);
 const menu=await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.textContent)); ok(menu.includes('Route to this track only'),'right-click a route arrow: Route to this track only '+JSON.stringify(menu));
 await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].find(b=>b.textContent==='Route to this track only').click()); await p.waitForTimeout(300);
 const only=await p.evaluate(()=>{ const r=rtOf('chords'); return {out:r.out,sends:Object.keys(r.sends||{}).filter(k=>r.sends[k]>-60)}; });
 ok(only.out===ids[1]&&!only.sends.length,'Route only: Chords goes to that bus alone '+JSON.stringify(only));
 // sidechain: Bass ducks on every Chords note
 await p.click('#cMix .strip[data-ch="bass"] [data-mxr]'); await p.waitForTimeout(300);
 const sc=await p.evaluate(()=>{ const d=duckOf('bass'), nd=nativeDuck().find(x=>x.ch===nativeFxCh('bass')), bars=barList('song'), B=METERS[S.meter].B; return {src:d&&d.src,list:duckList().includes('bass'),beats:nd?nd.beats.length:0,chordBars:bars.filter(b=>b.ch&&b.ch.pad&&b.ch.pad.length).length}; });
 ok(sc.src==='chords'&&sc.list&&sc.beats===sc.chordBars&&sc.beats>0,'Sidechain to this track: Bass ducks on every Chords hit, in the app engine too '+JSON.stringify(sc));
 const page=await p.evaluate(()=>{ ensureAudio(); if(!master) master=newMaster('song'); getBus('song','bass'); const b=busRec('song','bass'); return !!(b&&b.duck); }); ok(page,'and in the page the Bass strip has its duck stage');
 // the routing is saved and can be undone
 const saved=await p.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())); applyProject(d); return {out:rtOf('chords').out,src:(duckOf('bass')||{}).src}; }); ok(saved.out===ids[1]&&saved.src==='chords','saved with the song '+JSON.stringify(saved));
 await p.keyboard.press('Escape'); ok(await p.evaluate(()=>MXR.src===null&&!document.querySelector('#cMix [data-mxr]')),'Escape stops routing mode');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
