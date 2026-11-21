// 22 Nov (Jamie: "can we also get more arp rhythms... in nexus the ARP has a whole bunch of presets that change the rhythms"). Rhythm presets with
// rests, held (tied) steps and octave jumps; ‹ › step through them; a step can be held or jump octaves by hand.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:950}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ S.demo=false; const rb=$('#restoreBar'); if(rb) rb.hidden=true;
  // the generator on one held chord (C E G) over 4 beats
  const held=[{m:60,start:0,dur:4},{m:64,start:0,dur:4},{m:67,start:0,dur:4}], gen=name=>{ const R=arpRhyList().find(x=>x.n===name); return arpGen(held,4,{...arpCfg('chords'),swing:50,hold:false,oct:1,pat:'up',...arpRhyCfg(R)},1); };
  const tri=gen('Three-three-two'), flow=gen('Ballad flow'), bounce=gen('Octave bounce');
  return {count:arpRhyList().length,tri:tri.map(n=>n.start),flowFirst:flow[0].dur,flowSecond:flow[1]&&flow[1].start,bounce:bounce.slice(0,4).map(n=>n.m)}; });
 ok(r.count>=15,'there are '+r.count+' rhythm presets');
 ok(JSON.stringify(r.tri.slice(0,3))==='[0,0.75,1.5]','Three-three-two plays on 1, the "a" of 1 and the "and" of 2 (rests between) '+JSON.stringify(r.tri));
 ok(r.flowFirst>0.9&&r.flowSecond===1,'Ballad flow: the first note is held through the next step (a tie) '+JSON.stringify([r.flowFirst,r.flowSecond]));
 ok(JSON.stringify(r.bounce)==='[60,76,67,72]','Octave bounce: every second step jumps up an octave '+JSON.stringify(r.bounce));
 // the panel
 const ui=await p.evaluate(()=>{ const si=S.song.sections.findIndex(s=>s.type==='chorus'); selectSection(si); trSelectPart('chords'); arpToggle('chords',true); arpPopOpen('chords');
  const sel=$('#arpRhy'), n0=sel.options.length; $('#arpPop [data-arprhy="1"]').click(); const a=arpCfg('chords').rhy; $('#arpPop [data-arprhy="1"]').click(); const b2=arpCfg('chords').rhy;
  const sel2=$('#arpRhy'); sel2.value='Gallop'; sel2.dispatchEvent(new Event('change',{bubbles:true})); const g=arpCfg('chords'); const label=H.nextLabel||''; histUndo(); const back=arpCfg('chords').rhy;
  arpRhySet('chords',0,'Ballad flow'); const ties=document.querySelectorAll('#arpPop .arp-step.tie').length;
  // a hand edit: alt-click step 1 for an octave
  const st=document.querySelector('#arpPop .arp-step[data-arpacc="1"]'); st.dispatchEvent(new MouseEvent('click',{bubbles:true,altKey:true})); const after=arpCfg('chords');
  return {n0,a,b2,g:{rate:g.rate,rhy:g.rhy,acc:g.acc.join('')},back,ties,ost:after.ost&&after.ost[1],rhyAfter:after.rhy,shown:$('#arpRhy').options[$('#arpRhy').selectedIndex].text}; });
 ok(ui.n0>15&&ui.a&&ui.b2&&ui.a!==ui.b2,'› steps to the next rhythm and the next '+JSON.stringify([ui.a,ui.b2]));
 ok(ui.g.rhy==='Gallop'&&ui.g.rate==='1/16'&&ui.g.acc==='30223022','picking Gallop sets its steps and speed '+JSON.stringify(ui.g));
 ok(ui.back!=='Gallop','one Undo goes back to the rhythm before');
 ok(ui.ties===2,'held steps show dashed in the panel ('+ui.ties+')');
 ok(ui.ost===1&&ui.shown==='Your own steps','Alt-click gives a step an octave jump, and the menu then says Your own steps '+JSON.stringify(ui));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
