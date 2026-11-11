// 12 Nov, from the gap report: (1) a kept producer move took the drums out of every section, and the Drums strip in the mixer showed
// no sign of it; (2) the pass said "Keep 10 changes" and kept 4. The strip's M now shows a part that is out of sections, and Keep
// counts only the moves that change something.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const m=()=>p.evaluate(()=>{ const b=document.querySelector('#cMix .strip[data-ch="drums"] button[data-ms="m"]'); return b?{secm:b.dataset.secm||'',title:b.title,pressed:b.getAttribute('aria-pressed')}:null; });
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('mix'); renderStrips('song'); });
 const m0=await m(); ok(m0&&m0.secm===''&&m0.title==='Mute','a part that plays everywhere has a plain M');
 // take the drums out of section 2 the way the timeline menu does
 await p.evaluate(()=>{ clipRemove('drums',1); }); await p.waitForTimeout(300);
 const m1=await m(); ok(m1.secm==='here'&&/taken out of this section: 1 of/.test(m1.title)&&m1.pressed==='false','out of the section you are on: the M is hatched and says so (“'+m1.title.slice(0,60)+'…”)');
 await p.evaluate(()=>selectSection(2)); await p.waitForTimeout(200);
 const m2=await m(); ok(m2.secm==='some'&&/other sections/.test(m2.title),'on another section it is underlined and still says where');
 await p.evaluate(()=>{ selectSection(1); clipRemove('drums',1); }); await p.waitForTimeout(300);
 const m3=await m(); ok(m3.secm===''&&m3.title==='Mute','Put back clears it');
 // the pass: Keep counts what will change
 const ps=await p.evaluate(()=>{ const q=TL.d.producers.find(x=>/rubin/i.test(x.id))||TL.d.producers[0]; passOpen?passOpen(q.id):0; return {id:q.id}; }).catch(e=>({err:String(e)}));
 const ks=await p.evaluate(()=>{ if(PASS.A===null) return null; const live=passMoves().filter(m=>!m.soon&&!m.quiet&&!m.out&&PASS.on[m.id]), base=passFinger(JSON.parse(PASS.A)), same=live.filter(m=>passSame([m],base)).length, btn=$('#passKeep').textContent; return {live:live.length,same,btn,note:$('#trNote').textContent}; });
 if(ks){ console.log(JSON.stringify(ks)); const said=+(/Keep (\d+)/.exec(ks.btn)||[0,-1])[1]; ok(said>0&&said<=ks.live-ks.same,'Keep counts only moves that change something: '+said+' of the '+ks.live+' switched on (“'+ks.btn+'”)');
   const kept=await p.evaluate(()=>{ const n0=H.past.length; passKeep(); return H.past.length-n0; }); ok(kept===said,'and keeping makes exactly that many undo steps ('+kept+')'); }
 else ok(false,'the pass opened '+JSON.stringify(ps));
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
