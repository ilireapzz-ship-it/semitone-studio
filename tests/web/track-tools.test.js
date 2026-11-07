// Tracks fold to colour tags, custom colours, x / Delete / right-click delete, editable velocity, the loading screen.
const {chromium}=require('playwright'), path=require('path');
(async()=>{ const br=await chromium.launch(), p=await br.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 let bad=0; const ok=(c,m,x)=>{ if(!c){ bad++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
 await p.addInitScript(()=>{window.__noStart=1});
 await p.goto('file://'+path.resolve(__dirname,'../../web/semitone-studio.html')); await p.waitForTimeout(1500);
 ok(await p.evaluate(()=>!$('#boot')),'no loading screen in the browser');
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; H.quietUntil=0; S.parts.drums.on=true; arrRender(); });
 const rows=()=>p.evaluate(()=>[...document.querySelectorAll('.arr-row.track')].map(r=>r.dataset.track));
 ok((await rows()).join()==='lead,chords,bass,drums','tracks listed',await rows());
 // fold
 await p.click('.arr-row[data-track="bass"] [data-trdo="min"]'); await p.waitForTimeout(80);
 ok(!(await rows()).includes('bass')&&await p.locator('.tr-chip').count()===1,'bass folds into a tag');
 ok(await p.evaluate(()=>S.parts.bass.on),'a folded track still plays');
 await p.click('.tr-chip [data-trdo="exp"]'); ok((await rows()).includes('bass'),'tag opens again');
 // colour
 await p.click('.arr-row[data-track="chords"] .tr-col'); await p.waitForTimeout(80); await p.click('#colPop [data-col="#A974FF"]'); await p.waitForTimeout(80);
 const col=await p.evaluate(()=>({c:S.trackColor.chords,row:document.querySelector('.arr-row[data-track="chords"]').style.getPropertyValue('--c'),v:partVar('chords'),hex:partColor('chords'),saved:projectData().trackColor.chords}));
 ok(col.c==='#A974FF'&&col.row==='#A974FF'&&col.v==='#A974FF'&&col.hex==='#A974FF'&&col.saved==='#A974FF','colour sticks everywhere',col);
 // x
 await p.click('.arr-row[data-track="lead"] [data-trdo="del"]'); await p.waitForTimeout(150);
 const g=await p.evaluate(()=>({on:S.parts.lead.on,str:!!document.querySelector('#cMix .strip[data-ch="lead"]'),bars:buildMidi().length}));
 ok(!(await rows()).includes('lead')&&!g.on&&!g.str,'x deletes a track',g);
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(500);
 ok((await rows()).includes('lead')&&await p.evaluate(()=>S.parts.lead.on),'undo brings it back',await rows());
 // bring back from Add a part
 await p.click('.arr-row[data-track="bass"] [data-trdo="del"]'); await p.waitForTimeout(120); await p.evaluate(()=>spPut({big:false})); await p.waitForTimeout(150); await p.click('#addPartBtn'); await p.waitForTimeout(80); await p.click('[data-xback="bass"]'); await p.waitForTimeout(120);
 ok((await rows()).includes('bass'),'Add a part brings a deleted track back');
 // Delete key on a track and a section
 await p.mouse.click(...await p.evaluate(()=>{ const q=document.querySelector('.arr-row[data-track="bass"] [data-partpop]').getBoundingClientRect(); return [q.left+20,q.top+10]; })); await p.waitForTimeout(100); await p.keyboard.press('Delete'); await p.waitForTimeout(150);
 ok(!(await rows()).includes('bass'),'Delete removes the clicked track',await rows());
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(500);
 const n0=await p.evaluate(()=>S.song.sections.length);
 await p.click('#timeline .sec[data-i="2"]'); await p.waitForTimeout(100); await p.keyboard.press('Delete'); await p.waitForTimeout(250);
 ok(await p.evaluate(()=>S.song.sections.length)===n0-1,'Delete removes the clicked section');
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(500);
 ok(await p.evaluate(()=>S.song.sections.length)===n0,'undo restores the section');
 // clip
 await p.click('.arr-cell.clip[data-arrpart="chords"][data-arrsec="1"]'); await p.waitForTimeout(120); await p.keyboard.press('Delete'); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>S.secMute.chords[1]===true),'Delete on a clip takes it out of that section');
 // right click: our menu, never the browser's
 const rc=async sel=>{ await p.click(sel,{button:'right'}); await p.waitForTimeout(80); return p.evaluate(()=>[...document.querySelectorAll('#ctxMenu button')].map(b=>b.textContent)); };
 let m=await rc('.arr-row[data-track="drums"] [data-partpop]'); ok(m.includes('Delete Drums'),'track menu',m);
 m=await rc('#timeline .sec[data-i="1"]'); ok(m.some(x=>/^Delete /.test(x)),'section menu',m);
 m=await rc('.arr-cell.clip[data-arrpart="drums"][data-arrsec="1"]'); ok(m.some(x=>/^Remove Drums from/.test(x))&&m.includes('Delete Drums')&&m.some(x=>/section$/.test(x)),'clip menu',m);
 await p.click('#ctxMenu button:has-text("Delete Drums")'); await p.waitForTimeout(150); ok(!(await rows()).includes('drums'),'delete from the menu');
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(500);
 const prevented=await p.evaluate(()=>{ const e=new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:50,clientY:400}); document.querySelector('#view-compose').dispatchEvent(e); return e.defaultPrevented; });
 ok(prevented,'no browser menu (and no Reload) anywhere');
 await p.keyboard.press('Escape'); await p.evaluate(()=>{ ctxClose(); shellDock('roll'); setLayer('melody'); renderRoll(); }); await p.waitForTimeout(250);
 // velocity
 const v=await p.evaluate(()=>{ const n=velNotes()[0], cv=$('#rollLaneC'), q=cv.getBoundingClientRect(); return {x:q.left+VEL.X(n.start),t:q.top,h:q.height,open:$('.roll-lane').classList.contains('vel-open')}; });
 ok(v.open&&v.h>70,'velocity lane is roomy',v);
 await p.mouse.move(v.x,v.t+v.h-10); await p.mouse.down(); await p.mouse.move(v.x,v.t+v.h-20,{steps:3}); await p.waitForTimeout(40);
 ok(await p.isVisible('#velTip'),'shows the value while dragging'); await p.mouse.up(); await p.waitForTimeout(150);
 const w=await p.evaluate(()=>{ const q=velNotes()[0], src=q.src||q, vv=viewCached(S.sel), e=exprFor(vv)[0], d=projectData(), m=Object.values(d.mats).flatMap(x=>x.melody).find(x=>x.vel!=null); return {vel:src.vel,e:e.vel,saved:m&&m.vel,midi:buildMidi().length}; });
 ok(w.vel>0&&w.vel<.5&&Math.abs(w.e-w.vel)<.001&&w.saved===w.vel,'velocity plays and saves',w);
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(500);
 ok(await p.evaluate(()=>{ const q=velNotes()[0]; return (q.src||q).vel==null; }),'undo resets velocity');
 // drum step velocity
 await p.evaluate(()=>{ shellDock('beat'); S.beats[S.beatSel].kick[0]=2; renderSeq(); }); await p.waitForTimeout(150);
 const st=await p.locator('.step[data-r="kick"][data-i="0"]').boundingBox();
 await p.mouse.move(st.x+10,st.y+10); await p.mouse.down(); await p.mouse.move(st.x+10,st.y+55,{steps:4}); await p.mouse.up(); await p.waitForTimeout(150);
 const dv=await p.evaluate(()=>({vel:S.beatVel[S.beatSel].kick[0],stay:S.beats[S.beatSel].kick[0],perf:drumPerf(barList('song').find(b=>b.type===S.beatSel&&b.b===0)).find(e=>e.r==='kick'&&e.i===0)}));
 ok(dv.vel<.7&&dv.stay===2&&dv.perf,'drag a drum step sets its velocity',dv);
 ok(!errs.length,'no page errors',errs.slice(0,3));
 await br.close(); console.log(bad?`${bad} failed`:'track-tools ok'); process.exit(bad?1:0); })();
