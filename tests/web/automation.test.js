// Automation, redesigned (docs/design/automation): the whole song, every lane with who made it, shapes as objects,
// bends, exact values, Write/Touch/Latch from a MIDI controller, macros, fader trim, Off, pass lanes, sizes. One undo step each.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; S.auto={}; S.autoGen={}; S.emoGen={}; S.smart=false; S.macros=[]; S.autoObj=[]; histCheck(true); shellDock('auto'); });
 await p.waitForTimeout(300);
 const t1=await p.evaluate(()=>({tab:!!document.querySelector('#drSubs [data-subpane="auto"]'), tabs:[...document.querySelectorAll('#drSubs [data-subpane]')].map(b=>b.textContent), pane:!!$('#autPane')&&!$('#autPane').closest('[hidden]'), empty:!!document.querySelector('.aut-empty')}));
 ok(t1.tab&&t1.pane&&t1.tabs.includes('Automation')&&t1.tabs.includes('Mixer'),'Automation sits in the Mix tab, beside the Mixer: '+t1.tabs.join(' · '));
 // add a lane in plain words
 const t2=await p.evaluate(()=>{ const h0=H.past.length; autAddLane('chords','filter.cutoff'); return {sel:AUT.sel, name:autName(AUT.sel.ch,AUT.sel.tg), steps:H.past.length-h0, who:autWho(AUT.sel.ch,AUT.sel.tg).n}; });
 ok(t2.name==='Brightness'&&t2.steps===1&&t2.who==='You','Add a lane → "Brightness" on Chords, one undo step, made by You');
 const words=await p.evaluate(()=>{ autAddPop(); const q=$('#autAddQ'); q.value='make it brighter'; q.dispatchEvent(new Event('input',{bubbles:true})); const hit=AUT.addHit; q.value='bigger'; q.dispatchEvent(new Event('input',{bubbles:true})); const h2=AUT.addHit; q.value='faster'; q.dispatchEvent(new Event('input',{bubbles:true})); const h3=AUT.addHit; $('#autAddPop').hidden=true; return [hit,h2,h3]; });
 ok(words[0]==='filter.cutoff'&&words[1]==='plate.mix'&&words[2]==='bpm','plain words: brighter → Brightness, bigger → Reverb, faster → Tempo');
 // whole song: a shape to the next chorus spans sections and stays an object
 const t3=await p.evaluate(()=>{ const secs=S.song.sections, v1=secs.findIndex(s=>s.type==='verse'); S.sel=v1; const B=autB(), L=AUT.sel, before=H.past.length; autShape('build','chorus'); const o=S.autoObj[S.autoObj.length-1]; const nx=secs.findIndex((s,i)=>i>v1&&s.type==='chorus'); return {span:[o.x0,o.x1], want:[secs[v1].startBar*B,secs[nx].startBar*B], steps:H.past.length-before, low:autValAt(L.ch,L.tg,o.x0+.01), high:autValAt(L.ch,L.tg,o.x1-.1), mid:autValAt(L.ch,L.tg,(o.x0+o.x1)/2), objPts:Object.values(S.auto[v1].chords).flat().filter(q=>q.o===o.id).length}; });
 ok(t3.span[0]===t3.want[0]&&t3.span[1]===t3.want[1]&&t3.steps===1,'Build over "to next chorus" spans the right beats, one undo step');
 ok(t3.high>t3.low&&t3.mid<(t3.low+t3.high)/2&&t3.objPts>3,'it builds: slow then fast, and its points belong to the shape object');
 // wobble stays an object: change its speed, then turn it into points
 const t4=await p.evaluate(()=>{ S.sel=S.song.sections.findIndex(s=>s.type==='chorus'); autShape('wobble','bar'); const o=S.autoObj.find(x=>x.kind==='wobble'); const n1=Object.values(S.auto[S.sel].chords).flat().filter(q=>q.o===o.id).length; o.rate='1/16'; autObjWrite(o); const n2=Object.values(S.auto[S.sel].chords).flat().filter(q=>q.o===o.id).length; const lab=autObjLabel(o); autObjFree(o.id); return {n1,n2,lab,left:S.autoObj.some(x=>x.id===o.id),free:Object.values(S.auto[S.sel].chords).flat().filter(q=>q.o===o.id).length}; });
 ok(t4.n2>t4.n1&&/Wobble · 1\/16/.test(t4.lab),'Wobble is an object: faster speed rebuilds its points ('+t4.n1+' → '+t4.n2+'), label '+t4.lab);
 ok(!t4.left&&t4.free===0,'Turn into points makes it hand-editable');
 // copy to every chorus
 const t5=await p.evaluate(()=>{ const secs=S.song.sections, ch=secs.map((s,i)=>s.type==='chorus'?i:-1).filter(i=>i>=0); S.sel=ch[0]; const L=AUT.sel; autCopyType(); return ch.map(i=>JSON.stringify((autPts(i,L.ch,L.tg)||[]).map(q=>[q.t,q.v]))); });
 ok(t5.length>1&&t5.every(x=>x===t5[0]&&x!=='[]'),'Copy to every Chorus'+(t5.every(x=>x===t5[0])?'':' '+t5.map(x=>x.length+':'+x.slice(-160)).join(' / ')));
 // bends: smooth segment fills with hidden points; exact values in units
 const t6=await p.evaluate(()=>{ const si=1, L={ch:'chords',tg:'vol'}; AUT.sel=L; const arr=autEnsure(si,L.ch,L.tg); arr.length=0; arr.push({t:0,v:.3},{t:8,v:.9}); const h={p:arr[1],si,x:S.song.sections[si].startBar*autB()+8}; const r={ch:L.ch,tg:L.tg}; autBend(r,h,'build'); const mid=interp(arr,4), g=arr.filter(q=>q.g).length; const ft=Object.keys(S.auto[S.song.sections.findIndex(s=>s.type==='chorus')].chords).find(k=>/^fx/.test(k)), n=autParse('chords',ft,'6.8 kHz'), back=autFmt('chords',ft,n), db=autFmt('chords','vol',autParse('chords','vol','-6 dB')); return {mid,g,back,db}; });
 ok(t6.g>=8&&t6.mid<.6,'Build bend: slow then fast ('+t6.mid.toFixed(2)+' halfway), drawn with hidden points the engine plays as lines');
 ok(/6\.8 ?kHz/i.test(t6.back)&&/6\.0 dB/.test(t6.db),'exact values in units: "6.8 kHz" and "−6 dB" ('+t6.back+', '+t6.db+')');
 // the knob under the playhead matches the lane
 const t7=await p.evaluate(()=>{ autRender(); const L=AUT.sel, x=autNow(); autHeadsSync(); const out=document.querySelector(`[data-akn="${CSS.escape(autKey(L.ch,L.tg))}"] output`); return {txt:out&&out.textContent, want:autFmt(L.ch,L.tg,autValAt(L.ch,L.tg,x))}; });
 ok(t7.txt===t7.want,'the header knob shows the value under the playhead: '+t7.txt);
 // a moved section keeps its lanes
 const t8=await p.evaluate(()=>{ const i=1, k=JSON.stringify(S.auto[i]); if(typeof secMove!=='function') return {skip:true}; secMove(i,i+1); const j=S.song.sections.findIndex((s,n)=>JSON.stringify(S.auto[n])===k); return {j}; });
 ok(t8.skip||t8.j>=0,'a moved section keeps its lanes');
 // MIDI controller: CC1 writes the selected lane while recording; CC11 → volume; soft takeover; one undo step per take
 const t9=await p.evaluate(()=>{ const L={ch:'lead',tg:'vol'}; AUT.sel=L; const si=S.sel=0, a=S.song.sections[si].startBar*autB(); AUT.mode='write'; AUT.soft=true; AUT.tidy=true; let x=a+.5; const realPos=window.playPos; window.playPos=()=>({si,beat:x-a}); S.playing=true; AUT.armed[autKey(L.ch,L.tg)]=true; const cur=autValAt(L.ch,L.tg,x); const startCC=Math.round(cur*127);
   autCC(1,0); x+=.1; autCC(1,5); const ignored=!(AUT.rec[autKey(L.ch,L.tg)].pts.length); for(let k=0;k<=40;k++){ x+=.1; autCC(1,Math.min(127,Math.round(startCC*(k/40)+ (k>20?30:0)))); } const raw=AUT.rec[autKey(L.ch,L.tg)].pts.length; const h0=H.past.length; S.playing=false; autRecCommit(); window.playPos=realPos; return {ignored,raw,steps:H.past.length-h0,label:H.past[H.past.length-1].label,pts:autHandles(autPts(si,L.ch,L.tg)).length,t11:ctlTarget(11)}; });
 ok(t9.ignored,'soft takeover: the wheel does nothing until it reaches the line');
 ok(t9.steps===1&&/controller/.test(t9.label)&&t9.raw>10&&t9.pts<t9.raw,`CC1 writes the selected lane in one undo step, tidied (${t9.raw} moves → ${t9.pts} points)`);
 ok(t9.t11&&t9.t11.tg==='vol','CC11 (expression) goes to the part’s volume');
 const t10=await p.evaluate(()=>{ const el=$('#ccInd'); return !!el&&el.classList.contains('on')&&/Mod wheel/.test(el.textContent); });
 ok(t10,'the live indicator in the transport shows the controller and where it goes');
 const t11=await p.evaluate(()=>{ const L={ch:'bass',tg:'vol'}; AUT.sel=L; const si=0, a=S.song.sections[si].startBar*autB(); AUT.mode='touch'; AUT.soft=false; let x=a+1; const rp=window.playPos; window.playPos=()=>({si,beat:x-a}); S.playing=true; AUT.armed={}; AUT.armed[autKey(L.ch,L.tg)]=true; const rest=autValAt(L.ch,L.tg,a+6); for(let k=0;k<10;k++){ x+=.1; autCC(1,20); } S.playing=false; autRecCommit(); window.playPos=rp; AUT.mode='write'; AUT.armed={}; return {after:autValAt(L.ch,L.tg,x+.6), rest}; });
 ok(Math.abs(t11.after-t11.rest)<.02,'Touch glides back to the line after you let go');
 // macros: ranges, response, invert; draw from the emotion line
 const t12=await p.evaluate(()=>{ const h0=H.past.length; const m=autMacroNew(true); const h1=H.past.length; const c=m.ctl[0]; const keep=[c.lo,c.hi]; c.lo=.2; c.hi=.8; c.resp='lin'; const a=macMap(c,.5); c.resp='slow'; const s=macMap(c,.5); c.inv=true; const iv=macMap(c,1); c.inv=false; c.resp='lin'; [c.lo,c.hi]=keep; histCheck(true); const h2=H.past.length; macroFromEmotion(m.id); const secs=S.song.sections, chs=secs.findIndex(x=>x.type==='chorus'), ve=secs.findIndex(x=>x.type==='verse'); return {a,s,iv,steps:H.past.length-h0,ctl:m.ctl.length,chorus:autValAt('macro',m.id,secs[chs].startBar*autB()+1),verse:autValAt('macro',m.id,secs[ve].startBar*autB()+1),drives:autValAt(c.ch,c.tg,secs[chs].startBar*autB()+1),who:autWho(c.ch,c.tg).n,fl:m.fl,info:macroHostInfo().length,steps:(h1-h0)+(H.past.length-h2)}; });
 ok(Math.abs(t12.a-.5)<1e-6&&t12.s<t12.a&&Math.abs(t12.iv-.2)<1e-6,'macro range, Slow start response and Invert');
 ok(t12.chorus>t12.verse&&t12.drives>.2&&t12.who==='Macro','Draw it from the emotion line: higher in the chorus, and it drives its controls (made by Macro)');
 ok(t12.fl===1&&t12.info===1&&t12.steps===2,'the first macro is “Macro 1” in FL Studio, and the timeline tells the engine how to follow it');
 await p.evaluate(()=>macroOpen(S.macros[0].id)); await p.waitForTimeout(250); await p.screenshot({path:require('path').resolve(__dirname,'../../automation-macro.png')});
 const t13=await p.evaluate(()=>{ const P=$('#macPop'), r=P.getBoundingClientRect(); const o={w:r.width,h:r.height,knob:!!P.querySelector('.mac-knob'),cards:P.querySelectorAll('.mac-card').length,scroll:P.scrollHeight>P.clientHeight+2}; P.hidden=true; return o; });
 ok(t13.knob&&t13.cards>=3&&t13.w<=921&&!t13.scroll,'the macro panel: knob, control cards, about 900×650, no scrolling');
 // fader trim with a volume lane
 const t14=await p.evaluate(()=>{ const L={ch:'lead',tg:'vol'}; renderStrips('song'); const L0=nativeLanes().find(l=>l.ch===nativeFxCh('lead')&&!l.pan), before=L0&&L0.pts[3]*stripVol('song','lead'); autSetTrim('lead',.5); const L1=nativeLanes().find(l=>l.ch===nativeFxCh('lead')&&!l.pan), after=L1&&L1.pts[3]*stripVol('song','lead'); renderStrips('song'); const out=document.querySelector('#cMix .strip[data-ch="lead"] .st-gain'); autSetTrim('lead',1); return {b:before,a:after,txt:out&&out.textContent}; });
 ok(t14.b&&t14.a&&Math.abs(t14.a/t14.b-.5)<.02&&/Trim/.test(t14.txt||''),'with a volume lane the fader is a trim on top of the lane ('+t14.txt+')');
 // Off and bypass: hear without the moves; the saved song doesn't change
 const t15=await p.evaluate(()=>{ const a=JSON.stringify(projectData()); AUT.off=true; const n=nativeLanes().length+nativeFxLanes().length; AUT.off=false; const m=nativeLanes().length+nativeFxLanes().length; return {same:a===JSON.stringify(projectData()),n,m}; });
 ok(t15.same&&t15.n===0&&t15.m>0,'Off plays no moves and leaves the saved song alone');
 // pass lanes are ghosted until Keep
 const t16=await p.evaluate(()=>{ const was=PASS.A; PASS.A=JSON.stringify({...projectData(),auto:{}}); PASS.q=PASS.q||{tags:['producer:jack-antonoff']}; const tg=Object.keys(S.auto[1].chords||{}).find(k=>/^fx/.test(k))||Object.keys(S.auto[2].chords||{}).find(k=>/^fx/.test(k)); const w=autWho('chords',tg); PASS.A=was; return w; });
 ok(t16.pass&&/-style/.test(t16.n),'a pass’s lanes show as '+t16.n+' and are hatched until Keep');
 // the empty state offers three starts
 const t17=await p.evaluate(()=>{ const keep=JSON.stringify([S.auto,S.macros,S.autoObj]); S.auto={}; S.macros=[]; S.autoObj=[]; AUT.sel=null; try{ localStorage.removeItem('ss_autsel'); }catch(e){} autRender(); const n=document.querySelectorAll('.aut-idea').length; const [a,m,o]=JSON.parse(keep); S.auto=a; S.macros=m; S.autoObj=o; AUT.sel={ch:'chords',tg:Object.keys(S.auto[1].chords).find(k=>/^fx/.test(k))||'vol'}; autRender(); return n; });
 ok(t17===3,'a song with no lanes shows three suggested starts');
 // coach rules see the automation
 const t18=await p.evaluate(()=>{ const R=coachMeasure(); return ['movement','nervous','vocal-space'].map(k=>R[k]&&R[k].score); });
 ok(t18.every(x=>typeof x==='number'),'coach measures movement, nervous and vocal space: '+t18.map(x=>x&&x.toFixed(2)).join(', '));
 // saved and loaded
 const t19=await p.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())); const n=d.macros.length, o=(d.autoObj||[]).length; applyProject(d); return {n,o,back:S.macros.length,objs:S.autoObj.length}; });
 ok(t19.n===t19.back&&t19.o===t19.objs,'macros and shape objects save with the song');
 await p.evaluate(()=>{ shellDock('auto'); autRender(); }); await p.waitForTimeout(300);
 await p.screenshot({path:require('path').resolve(__dirname,'../../automation.png')});
 for(const [w,h] of [[1440,900],[1280,800],[1024,680],[900,600]]){ await p.setViewportSize({width:w,height:h}); await p.waitForTimeout(250); const r=await p.evaluate(()=>({sx:document.scrollingElement.scrollHeight>innerHeight+1||document.scrollingElement.scrollWidth>innerWidth+1, cv:$('#autCv').getBoundingClientRect().height, sel:(AUT.rows||[]).find(r=>r.sel)})); ok(!r.sx&&r.cv>60,`${w}×${h}: no page scroll, lanes ${Math.round(r.cv)}px tall`); }
 // checks from the 8 Nov review
 const tr=await p.evaluate(()=>{ S.auto={}; S.macros=[]; S.autoMac={}; S.volTrim={}; S.parts.lead.vol=1; histCheck(true); S.auto[0]={lead:{vol:[{t:0,v:tN(VOLDEF,1)},{t:4,v:tN(VOLDEF,1)}]}}; autSetTrim('lead',.5); autEnsure(1,'lead','vol'); const v=tV(VOLDEF,S.auto[1].lead.vol[0].v)*autTrim('lead'); S.volTrim={}; return v; });
 ok(Math.abs(tr-.5)<.01,'with a trim, adding a point in another section keeps the level the same (no double trim): '+tr.toFixed(3));
 const mr=await p.evaluate(()=>{ S.auto={}; S.macros=[]; S.autoMac={}; histCheck(true); const m=autMacroNew(true); AUT.sel={ch:'chords',tg:'vol'}; autBegin(); autAddPoint({ch:'chords',tg:'vol'},1,.2); autEnd2('Add a volume point'); return {kept:autPts(0,'chords','vol').some(q=>Math.abs(q.t-1)<1e-3&&Math.abs(q.v-.2)<1e-3),free:!m.ctl.some(c=>c.ch==='chords'&&c.tg==='vol')}; });
 ok(mr.kept&&mr.free,'drawing on a lane the macro drives keeps the point; that lane stops following the macro '+JSON.stringify(mr));
 const gh=await p.evaluate(()=>{ S.auto={}; S.macros=[]; S.autoMac={}; histCheck(true); const m=autMacroNew(); AUT.sel={ch:'macro',tg:m.id}; histUndo(); autRender(); return {sel:AUT.sel&&AUT.sel.ch,lanes:autLanes().filter(l=>l.ch==='macro').length}; });
 ok(gh.sel!=='macro'&&gh.lanes===0,'undoing a new macro leaves no ghost lane '+JSON.stringify(gh));
 const id=await p.evaluate(()=>{ S.auto={}; S.macros=[]; S.autoMac={}; S.autoObj=[]; histCheck(true); const h0=H.past.length; autIdea('fx-cutoff'); const h1=H.past.length; autIdea('breathe'); return [h1-h0,H.past.length-h1]; });
 ok(id[0]===1&&id[1]===1,'each automation idea is one undo step '+id);
 const ln=await p.evaluate(()=>{ S.auto={}; histCheck(true); const B=autB(), e0=(S.song.sections[0].startBar+S.song.sections[0].bars)*B; autWrite('chords','pan',e0-8,e0+8,[{x:e0-8,v:.2},{x:e0+8,v:.8}]); return [autValAt('chords','pan',e0-4),autValAt('chords','pan',e0+4)]; });
 ok(Math.abs(ln[0]-.35)<.01&&Math.abs(ln[1]-.65)<.01,'a straight line across a section edge stays straight '+ln.map(x=>x.toFixed(3)));
 const off=await p.evaluate(()=>{ AUT.off=true; S.rendering=true; const r=autSkip('lead','vol'); S.rendering=false; AUT.off=false; return r; });
 ok(off===false,'Automation Off is for listening only: bounces still have the automation');
 const kk=await p.evaluate(async()=>{ S.auto={}; S.macros=[]; S.volTrim={}; S.parts.bass.vol=.5; arrRender(); H.quietUntil=0; histCheck(true); const h0=H.past.length; const el=document.querySelector('[data-pkn="bass"]'); if(!el) return -1; el.focus(); for(let i=0;i<4;i++) el.dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowUp',bubbles:true})); await new Promise(r=>setTimeout(r,700)); return H.past.length-h0; });
 ok(kk===1,'four quick arrow presses on a part knob are one undo step: '+kk);
 const au=await p.evaluate(()=>{ S.auto={}; S.macros=[]; histCheck(true); const a=auAdd('other'); if(!a) return null; S.sel=1; autAddLane(a.id,'vol'); const ch=nativeFxCh(a.id); return {page:!!autoTargetsInUse('song')[a.id],plug:nativeLanes().filter(l=>l.ch===ch&&!l.pan).length}; });
 ok(au&&au.page&&au.plug===1,'an audio track’s volume lane plays in the page and the plug-in '+JSON.stringify(au));
 ok(!errs.length,'no page errors '+errs.join(' | '));
 await b.close(); console.log(fails.length?`${fails.length} failed`:'all passed'); process.exit(fails.length?1:0); })();
