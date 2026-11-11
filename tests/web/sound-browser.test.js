// 12 Nov (Jamie: "why is the sound tab nearly empty and virtually useless?"): the Sound tab opens on a browser: a tab per part with its sound,
// a search box, every sound that part can use as tiles (▶ hears it, a click uses it, one undo step).
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('sound'); H.quietUntil=0; histCheck(true); }); await p.waitForTimeout(300);
 const s0=await p.evaluate(()=>({tabs:[...document.querySelectorAll('#sndBrowse [data-sbp]')].map(b=>b.dataset.sbp),tiles:document.querySelectorAll('#sndBrowse [data-sbv]').length,hear:document.querySelectorAll('#sndBrowse [data-sbh]').length}));
 ok(s0.tabs.join()==='lead,chords,bass,drums'&&s0.tiles>10&&s0.hear===s0.tiles,'the Sound tab shows each part and its sounds as tiles '+JSON.stringify(s0));
 await p.click('#sndBrowse [data-sbp="bass"]'); await p.waitForTimeout(200);
 const v=await p.evaluate(()=>{ const t=[...document.querySelectorAll('#sndBrowse [data-sbv]')].find(x=>x.getAttribute('aria-pressed')!=='true'&&!/^kit:/.test(x.dataset.sbv)); return t.dataset.sbv; });
 const steps=await p.evaluate(()=>H.past.length); await p.click(`#sndBrowse [data-sbv="${v}"]`); await p.waitForTimeout(300);
 const s1=await p.evaluate(v=>({sound:S.parts.bass.sound,steps:H.past.length,label:H.past[H.past.length-1].label,on:document.querySelector(`#sndBrowse [data-sbv="${v}"]`).getAttribute('aria-pressed'),tab:document.querySelector('#sndBrowse [data-sbp="bass"] small').textContent}),v);
 ok(s1.sound===v&&s1.steps===steps+1&&/bass sound/.test(s1.label)&&s1.on==='true','clicking a tile gives the bass that sound, one undo step '+JSON.stringify(s1));
 await p.fill('#sbFind','juno'); await p.waitForTimeout(200); const f=await p.evaluate(()=>[...document.querySelectorAll('#sndBrowse [data-sbv] b')].map(b=>b.textContent));
 ok(f.length>0&&f.every(t=>/juno/i.test(t)),'the search narrows the tiles '+JSON.stringify(f.slice(0,4)));
 await p.click('#sndBrowse [data-sbp="drums"]'); await p.waitForTimeout(200); const d=await p.evaluate(()=>[...document.querySelectorAll('#sndBrowse [data-sbv]')].map(b=>b.dataset.sbv));
 ok(d.length>0&&d.every(x=>/^kit:|^h:/.test(x)),'drums show kits '+d.length);
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
