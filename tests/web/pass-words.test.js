// 12 Nov, seen on a Rick Rubin-style pass in the real app: three moves were titled "drop:counter@all", "drop:pad@verse", "drop:bass2@all".
// Every part move a producer brings is shown in plain words, for all producers.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ const out={}, keep=PASS.q; Object.keys(XP.roles.producerParts).forEach(id=>{ PASS.q={id,pass:{moves:[]}}; out[id]=xPassMoves().map(m=>m.what); }); PASS.q=keep; return out; });
 const all=Object.values(r).flat(), raw=all.filter(t=>/[:@]/.test(t)||/^(add|drop|thin|layer)\b/.test(t));
 ok(all.length>8,'producers bring part moves ('+all.length+' across '+Object.keys(r).length+' producers)');
 ok(raw.length===0,'none of them shows its code: '+(raw.join(' | ')||'all in words'));
 ok(r.rubin&&/^Leave out the /.test(r.rubin[0])&&/whole song$/.test(r.rubin[0])&&/in the verse$/.test(r.rubin[1]),'the Rubin-style ones read: '+(r.rubin||[]).join(' | '));
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
