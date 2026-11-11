// Arpeggiator: the notes match the order, speed and octaves; the page's playback, the MIDI file and the app's engine get the same notes;
// the Arp button and its popover; the roll's preview; Commit to notes is one undo step; settings save, reload and undo; style defaults.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ if(!c) fails.push(m); console.log((c?'ok  ':'FAIL')+' '+m); };
 await p.addInitScript(()=>{ window.__noStart=1; window.__nlog=[]; const L={}; window.__JUCE__={backend:{addEventListener:(e,f)=>{(L[e]=L[e]||[]).push(f); return [e,f];},removeEventListener(){},emitEvent:(e,d)=>__nlog.push([e,d])},initialisationData:{}}; window.__fire=(e,d)=>(L[e]||[]).forEach(f=>f(d)); }); await p.goto(url); await p.waitForTimeout(1800);
 await p.evaluate(()=>{ __fire('st_init',{plugin:false,host:''}); }); await p.waitForTimeout(600);
 ok(await p.evaluate(()=>!('arp' in JSON.parse(JSON.stringify(projectData())))),'a song with no arp saves as before');

 // 1. the generator: order, speed, octaves, length, swing, accents, hold
 const g=await p.evaluate(()=>{ const held=[{m:60,start:0,dur:4},{m:64,start:0,dur:4},{m:67,start:0,dur:4}], base={rate:'1/8',pat:'up',oct:1,gate:80,swing:50,acc:[2],hold:false}, G=(c,h=held,len=4)=>arpGen(h,len,{...base,...c},7), ms=a=>a.map(n=>n.m);
  const rates={}; Object.keys(ARP_RATES).forEach(r=>rates[r]=G({rate:r}).length);
  const rnd=G({pat:'random',rate:'1/16'}), rnd2=G({pat:'random',rate:'1/16'});
  const chord=G({pat:'chord',rate:'1/4'}), sw=G({rate:'1/8',swing:66}), acc=G({acc:[3,0,1,2]}), gap=[{m:60,start:0,dur:1},{m:64,start:0,dur:1}];
  return {up:ms(G({})),down:ms(G({pat:'down'})),updown:ms(G({pat:'updown'})),downup:ms(G({pat:'downup'})),played:ms(G({pat:'played'},[{m:67,start:0,dur:4},{m:60,start:0,dur:4},{m:64,start:0,dur:4}])),
   oct2:ms(G({oct:2})),oct4:[...new Set(ms(G({oct:4,rate:'1/16'})))].length,rates,starts:G({}).map(n=>n.start),durs:[...new Set(G({gate:50}).map(n=>n.dur))],trip:G({rate:'1/8T'}).slice(0,4).map(n=>n.start),
   rndIn:rnd.every(n=>[60,64,67].includes(n.m)),rndSame:JSON.stringify(rnd)===JSON.stringify(rnd2),rndMixed:new Set(ms(rnd)).size===3,rndNoRepeat:rnd.every((n,i)=>!i||n.m!==rnd[i-1].m),
   chordN:chord.length,chordAt0:chord.filter(n=>n.start===0).map(n=>n.m),swing:sw.slice(0,4).map(n=>n.start),swDur:sw.slice(0,2).map(n=>n.dur),
   accV:acc.slice(0,3).map(n=>n.vel),accN:acc.length,accStarts:acc.slice(0,3).map(n=>n.start),
   noHold:G({},gap).length,hold:G({hold:true},gap).length,restart:ms(G({},[{m:60,start:0,dur:1.5},{m:64,start:0,dur:1.5},{m:67,start:0,dur:1.5},{m:62,start:1.5,dur:2.5},{m:65,start:1.5,dur:2.5}])),
   clip:G({rate:'1/4',gate:100},held,3.5).every(n=>n.start+n.dur<=3.5+1e-6)}; });
 console.log(JSON.stringify(g));
 ok(g.up.join()==='60,64,67,60,64,67,60,64','up climbs the chord and starts again');
 ok(g.down.join()==='67,64,60,67,64,60,67,64','down falls');
 ok(g.updown.join()==='60,64,67,64,60,64,67,64','up-down turns round without repeating the top or bottom');
 ok(g.downup.join()==='67,64,60,64,67,64,60,64','down-up is its mirror');
 ok(g.played.join()==='67,60,64,67,60,64,67,60','as played keeps the order the notes were given');
 ok(g.oct2.join()==='60,64,67,72,76,79,60,64','two octaves climb through both'); ok(g.oct4===12,'four octaves reach twelve different notes');
 ok(JSON.stringify(g.rates)===JSON.stringify({'1/4':4,'1/8':8,'1/8T':12,'1/16':16,'1/16T':24,'1/32':32}),'each speed gives the right number of notes in a bar');
 ok(g.starts.join()==='0,0.5,1,1.5,2,2.5,3,3.5'&&g.durs.join()==='0.25','eighths land on the eighths, and Length 50% halves each note'); ok(g.trip.join()==='0,0.3333,0.6667,1','triplets split the beat in three');
 ok(g.rndIn&&g.rndSame&&g.rndMixed&&g.rndNoRepeat,'random stays in the chord, never repeats a note twice running, and is the same every time it is asked');
 ok(g.chordN===12&&g.chordAt0.join()==='60,64,67','whole chord repeats all the notes together');
 ok(g.swing.join()==='0,0.66,1,1.66'&&g.swDur[0]>g.swDur[1],'swing delays every second note');
 ok(g.accV.join()==='1.05,0.55,0.8'&&g.accN===6&&g.accStarts.join()==='0,1,1.5','accents set how hard each step plays, and a silent step plays nothing');
 ok(g.noHold===2&&g.hold===8,'without Hold the arp stops when the notes stop; with Hold it carries on');
 ok(g.restart.join()==='60,64,67,62,65,62,65,62','a new chord starts the pattern again'); ok(g.clip,'no note runs past the end');

 // 2. style defaults
 const d=await p.evaluate(()=>{ const keep=S.style, bpm=S.bpm, tags=S.tags; S.tags=[]; const out={}; S.bpm=120; S.style={id:'synthwave',name:'Synthwave'}; out.synth=arpDefault('chords'); S.style={id:'piano-ballad',name:'Piano ballad'}; out.ballad=arpDefault('chords'); S.style={id:'indie',name:'Indie rock'}; out.plain=arpDefault('chords'); out.lead=arpDefault('lead'); S.bpm=70; out.slow=arpDefault('chords'); S.style=keep; S.bpm=bpm; S.tags=tags; return out; });
 ok(d.synth.rate==='1/16'&&d.synth.pat==='up'&&d.synth.oct===2,'a synth style starts fast and rising'); ok(d.ballad.rate==='1/8'&&d.ballad.pat==='updown'&&d.ballad.gate>=85,'a ballad starts with gentle up-and-down eighths'); ok(d.slow.pat==='updown'&&d.plain.pat==='up'&&d.plain.rate==='1/8','a slow song is gentle too, anything else gets steady eighths'); ok(d.lead.oct>=2&&!d.synth.on,'a melody gets two octaves so it moves, and nothing is switched on for you');

 // 3. the Arp button and popover on the chords
 await p.evaluate(()=>{ S.parts.drums.on=false; S.transOn=false; shellDock('roll'); setLayer('chords'); }); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>{ const bt=document.querySelector('#partBar #arpBtn'); return !!bt&&bt.dataset.arp==='chords'&&bt.getAttribute('aria-pressed')==='false'&&!!bt.closest('#drTools,.roll-tools'); }),'the part bar in the roll’s top row has an Arp button, off to start');
 const h0=await p.evaluate(()=>H.past.length); await p.click('#arpBtn'); await p.waitForTimeout(150);
 const o=await p.evaluate(()=>{ const m=document.querySelector('#arpPop'), r=m.getBoundingClientRect(); return {open:!m.hidden,on:arpOn('chords'),steps:H.past.length,label:H.past[H.past.length-1]&&H.past[H.past.length-1].label,inView:r.top>=0&&r.bottom<=innerHeight&&r.left>=0&&r.right<=innerWidth,rates:m.querySelectorAll('[data-arpk="rate"]').length,pats:m.querySelectorAll('#arpPat option').length,octs:m.querySelectorAll('[data-arpk="oct"]').length,has:!!m.querySelector('#arpGate')&&!!m.querySelector('#arpSwing')&&!!m.querySelector('#arpHold')&&!!m.querySelector('[data-arpcommit]')&&m.querySelectorAll('[data-arpacc]').length>0,btn:document.querySelector('#arpBtn').getAttribute('aria-pressed'),sum:document.querySelector('#arpBtn').textContent}; });
 console.log(JSON.stringify(o)); ok(o.open&&o.on&&o.btn==='true','clicking Arp turns it on and opens its settings'); ok(o.steps===h0+1&&/arp/i.test(o.label||''),'that is one undo step ('+o.label+')'); ok(o.inView,'the popover fits on screen'); ok(o.rates===6&&o.pats===7&&o.octs===4&&o.has,'it has speed, order, octaves, length, swing, accents, hold and Commit'); ok(/Arp\s*1\/\d/.test(o.sum),'the button says what the arp is doing: '+o.sum);
 await p.click('#arpPop [data-arpk="rate"][data-v="1/16"]'); await p.selectOption('#arpPat','updown'); await p.click('#arpPop [data-arpk="oct"][data-v="2"]'); await p.evaluate(()=>{ const s=document.querySelector('#arpGate'); s.value=50; s.dispatchEvent(new Event('input',{bubbles:true})); }); await p.click('#arpPop [data-arpacc="0"]'); await p.click('#arpPop [data-arplen="1"]'); await p.check('#arpHold'); await p.waitForTimeout(100);
 const c=await p.evaluate(()=>({...S.arp.chords,n:document.querySelectorAll('#arpPop [data-arpacc]').length,open:!document.querySelector('#arpPop').hidden})); console.log(JSON.stringify(c));
 ok(c.rate==='1/16'&&c.pat==='updown'&&c.oct===2&&c.gate===50&&c.hold===true&&c.open,'each control changes its setting and the popover stays open'); ok(c.acc.length===c.n&&c.acc[0]===0,'an accent step cycles (accent → silent) and + adds a step');
 await p.click('#arpPop [data-arpacc="0"]'); await p.evaluate(()=>arpSet('chords',{hold:false}));

 // 4. the notes in the song follow the chords
 const n=await p.evaluate(()=>{ const B=METERS[S.meter].B, si=S.sel, v=sectionView(si), ns=arpNotes('chords',si), pads=v.chords.map(c=>new Set((c.pad||[]).flatMap(m=>[m,m+12]))); return {n:ns.length,bars:v.bars,B,inChord:ns.every(q=>pads[Math.floor(q.start/B+1e-6)].has(q.m)),first:ns.slice(0,4).map(q=>q.m),pad:[...v.chords[0].pad].sort((a,b)=>a-b)}; });
 console.log(JSON.stringify(n)); ok(n.n>n.bars*n.B*3&&n.inChord,'every arp note belongs to the chord held in its bar, across two octaves'); ok(n.first[0]===n.pad[0]&&n.first[1]===n.pad[1],'it starts from the bottom of the chord');

 // 5. the page's scheduler, the MIDI file and the app's engine agree
 const agree=async(part,ch)=>p.evaluate(async({part,ch})=>{ ensureAudio(); if(!master) master=newMaster('song'); const B=METERS[S.meter].B, bars=barList('song'), page=[], keepP=playPart; window.playPart=(q,m,t,dur,mul)=>{ if(q===part) page.push([Math.round(t*1000),m,+mul.toFixed(3)]); };
   try{ bars.forEach((bar,i)=>scheduleBar(bar,i*B,1,0)); } finally{ window.playPart=keepP; stopAll&&stopAll(); }
   NATIVE.lastTl=''; nativeSync(); await new Promise(r=>setTimeout(r,60)); const tl=__nlog.filter(x=>x[0]==='st_timeline').pop()[1], eng=[]; for(let i=0;i<tl.notes.length;i+=5) if(tl.notes[i+2]===ch) eng.push([Math.round(tl.notes[i]*1000),tl.notes[i+3],tl.notes[i+4]]);
   const mid=dawTracks('song').tracks.find(t=>t.id===part), midi=mid?mid.notes.map(q=>[Math.round(q.t*1000),q.m,q.v]):[];
   const key=a=>a.map(q=>q[0]+':'+q[1]).sort().join(' '), want=[]; S.song.sections.forEach((s,si)=>arpNotes(part,si).forEach(q=>want.push([Math.round((s.startBar*B+q.start)*1000),q.m])));
   const pv=page.slice().sort((a,b)=>a[0]-b[0]||a[1]-b[1]), ev=eng.slice().sort((a,b)=>a[0]-b[0]||a[1]-b[1]); let rise=0, same=0; for(let i=1;i<pv.length;i++){ const dp=Math.sign(pv[i][2]-pv[i-1][2]), de=Math.sign(ev[i][2]-ev[i-1][2]); if(Math.abs(pv[i][2]-pv[i-1][2])>.1){ rise++; if(dp===de) same++; } }
   return {page:page.length,eng:eng.length,midi:midi.length,want:want.length,pe:key(page)===key(eng),pm:key(page)===key(midi),pw:key(page)===key(want),rise,same}; },{part,ch});
 const a1=await agree('chords',1); console.log(JSON.stringify(a1));
 ok(a1.page>0&&a1.page===a1.want&&a1.pw,'the page plays exactly the arp’s notes ('+a1.page+')'); ok(a1.pe&&a1.eng===a1.page,'the app’s engine is sent the same notes at the same times'); ok(a1.pm&&a1.midi===a1.page,'the MIDI file has the same notes'); ok(a1.rise>0&&a1.same===a1.rise,'accents are louder in both');
 const blk=await p.evaluate(()=>{ arpSet('chords',{on:false}); const off=dawTracks('song').tracks.find(t=>t.id==='chords').notes.length; arpSet('chords',{on:true}); return {off,on:dawTracks('song').tracks.find(t=>t.id==='chords').notes.length}; }); ok(blk.off!==blk.on,'with the arp off the chords play as before ('+blk.off+' block notes, '+blk.on+' arp notes)');
 await p.evaluate(()=>{ arpSet('lead',{on:true,rate:'1/16',pat:'random',oct:3,swing:62}); arpSet('bass',{on:true,rate:'1/8T',pat:'updown',oct:2}); }); const a2=await agree('lead',0), a3=await agree('bass',2); console.log(JSON.stringify(a2),JSON.stringify(a3));
 ok(a2.page>0&&a2.pe&&a2.pm&&a2.pw,'melody: random and swung, the same in the page, engine and MIDI'); ok(a3.page>0&&a3.pe&&a3.pm&&a3.pw,'bass: triplets, the same in the page, engine and MIDI');
 ok(await p.evaluate(()=>songLeadBends().length===0),'an arp on the melody sends no slides or vibrato from the old notes');
 const x=await p.evaluate(()=>{ const x=xAdd('pad'); arpSet(x.id,{on:true,rate:'1/8',pat:'down',oct:1}); return {id:x.id,ch:x.ch}; }); const a4=await agree(x.id,x.ch); console.log(JSON.stringify(a4)); ok(a4.page>0&&a4.pe&&a4.pm&&a4.pw,'an added part: the same in the page, engine and MIDI');
 ok(await p.evaluate(()=>{ const x=xAdd('perc'); return !arpCan(x.id)&&!arpCan('drums'); }),'drums and percussion have no arp');
 const ex=await p.evaluate(async()=>{ let got=null; const keep=downloadsCap; downloadsCap={save:async o=>{ got=o; }}; try{ await exportMidi(); } finally{ downloadsCap=keep; } const direct=readMidi(buildMidi()).tracks.find(t=>t.name==='Chords'); let want=0; S.song.sections.forEach((s,si)=>want+=arpNotes('chords',si).length); return {saved:!!got&&got.data.length>200,n:direct.notes.length,want}; }); ok(ex.saved&&ex.n===ex.want,'Export MIDI writes the arp’s notes ('+ex.n+')');
 await p.evaluate(()=>{ arpSet('lead',{on:false}); arpSet('bass',{on:false}); });

 // 6. the roll shows them
 const dr=await p.evaluate(()=>{ SHELL.part='chords'; setLayer('chords'); const g=CanvasRenderingContext2D.prototype, keep=g.strokeRect; let n=0; const hook=()=>{ g.strokeRect=function(){ n++; return keep.apply(this,arguments); }; }; hook(); renderRoll(); const on=n; n=0; arpSet('chords',{on:false}); renderRoll(); const off=n; g.strokeRect=keep; arpSet('chords',{on:true}); return {on,off,want:arpNotes('chords',S.sel).length}; });
 console.log(JSON.stringify(dr)); ok(dr.on-dr.off>=dr.want,'the roll draws the arp’s notes over the held chords, and not when it is off');

 // 7. settings save, reload and undo
 const sv=await p.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())), was=JSON.stringify(S.arp.chords), t0=dawTracks('song').tracks.find(t=>t.id==='chords').notes.length; S.arp={}; applyProject(JSON.parse(JSON.stringify(d))); const back=JSON.stringify(S.arp.chords)===was, t1=dawTracks('song').tracks.find(t=>t.id==='chords').notes.length; const st=JSON.parse(JSON.parse(songStateJSON()).song?JSON.stringify(JSON.parse(songStateJSON()).song):'{}'); const d2=JSON.parse(JSON.stringify(d)); delete d2.arp; return {saved:!!d.arp&&d.arp.chords.on===true&&d.arp.chords.rate==='1/16',back,same:t0===t1,file:!!(st.arp&&st.arp.chords)}; });
 console.log(JSON.stringify(sv)); ok(sv.saved&&sv.file,'the arp is saved in the song'); ok(sv.back&&sv.same,'opening the song brings it back, playing the same notes');
 await p.waitForTimeout(1700);
 const un=await p.evaluate(async()=>{ histCheck(true); H.quietUntil=0; histCheck(true); const n0=H.past.length; arpSet('chords',{rate:'1/8'}); histCheck(true); const n1=H.past.length, lab=H.past[H.past.length-1].label; histUndo(); await new Promise(r=>setTimeout(r,50)); const r=S.arp.chords.rate; histRedo(); await new Promise(r=>setTimeout(r,50)); return {step:n1-n0,lab,undone:r,redone:S.arp.chords.rate}; });
 console.log(JSON.stringify(un)); ok(un.step===1&&/arp/i.test(un.lab)&&un.undone==='1/16'&&un.redone==='1/8','a setting change is an undo step, and Undo and Redo move it');

 // 8. Commit to notes
 await p.waitForTimeout(1700);
 const cm=await p.evaluate(async()=>{ S.parts.drums.on=false; const id=xList().find(q=>q.role==='pad').id, si=xList().find(q=>q.id===id).sections[0], want=arpNotes(id,si).map(q=>q.start+':'+q.m).join(' '), held=xNotes(xList().find(q=>q.id===id),si).length; histCheck(true); const n0=H.past.length; arpCommit(id); const n1=H.past.length, X=xList().find(q=>q.id===id), got=X.notes[secKey(si)].map(q=>q.start+':'+q.m).join(' '), off=!arpOn(id), lab=H.past[H.past.length-1].label, played=dawTracks('song').tracks.find(t=>t.id===id).notes.length; histUndo(); await new Promise(r=>setTimeout(r,60)); const Y=xList().find(q=>q.id===id); return {step:n1-n0,lab,same:got===want,off,played,undoOn:arpOn(id),undoNotes:(Y.notes[secKey(si)]||[]).length,held}; });
 console.log(JSON.stringify(cm)); ok(cm.same&&cm.off,'Commit writes the arp’s notes into the part and turns the arp off'); ok(cm.step===1,'as one undo step ('+cm.lab+')'); ok(cm.undoOn&&cm.undoNotes===cm.held,'Undo brings the held notes and the arp back');
 await p.waitForTimeout(1700);
 const cb=await p.evaluate(async()=>{ const pick=q=>q.map(n=>n.start+':'+n.m).join(' '); arpSet('bass',{on:true,rate:'1/8',pat:'up',oct:2}); arpSet('lead',{on:true,rate:'1/16',pat:'up',oct:2}); await new Promise(r=>setTimeout(r,1700)); histCheck(true); const si=S.sel, wb=pick(arpNotes('bass',si)), n0=H.past.length; arpCommit('bass'); const n1=H.past.length, gb=pick((S.bassOwn||{})[secKey(si)]||[]); await new Promise(r=>setTimeout(r,1700)); histCheck(true); const mat=matOfSel(), B=METERS[S.meter].B, wl=arpGen(mat.melody.map(n=>({m:n.m,start:n.start,dur:n.dur})),mat.blockBars*B,arpCfg('lead'),arpSeed('lead',S.song.sections.findIndex(s=>matOf(s)===mat))), n2=H.past.length; arpCommit('lead'); const n3=H.past.length; return {bass:wb===gb&&wb.length>0,bstep:n1-n0,lead:pick(matOfSel().melody)===pick(wl)&&wl.length>0,lstep:n3-n2,off:!arpOn('bass')&&!arpOn('lead')}; });
 console.log(JSON.stringify(cb)); ok(cb.bass&&cb.bstep===1,'bass: its arp becomes the bass’s own notes in one step'); ok(cb.lead&&cb.lstep===1&&cb.off,'melody: its arp becomes the melody in one step');
 await p.waitForTimeout(1700);
 await p.evaluate(()=>{ SHELL.part='chords'; shellDock('roll'); setLayer('chords'); }); await p.waitForTimeout(150); await p.click('#arpBtn'); await p.waitForTimeout(150);
 const cc0=await p.evaluate(()=>{ histCheck(true); let want=0; S.song.sections.forEach((s,si)=>want+=arpNotes('chords',si).length); return {n0:H.past.length,parts:xList().length,want,open:!document.querySelector('#arpPop').hidden}; }); await p.click('#arpPop [data-arpcommit]'); await p.waitForTimeout(250);
 const cc=await p.evaluate(async()=>{ const x=xList()[xList().length-1], n=Object.values(x.notes).flat().length, r={step:H.past.length,labs:H.past.slice(-2).map(s=>s.label),parts:xList().length,name:x.name,n,chordsOn:S.parts.chords.on,arp:arpOn('chords'),closed:document.querySelector('#arpPop').hidden,toast:document.querySelector('#toast').textContent,played:dawTracks('song').tracks.find(t=>t.id===x.id).notes.length,noChords:!dawTracks('song').tracks.find(t=>t.id==='chords')}; histUndo(); await new Promise(q=>setTimeout(q,80)); return {...r,undoParts:xList().length,undoOn:S.parts.chords.on,undoArp:arpOn('chords')}; });
 console.log(JSON.stringify(cc0),JSON.stringify(cc)); ok(cc0.open&&cc.parts===cc0.parts+1&&cc.name==='Chords arp'&&cc.n===cc0.want&&cc.played===cc0.want,'chords: Commit puts the notes on a new part, “Chords arp”'); ok(!cc.chordsOn&&cc.noChords&&!cc.arp&&cc.closed&&/Undo/.test(cc.toast),'the block chords are muted so it sounds the same, and it says so'); ok(cc.step===cc0.n0+1&&cc.undoParts===cc0.parts&&cc.undoOn&&cc.undoArp,'one Undo takes the part away and brings the arp back');

 // 9. tidy: a removed part forgets its arp, a new song starts without one
 const td=await p.evaluate(()=>{ const x=xAdd('pad'); arpSet(x.id,{on:true}); const had=!!S.arp[x.id]; xRemove(x.id); return {had,gone:!S.arp[x.id]}; }); ok(td.had&&td.gone,'removing a part forgets its arp');
 console.log(fails.length?fails.length+' FAILED':'all passed'); console.log(errs.slice(0,6)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
