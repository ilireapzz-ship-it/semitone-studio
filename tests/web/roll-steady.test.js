// 12 Nov, found by drawing a melody by hand in the real app: the piano roll worked its range out again from the notes after every edit,
// so after each drawn note the row under the pointer was a different pitch (eight clicks down a line landed on C7, C7, C7, C7).
// The rows now stay put while you write; Fit the notes still fits. Also: a deleted track leaves no ghost notes in the roll.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 // a deleted track leaves no ghost
 const gh2=await p.evaluate(()=>{ const g0=CanvasRenderingContext2D.prototype.fillRect; const count=()=>{ let n=0; CanvasRenderingContext2D.prototype.fillRect=function(...a){ if(Math.abs(this.globalAlpha-.3)<.001) n++; return g0.apply(this,a); }; renderRoll(); CanvasRenderingContext2D.prototype.fillRect=g0; return n; };
   S.demo=false; selectSection(1); shellDock('roll'); setLayer('melody'); S.ghost='bass'; drawerTitle(); const before=count(); S.trackGone=S.trackGone||{}; const have=xNotes({linkTo:'bass'},1).length; S.trackGone.bass=1; drawerTitle(); const after=count(), lab=$('#drGhost').textContent; delete S.trackGone.bass; drawerTitle(); return {before,after,lab,have,lab2:$('#drGhost').textContent}; });
 ok(gh2.before>0&&gh2.after===0,'with the Bass track deleted the roll draws no bass ghosts ('+gh2.before+' before, '+gh2.after+' after, '+gh2.have+' bass notes)');
 ok(!/Bass/.test(gh2.lab)&&/Bass/.test(gh2.lab2),'and the button no longer says Ghosts: Bass (“'+gh2.lab+'”)');
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); selectSection(1); shellDock('roll'); setLayer('melody'); S.ctxOn=false; renderRoll(); }); await p.waitForTimeout(400);
 const view=()=>p.evaluate(()=>({lo:S.rl.lo,hi:S.rl.hi}));
 const at=(m,beat)=>p.evaluate(([m,beat])=>{ const L=S.rl, r=$('#roll').getBoundingClientRect(); return {x:r.left+beat*(L.W/L.TB)+5,y:r.top+L.top+(L.hi-m)*L.rh+L.rh/2}; },[m,beat]);
 const mel=()=>p.evaluate(()=>matOfSel().melody.map(n=>n.m));
 const v0=await view();
 // a line drawn from where the rows are NOW: every position is worked out once, before the first click, as your eye does
 const want=[67,69,70,72,74,72,70,62], pos=[]; for(let i=0;i<want.length;i++) pos.push(await at(want[i],i));
 for(const q of pos){ await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(260); }
 const got=await mel(), v1=await view();
 ok(JSON.stringify(got)===JSON.stringify(want),'eight notes drawn down a line land on the pitches under the pointer: '+got.join(' '));
 ok(v1.lo===v0.lo&&v1.hi===v0.hi,'the rows did not move while writing ('+v0.lo+'–'+v0.hi+' → '+v1.lo+'–'+v1.hi+')');
 // undo and redo keep the rows too
 await p.keyboard.press('Control+z'); await p.waitForTimeout(300); const v2=await view(); ok(v2.lo===v0.lo&&v2.hi===v0.hi,'Undo leaves the rows where they are');
 // Fit the notes fits them again
 await p.evaluate(()=>rollFit()); await p.waitForTimeout(200); const v3=await view(), g3=await mel();
 ok(v3.lo<=Math.min(...g3)&&v3.hi>=Math.max(...g3)&&(v3.lo!==v0.lo||v3.hi!==v0.hi),'Fit the notes fits the view to them ('+v3.lo+'–'+v3.hi+')');
 // a note outside the view (written by something else) brings a new fit, so nothing is ever hidden
 await p.evaluate(()=>{ matOfSel().melody.push({m:96,start:7,dur:1}); refreshAfterEdit(true); }); const v4=await view(); ok(v4.hi>=96,'a note above the view is shown, not hidden ('+v4.lo+'–'+v4.hi+')');
 // another section starts from its own fit
 await p.evaluate(()=>{ selectSection(2); renderRoll(); }); const v5=await view(); ok(v5.hi<96,'another section gets its own view ('+v5.lo+'–'+v5.hi+')');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
