// 22 Nov (Jamie: "i want to be able to change the 'fits the song' for one hand of the piano without it changing the other hand").
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:950}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ S.demo=false; const rb=$('#restoreBar'); if(rb) rb.hidden=true; xAdd('piano'); const R=xList().find(q=>q.role==='pianoR'), L=xList().find(q=>q.role==='pianoL'), si=R.sections[0], k=secKey(si);
  const Lwas=JSON.stringify(L.notes[k]), Rwas=JSON.stringify(R.notes[k]); SHELL.part=R.id; try{ partBarRender(); }catch(e){} try{ xPaneRender(); }catch(e){}
  const label=($('#pbPiano')&&$('#pbPiano').closest('label').textContent)||''; const other=Object.keys(PN_STYLES).find(s=>s!==pnAuto());
  $('#pbPiano').value=other; $('#pbPiano').dispatchEvent(new Event('change',{bubbles:true}));
  const r1={label:label.slice(0,20),R:R.variant,L:L.variant||'',Rchanged:JSON.stringify(R.notes[k])!==Rwas,Lsame:JSON.stringify(L.notes[k])===Lwas};
  try{ partBarRender(); }catch(e){} try{ xPaneRender(); }catch(e){} const both=$('#pbPianoBoth'); r1.both=!!both; if(both) both.click(); r1.Lafter=L.variant; histUndo(); const L2=xList().find(q=>q.role==='pianoL'), R2=xList().find(q=>q.role==='pianoR'); r1.Lundo=L2.variant||''; r1.Rundo=R2.variant; return r1; });
 ok(/Right hand/.test(r.label),'the style menu says which hand it is for '+r.label);
 ok(r.R&&!r.L&&r.Rchanged&&r.Lsame,'changing the right hand style leaves the left hand as it was '+JSON.stringify(r));
 ok(r.both&&r.Lafter===r.R,'Both hands gives the left hand the same style');
 ok(!r.Lundo&&r.Rundo===r.R,'one Undo takes back just the Both hands step');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
