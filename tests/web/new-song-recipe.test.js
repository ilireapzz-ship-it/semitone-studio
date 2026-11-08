// On the new-song page every part of "A song like … produced like … feels … like a … in … at … about … for …" opens its own choices on top.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; homeShow(); $('#hmNew').click(); }); await p.waitForTimeout(400);
 const onTop=q=>p.evaluate(q=>{ const e=document.querySelector(q); if(!e||e.hidden) return false; const R=e.getBoundingClientRect(), t=document.elementFromPoint(R.x+R.width/2,R.y+Math.min(30,R.height/2)); return e.contains(t); },q);
 await p.locator('#startView [data-rc="style"]').click(); await p.waitForTimeout(300); ok(await onTop('#tagView'),'the style / artist name opens the style picker above the page');
 const st=await p.evaluate(()=>{ document.querySelector('#tagView [data-tag^="artist:"]').click(); $('#tagUse').click(); return {open:!$('#startView').hidden,chip:document.querySelector('#startView [data-rc="style"]').textContent.trim(),tags:S.tags}; });
 ok(st.open&&st.tags.some(t=>t.startsWith('artist:'))&&st.tags.map(t=>t.slice(t.indexOf(':')+1)).includes(st.chip),'choosing an artist keeps you on the page and shows their name '+JSON.stringify(st));
 for(const k of ['producer','emotion','vibe','key','bpm','length','purpose']){ await p.evaluate(()=>recipePop()); await p.locator(`#startView [data-rc="${k}"]`).click(); await p.waitForTimeout(250); const n=await p.evaluate(()=>$('#recipePop').querySelectorAll('button,input').length); ok(await onTop('#recipePop')&&n>=2,`"${k}" opens its choices on top (${n})`); }
 const r=await p.evaluate(()=>{ const pick=(k,sel)=>{ recipePop(); document.querySelector(`#startView [data-rc="${k}"]`).click(); $('#recipePop').querySelector(sel).click(); }, txt=k=>document.querySelector(`#startView [data-rc="${k}"]`).textContent.trim();
  pick('emotion','[data-rset="emotion:tender"]'); pick('vibe','[data-rset^="vibe:"]'); pick('key','[data-rset="key:7"]'); pick('key','[data-rset="mode:ionian"]'); const b0=S.bpm; pick('bpm','[data-rset="bpm:+5"]');
  const out={emo:txt('emotion'),vibe:txt('vibe'),key:txt('key'),bpm:txt('bpm'),b0,b1:S.bpm,open:!$('#startView').hidden}; histUndo(); out.undo=S.bpm; return out; });
 ok(r.emo==='tender'&&r.vibe!=='what?'&&/^G (maj|major)/.test(r.key)&&r.b1===r.b0+5&&r.bpm===r.b1+' BPM'&&r.open,'feel, vibe, key, major/minor and tempo change right there and the page shows them '+JSON.stringify(r));
 ok(r.undo===r.b0,'each change is one undo step');
 ok(!errs.length,'no page errors '+errs.join(' | ')); await b.close(); if(fails.length) process.exit(1); })();
