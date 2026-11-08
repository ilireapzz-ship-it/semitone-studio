// Track names never overlap their buttons, and the piano roll's tool bar can show or hide articulation marks.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; shellDock('roll'); xAdd('counter'); xAdd('arp'); });
 const clash=async()=>p.evaluate(()=>{ const hit=(a,b)=>a.width&&b.width&&a.left<b.right-1&&b.left<a.right-1&&a.top<b.bottom-1&&b.top<a.bottom-1, bad=[];
  document.querySelectorAll('#arrRows .arr-row.track .tr-h:not(.au-h)').forEach(h=>{ const box=h.getBoundingClientRect(), parts=[h.querySelector('.tr-n b'),h.querySelector('.tr-n small'),...h.querySelectorAll('.tr-ms > *'),h.querySelector('.tr-kn svg'),h.querySelector('.tr-kn output'),...h.querySelectorAll('.tr-corner button')].filter(e=>e&&getComputedStyle(e).display!=='none').map(e=>[e,e.getBoundingClientRect()]);
   parts.forEach(([e,r],i)=>{ if(r.width&&(r.bottom>box.bottom+1||r.top<box.top-1||r.right>box.right+1)) bad.push('outside:'+e.textContent.trim().slice(0,12)); parts.slice(i+1).forEach(([f,q])=>{ if(hit(r,q)) bad.push(e.textContent.trim().slice(0,12)+'×'+f.textContent.trim().slice(0,12)); }); }); }); return bad; });
 for(const [h,label] of [[.42,'tall rows'],[.55,'medium rows'],[.66,'short rows']]){
  await p.evaluate(h=>{ spPut({big:false,fold:false,h:Math.round(innerHeight*h)}); spApply(); },h); await p.waitForTimeout(200);
  const bad=await clash(); ok(!bad.length,`track names, tags, M S, knob and fold/delete don't overlap (${label}) `+JSON.stringify(bad.slice(0,6)));
 }
 const tag=await p.evaluate(()=>{ const r=[...document.querySelectorAll('#arrRows .tr-h')].find(h=>/Counter/.test(h.textContent)); return {inName:!!r.querySelector('.tr-n .tr-tag'),inRow:!!r.querySelector('.tr-ms .tr-tag')}; });
 ok(!tag.inName&&tag.inRow,'the New tag sits with M and S, not inside the name '+JSON.stringify(tag));
 // the articulation marks button
 await p.evaluate(()=>{ SHELL.part='lead'; setLayer('melody'); selectSection(1); const s=$('#cSmartArt'); if(s) s.click(); }); await p.waitForTimeout(200);
 const st=await p.evaluate(()=>({btn:!!$('#rollRail #artShowBtn'),on:$('#artShowBtn').getAttribute('aria-pressed'),after:RAIL.L.indexOf('marks')===RAIL.L.indexOf('art')+1}));
 ok(st.btn&&st.on==='true'&&st.after,'the roll tool bar has a Show articulations button, on by default, next to Articulation mode '+JSON.stringify(st));
 const ink=()=>p.evaluate(()=>{ renderRoll(); return $('#roll').toDataURL().length; });
 const n0=await ink(), h0=await p.evaluate(()=>H.past.length); await p.click('#artShowBtn'); await p.waitForTimeout(200); const n1=await ink();
 const off=await p.evaluate(()=>({hide:S.artHide,pressed:$('#artShowBtn').getAttribute('aria-pressed'),saved:localStorage.getItem('ss_arthide'),steps:H.past.length}));
 ok(off.hide&&off.pressed==='false'&&off.saved==='1'&&n1<n0,'clicking it hides the marks and remembers that '+JSON.stringify({...off,n0,n1}));
 ok(off.steps===h0,'showing or hiding marks is not an undo step and does not change the song');
 await p.click('#artShowBtn'); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>!S.artHide)&&await ink()===n0,'clicking again brings the marks back');
 // a saved tool bar from before gets the new button once
 await p.evaluate(()=>{ localStorage.setItem('ss_rail',JSON.stringify(['select','art','draw'])); localStorage.removeItem('ss_rail_marks'); }); await p.reload(); await p.waitForTimeout(1200);
 ok(await p.evaluate(()=>RAIL.L.join()==='select,art,marks,draw'),'an older saved tool bar gets the button next to Articulation mode');
 await p.evaluate(()=>railSave(['select','draw'])); await p.reload(); await p.waitForTimeout(1200);
 ok(await p.evaluate(()=>!RAIL.L.includes('marks')),'once removed to the Tool Pool, it stays removed');
 ok(!errs.length,'no page errors '+errs.join(' | '));
 console.log(fails.length?fails.length+' failed':'all passed'); await b.close(); process.exit(fails.length?1:0); })();
