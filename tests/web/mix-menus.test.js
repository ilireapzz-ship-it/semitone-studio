// 12 Nov (Jamie: "right clicking on a plugin in the mixer should allow the option to delete that plugin from the chain instead of the option to
// delete the entire mixer... when i add a new mixer there is no option in its menu to delete it. all mixers except the master mixer should have an
// option to delete that mixer and/or quick clear the chain").
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; window.__e=[]; hostsHere=()=>true; nEmit=(n,d)=>__e.push([n,d]); const m=[...document.querySelectorAll('button')].find(x=>x.textContent.trim()==='Mix'&&x.offsetParent&&x.getBoundingClientRect().top<60); if(m) m.click();
  const st=strip('song','lead'); st.fx=[{type:'eq',on:true,p:{...fxDefaults('eq')}},{type:'plate',on:true,p:{...fxDefaults('plate')}},null,null]; S.hostFx={lead:[{id:'VST3-X',name:'Pro-Q 4'}]}; renderStrips('song'); });
 const menu=sel=>p.evaluate(sel=>{ const el=document.querySelector(sel); if(!el) return null; const r=el.getBoundingClientRect(); el.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+4,clientY:r.top+4})); return [...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.textContent); },sel);
 const pick=t=>p.evaluate(t=>{ const b=[...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>x.textContent===t); if(!b) throw new Error('no item '+t); b.click(); },t);
 // a plug-in on the chain
 let m=await menu('#cMix .strip[data-ch="lead"] .pfx-chip button');
 ok(m&&m.includes('Remove Pro-Q 4 from the chain')&&!m.some(x=>/^Delete /.test(x)),'right-click a plug-in: remove it from the chain, not delete the part '+JSON.stringify(m));
 await pick('Remove Pro-Q 4 from the chain'); await p.waitForTimeout(200); ok(await p.evaluate(()=>!((S.hostFx||{}).lead||[]).length),'the plug-in is gone from the chain');
 // a built-in effect
 m=await menu('#cMix .strip[data-ch="lead"] .fxchip[data-fxslot="1"]');
 ok(m&&m.some(x=>/^Remove .* from the chain$/.test(x))&&m.some(x=>/^Bypass /.test(x)),'right-click an effect: bypass or remove it '+JSON.stringify(m));
 await p.evaluate(()=>{ [...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>/^Remove .* from the chain$/.test(x.textContent)).click(); }); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>!strip('song','lead').fx[1]&&!!strip('song','lead').fx[0]),'just that effect is removed');
 // clear the chain on a part
 m=await menu('#cMix .strip[data-ch="lead"] .st-head, #cMix .strip[data-ch="lead"]'); ok(m&&m.includes('Clear the chain'),'a part strip offers Clear the chain '+JSON.stringify(m&&m.slice(0,3)));
 await pick('Clear the chain'); await p.waitForTimeout(200); ok(await p.evaluate(()=>!strip('song','lead').fx.some(Boolean)),'Clear the chain empties it');
 // a new mixer track (bus) can be deleted
 const id=await p.evaluate(()=>{ const id=rtAddBus(true); const st=rtTouch('chords'); st.route.out=id; renderStrips('song'); return id; }); await p.waitForTimeout(200);
 m=await menu(`#cMix .strip[data-ch="${id}"]`); ok(m&&m.includes('Clear the chain')&&m.some(x=>/^Delete /.test(x)),'a new bus offers Clear the chain and Delete '+JSON.stringify(m));
 await p.evaluate(()=>{ [...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>/^Delete /.test(x.textContent)).click(); }); await p.waitForTimeout(300);
 const gone=await p.evaluate(id=>({inList:!!rtGet(id),strip:!!document.querySelector(`#cMix .strip[data-ch="${id}"]`),out:rtOf('chords').out,label:H.past[H.past.length-1].label}),id);
 ok(!gone.inList&&!gone.strip&&gone.out==='master'&&/^Delete /.test(gone.label),'deleting the bus sends what went into it to the master, one undo step '+JSON.stringify(gone));
 // a built-in bus can be deleted too, and it stays gone after saving
 await p.evaluate(()=>{ const st=rtTouch('bass'); st.route.out='bus-music'; renderStrips('song'); }); await p.waitForTimeout(200); /* a built-in bus shows once you route something there */
 m=await menu('#cMix .strip[data-ch="bus-music"]'); ok(m&&m.some(x=>/^Delete /.test(x)),'a built-in bus can be deleted '+JSON.stringify(m));
 await p.evaluate(()=>{ [...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>/^Delete /.test(x.textContent)).click(); }); await p.waitForTimeout(300);
 const kept=await p.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())); applyProject(d); return {gone:!rtGet('bus-music')}; }); ok(kept.gone,'and stays deleted when the song is saved and opened');
 // the master: clear only
 m=await menu('#cMix .strip[data-ch="master"]'); ok(m&&m.includes('Clear the chain')&&!m.some(x=>/^Delete /.test(x)),'the master can be cleared, not deleted '+JSON.stringify(m));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
