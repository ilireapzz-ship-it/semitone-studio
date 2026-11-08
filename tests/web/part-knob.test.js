// A volume knob on every part header (docs/design/space "A volume knob on every part"): same number as the mixer fader,
// drag up/down (⇧ fine), double-click for 0 dB, ⌘-click to type; a volume lane shows a brass dot and the knob sets the trim. One undo step each.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; S.auto={}; S.volTrim={}; histCheck(true); arrRender(); });
 const t1=await p.evaluate(()=>{ const ks=[...document.querySelectorAll('#arr [data-pkn]')].map(x=>x.dataset.pkn); return {ks,parts:songParts().concat('drums')}; });
 ok(t1.parts.every(x=>t1.ks.includes(x)),'every part has a knob: '+t1.ks.join(', '));
 // same number as the mixer fader
 const t2=await p.evaluate(()=>{ S.parts.chords.vol=.5; histCheck(true); arrRender(); return {knob:document.querySelector('[data-pkn="chords"] output').textContent, fader:F.db(stripVol('song','chords')).replace(' dB','')}; });
 ok(t2.knob===t2.fader&&/6\.0/.test(t2.knob),'the knob shows the mixer fader’s number: '+t2.knob);
 // drag up 30px = +6 dB, one undo step; ⇧ is finer
 const box=await p.locator('#arr [data-pkn="chords"]').boundingBox();
 const h0=await p.evaluate(()=>H.past.length);
 await p.mouse.move(box.x+box.width/2,box.y+box.height/2); await p.mouse.down(); await p.mouse.move(box.x+box.width/2,box.y+box.height/2-15,{steps:4}); await p.mouse.move(box.x+box.width/2,box.y+box.height/2-30,{steps:4}); await p.mouse.up();
 const t3=await p.evaluate(h0=>({db:20*Math.log10(S.parts.chords.vol),steps:H.past.length-h0,label:H.past[H.past.length-1].label}),h0);
 ok(Math.abs(t3.db-0)<.3&&t3.steps===1&&!/−0\.0/.test(t3.label),`drag up 30px: −6 dB → ${t3.db.toFixed(1)} dB, one undo step (${t3.label}, ${t3.steps})`);
 await p.mouse.move(box.x+box.width/2,box.y+box.height/2); await p.keyboard.down('Shift'); await p.mouse.down(); await p.mouse.move(box.x+box.width/2,box.y+box.height/2+40,{steps:5}); await p.mouse.up(); await p.keyboard.up('Shift');
 const t4=await p.evaluate(()=>20*Math.log10(S.parts.chords.vol));
 ok(Math.abs(t4-(t3.db-2))<.3,`⇧-drag is fine: 40px down → ${t4.toFixed(1)} dB`);
 // double-click 0 dB; the mixer agrees
 await p.mouse.dblclick(box.x+box.width/2,box.y+box.height/2);
 const t5=await p.evaluate(()=>({v:S.parts.chords.vol,txt:document.querySelector('[data-pkn="chords"] output').textContent}));
 ok(Math.abs(t5.v-1)<1e-6&&/0\.0/.test(t5.txt),'double-click → 0 dB');
 // ⌘-click to type
 await p.keyboard.down(process.platform==='darwin'?'Meta':'Control'); await p.mouse.click(box.x+box.width/2,box.y+box.height/2); await p.keyboard.up(process.platform==='darwin'?'Meta':'Control');
 await p.keyboard.press(process.platform==='darwin'?'Meta+A':'Control+A'); await p.keyboard.type('-9.5'); await p.keyboard.press('Enter');
 const t6=await p.evaluate(()=>20*Math.log10(S.parts.chords.vol));
 ok(Math.abs(t6+9.5)<.05,`⌘-click and type −9.5 → ${t6.toFixed(2)} dB`);
 // keyboard: arrows move half a dB
 const t6b=await p.evaluate(()=>{ const el=document.querySelector('[data-pkn="bass"]'); el.focus(); const a=20*Math.log10(stripVol('song','bass')||1e-9); el.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); return 20*Math.log10(stripVol('song','bass'))-a; });
 ok(Math.abs(t6b-.5)<.01,'↑ on a focused knob: +0.5 dB');
 // a volume lane: brass dot, shows the lane value, and turning sets the trim (the lane is untouched)
 const t7=await p.evaluate(()=>{ S.sel=1; S.auto[1]={lead:{vol:[{t:0,v:.5},{t:8,v:.5}]}}; arrRender(); const lane=JSON.stringify(S.auto[1].lead.vol), el=document.querySelector('[data-pkn="lead"]'), dot=el.innerHTML.includes('var(--st-accent)'), shown=el.querySelector('output').textContent; const want=F.db(tV(VOLDEF,autValAt('lead','vol',autNow()))).replace(' dB',''); histCheck(true); pkSet('lead',pkGain('lead')*Math.pow(10,3/20)); pkCommit('lead'); return {dot,shown,want,trim:20*Math.log10(autTrim('lead')),same:JSON.stringify(S.auto[1].lead.vol)===lane,eff:20*Math.log10(stripVol('song','lead'))}; });
 ok(t7.dot&&t7.shown===t7.want,'with a volume lane the knob has a brass dot and shows the lane’s value ('+t7.shown+' dB)');
 ok(Math.abs(t7.trim-3)<.05&&t7.same,`turning it sets a +${t7.trim.toFixed(1)} dB trim and leaves the lane alone`);
 // sizes: editing 22px, arranging 34px with the value, small window 20px
 const sz=async()=>p.evaluate(()=>{ const s=document.querySelector('[data-pkn="chords"] svg').getBoundingClientRect(), o=getComputedStyle(document.querySelector('[data-pkn="chords"] output')).display; return [Math.round(s.width),o]; });
 await p.evaluate(()=>{ spPut({big:true,fold:false}); spFit(); }); await p.waitForTimeout(200); const e1=await sz();
 await p.evaluate(()=>{ spPut({big:false,fold:true}); spFit(); }); await p.waitForTimeout(200); const e2=await sz();
 await p.setViewportSize({width:1024,height:680}); await p.waitForTimeout(300); const e3=await sz();
 ok(e1[0]===22&&e1[1]==='none','editing: 22px knob, value on hover ('+e1+')');
 ok(e2[0]===34&&e2[1]!=='none','arranging: 34px knob with the dB value ('+e2+')');
 ok(e3[0]===20,'small window: 20px knob ('+e3+')');
 await p.setViewportSize({width:1440,height:900}); await p.evaluate(()=>{ spFold(); spFit(); }); await p.waitForTimeout(200);
 await p.screenshot({path:'part-knob.png'});
 ok(!errs.length,'no page errors '+errs.slice(0,3).join(' | '));
 await b.close(); if(fails.length){ console.log(fails.length+' failed'); process.exit(1); } })();
