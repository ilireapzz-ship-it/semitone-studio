// 11 Nov (Jamie: "transitions are a bridge between 2 sections; they shouldn't be cut off between sections"): the timeline has a Transitions lane whose
// blocks sit across the join they bridge, change through the transition menu, and a + adds one on an empty join; M turns them all off.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1456,height:819}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1600);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; S.transOn=true; S.transOwn={}; S.trans={}; arrRender(); }); await p.waitForTimeout(150);
 const g=await p.evaluate(()=>{ const row=document.querySelector('#arrRows .trl-row'), cells=[...row.querySelectorAll('.trl-cells > .arr-cell')]; const out=[...row.querySelectorAll('.trl-clip')].map(c=>{ const i=+c.dataset.trlane, r=c.getBoundingClientRect(), j=cells[i].getBoundingClientRect().left; return {i,l:r.left,r:r.right,j}; }); return {n:out.length,joins:S.song.sections.length-1,across:out.every(x=>x.l<x.j-1&&x.r>x.j+1),adds:row.querySelectorAll('.trl-add').length}; });
 ok(g.n>0&&g.across&&g.n+g.adds===g.joins,'each transition is a block across the join it bridges, and empty joins have a + '+JSON.stringify(g));
 // click a block: the menu opens; adding Impact shows on the block
 const i=await p.evaluate(()=>+document.querySelector('#arrRows .trl-clip').dataset.trlane); await p.click(`#arrRows .trl-clip[data-trlane="${i}"]`); await p.waitForTimeout(100);
 ok(await p.evaluate(()=>!!$('#trMenu')),'clicking a block opens its transition menu');
 await p.evaluate(()=>{ const c=document.querySelector('#trMenu [data-trk="impact"]'); if(c&&!c.checked){ c.checked=true; c.dispatchEvent(new Event('change',{bubbles:true})); } }); await p.waitForTimeout(150);
 const im=await p.evaluate(i=>{ const c=document.querySelector(`#arrRows .trl-clip[data-trlane="${i}"]`); return {own:c.classList.contains('own'),post:c.dataset.post,txt:c.textContent}; },i);
 ok(im.own&&im.post==='1'&&/✸/.test(im.txt),'adding an Impact shows on the block, on the landing side '+JSON.stringify(im));
 await p.evaluate(()=>{ if(typeof closeTransMenu==='function') closeTransMenu(); });
 // lengths: Semitone picks them; drag the left edge to make the build-up longer
 const L0=await p.evaluate(i=>({...trLen(i),auto:trLenAuto(i),bars:S.song.sections[i-1].bars}),i);
 ok(L0.pre>0&&L0.pre<=L0.bars,'Semitone gives the transition a build-up length '+JSON.stringify(L0));
 const ed=await p.evaluate(i=>{ trLaneLayout(); const c=document.querySelector(`#arrRows .trl-clip[data-trlane="${i}"]`), r=c.getBoundingClientRect(); return {x:r.left+2,y:r.top+r.height/2,ba:+c.dataset.ba,xr:r.right-2}; },i);
 const h0=await p.evaluate(()=>H.past.length);
 await p.mouse.move(ed.x,ed.y); await p.mouse.down(); await p.mouse.move(ed.x-ed.ba*.5,ed.y,{steps:4}); await p.mouse.move(ed.x-ed.ba*1.0,ed.y,{steps:4}); await p.mouse.up(); await p.waitForTimeout(150);
 const L1=await p.evaluate(([i,h0])=>({...trLen(i),own:(S.transLen||{})[i],steps:H.past.length-h0,label:H.past[H.past.length-1].label,menu:!!$('#trMenu')}),[i,h0]);
 ok(Math.abs(L1.pre-(L0.pre+1))<.3&&L1.steps===1&&/Transition into/.test(L1.label)&&!L1.menu,'dragging the left edge one bar out makes the build-up a bar longer, one undo step, no menu '+JSON.stringify(L1));
 const nat=await p.evaluate(i=>{ const B=METERS[S.meter].B, at=S.song.sections[i].startBar*B, L=nativeTrans().filter(e=>e.at<at&&e.at>=at-trLen(i).pre*B-1e-3); return {n:L.length,first:Math.min(...L.map(e=>e.at)),want:at-trLen(i).pre*B}; },i);
 ok(nat.n>0&&Math.abs(nat.first-nat.want)<.6,'the engine starts the build-up where the block starts '+JSON.stringify(nat));
 const sv=await p.evaluate(i=>{ const d=JSON.parse(JSON.stringify(projectData())); S.transLen={}; applyProject(d); return (S.transLen||{})[i]; },i);
 ok(sv&&Math.abs(sv.pre-L1.pre)<1e-6,'the length is saved with the song');
 // M turns them all off
 await p.evaluate(()=>document.querySelector('#arrRows .trl-row [data-trlaneon]').click()); await p.waitForTimeout(150);
 const off=await p.evaluate(()=>({on:S.transOn,clips:document.querySelectorAll('#arrRows .trl-clip').length,label:H.past[H.past.length-1].label}));
 ok(off.on===false&&off.clips===0&&/Transitions off/.test(off.label),'M turns every transition off, one undo step '+JSON.stringify(off));
 // the header (11 Nov, Jamie: the M overlapped the name; wants solo and volume): name, M, S and a knob on one line, nothing overlapping
 await p.evaluate(()=>{ S.transOn=true; arrRender(); }); await p.waitForTimeout(200);
 const hd=await p.evaluate(()=>{ const r=document.querySelector('#arrRows .trl-row'); const els=[...r.querySelectorAll('.tr-h>.tr-n,.tr-h>.tr-ms button,.tr-h>.tr-kn')].map(e=>e.getBoundingClientRect()); let o=0; for(let i=0;i<els.length;i++)for(let j=i+1;j<els.length;j++){ const a=els[i],c=els[j]; if(a.right>c.left+.5&&c.right>a.left+.5&&a.bottom>c.top+.5&&c.bottom>a.top+.5) o++; } return {o,n:els.length}; });
 ok(hd.n===4&&hd.o===0,'the Transitions header has M, S and volume, none overlapping '+JSON.stringify(hd));
 await p.click('#arrRows .trl-row [data-trms="s"]'); await p.waitForTimeout(150); const so=await p.evaluate(()=>({solo:S.solo,parts:Object.values(S.parts).some(x=>x.on),tr:nativeTrans().length>0})); await p.click('#arrRows .trl-row [data-trms="s"]'); await p.waitForTimeout(150);
 ok(so.solo==='fx'&&!so.parts&&so.tr,'S plays only the transitions '+JSON.stringify(so));
 const kb=await (await p.$('#arrRows .trl-row .tr-kn')).boundingBox(); await p.mouse.move(kb.x+kb.width/2,kb.y+kb.height/2); await p.mouse.down(); await p.mouse.move(kb.x+kb.width/2,kb.y+kb.height/2+30,{steps:4}); await p.mouse.up(); await p.waitForTimeout(250);
 const kv=await p.evaluate(()=>({v:stripVol('song','fx'),nat:nativeSoundInfo().transLevel,label:H.past[H.past.length-1].label}));
 ok(kv.v<.9&&Math.abs(kv.nat-kv.v)<1e-6&&/Transitions volume/.test(kv.label),'the knob turns the transitions down, in the engine too, as one undo step '+JSON.stringify(kv));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
