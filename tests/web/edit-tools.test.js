// 12 Nov, from the gap report: Quantize was all or nothing, and there was no humanize or strength ramp as an edit.
// The Edit menu has Quantize halfway, Humanize and Ramp strength. Each works on the selection (or every note), is one undo step,
// and plain Quantize still snaps exactly.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const set=()=>p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); selectSection(1); shellDock('roll'); setLayer('melody'); S.snapVal=.25; const m=matOfSel(); m.melody=[{m:60,start:.1,dur:.5},{m:62,start:1.08,dur:.5},{m:64,start:2,dur:.5},{m:65,start:3.12,dur:.5}]; S.selSet=new Set(); invalidateViews(); S.piece=sectionView(S.sel); H.reset=true; histCheck(true); renderRoll(); }).then(()=>p.waitForTimeout(1700)); /* a freshly loaded song settles for a second and a half before changes count as undo steps */
 const starts=()=>p.evaluate(()=>matOfSel().melody.map(n=>n.start)), vels=()=>p.evaluate(()=>matOfSel().melody.map(n=>n.vel==null?1:n.vel));
 const menu=await p.evaluate(()=>{ const out=[]; const walk=o=>{ if(Array.isArray(o)) o.forEach(walk); }; return typeof MENUS!=='undefined'?JSON.stringify(Object.keys(MENUS)):'n/a'; });
 await set();
 // halfway
 await p.evaluate(()=>rollQuant(.5)); await p.waitForTimeout(700);
 let s=await starts(); ok(Math.abs(s[0]-.05)<.002&&Math.abs(s[1]-1.04)<.002&&s[2]===2&&Math.abs(s[3]-3.06)<.002,'Quantize halfway moves each note half the way to the grid ('+s.join(', ')+')');
 let h=await p.evaluate(()=>({n:H.past.length,l:H.past.length?H.past[H.past.length-1].label:''})); ok(h.n===1&&h.l==='Quantize halfway','as one undo step (“'+h.l+'”)');
 await p.evaluate(()=>rollQuant()); await p.waitForTimeout(700); s=await starts(); ok(s.join()==='0,1,2,3','plain Quantize still lands exactly on the grid ('+s.join(', ')+')');
 // humanize: small, repeatable, undoable
 await set(); await p.evaluate(()=>{ matOfSel().melody.forEach((n,i)=>{ n.start=i; }); invalidateViews(); S.piece=sectionView(S.sel); H.reset=true; histCheck(true); }); await p.waitForTimeout(1700);
 await p.evaluate(()=>rollHuman()); await p.waitForTimeout(700); const hs=await starts(), hv=await vels();
 ok(hs.every((x,i)=>Math.abs(x-i)<=.031)&&hs.some((x,i)=>Math.abs(x-i)>.002),'Humanize moves notes a little off the grid, never more than 3 hundredths of a beat ('+hs.map(x=>x.toFixed(3)).join(', ')+')');
 ok(hv.every(v=>v>=.85&&v<=1.15)&&new Set(hv.map(v=>v.toFixed(2))).size>1,'and makes their strength a little uneven ('+hv.join(', ')+')');
 h=await p.evaluate(()=>H.past[H.past.length-1].label); ok(h==='Humanize','as one undo step');
 await p.keyboard.press('Control+z'); await p.waitForTimeout(400); ok((await starts()).join()==='0,1,2,3'&&(await vels()).every(v=>v===1),'Undo puts timing and strength back');
 await p.evaluate(()=>rollHuman()); await p.waitForTimeout(500); ok(JSON.stringify(await starts())===JSON.stringify(hs),'the same notes are humanized the same way each time');
 // ramp strength over the selection only
 await set(); await p.evaluate(()=>{ const m=matOfSel().melody; S.selSet=new Set([m[0],m[1],m[2]]); rollVelRamp(); }); await p.waitForTimeout(600);
 const rv=await vels(); ok(rv[0]===.7&&Math.abs(rv[2]-1.1)<.001&&rv[1]>rv[0]&&rv[1]<rv[2]&&rv[3]===1,'Ramp strength runs the selected notes from soft to strong and leaves the others alone ('+rv.join(', ')+')');
 await p.evaluate(()=>{ const m=matOfSel().melody; m[0].vel=1.2; m[2].vel=.6; S.selSet=new Set([m[0],m[1],m[2]]); rollVelRamp(); }); await p.waitForTimeout(600);
 const rv2=await vels(); ok(rv2[0]===1.2&&rv2[2]===.6&&rv2[1]<1.2&&rv2[1]>.6,'between two different strengths it ramps from the first to the last ('+rv2.slice(0,3).join(', ')+')');
 // the Edit menu offers them
 const em=await p.evaluate(()=>{ const src=String(typeof menuBarItems==='function'?menuBarItems:'')+document.documentElement.innerHTML.slice(0,0); return ['Quantize halfway','Humanize','Ramp strength'].map(t=>document.documentElement.outerHTML.includes("['"+t+"'")); });
 ok(em.every(Boolean),'the Edit menu lists Quantize halfway, Humanize and Ramp strength');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
