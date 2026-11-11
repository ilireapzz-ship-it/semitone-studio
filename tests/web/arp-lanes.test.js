// 12 Nov (Jamie: "give the arp an automation control so producers can make it fade in and out or create contrast"). Arp level and
// Arp length lanes act where the arp's notes are made, so the page, the engine and MIDI all follow. Fade in / Fade out in the Arp panel.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:950}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; const si=S.song.sections.findIndex(s=>s.type==='chorus'); selectSection(si); trSelectPart('chords'); arpToggle('chords',true);
  const bars=barList('song').filter(x=>x.si===si), vel=()=>bars.flatMap(x=>arpBar('chords',x)), plain=vel(), mid=Math.floor(bars.length/2);
  arpAuto('chords','in'); const faded=vel(), first=arpBar('chords',bars[0]), last=arpBar('chords',bars[bars.length-1]);
  const avg=L=>L.reduce((a,n)=>a+n.vel,0)/Math.max(1,L.length);
  const tl=nativeTimeline(); const lanes=autLanes().map(l=>l.ch+'|'+l.tg);
  const undoLbl=H.nextLabel; histUndo(); const back=vel().length;
  arpAuto('chords','gate'); const pts=lanePts('song',si,'chords','arpgate'); pts.forEach(q=>q.v=0); const short=vel(); histUndo();
  return {plain:plain.length,faded:faded.length,first:avg(first),last:avg(last),lastPlain:avg(arpBar('chords',bars[bars.length-1])),lanes,back,shortDur:short[0]&&short[0].dur,plainDur:plain[0].dur,midiOk:!!tl};
 });
 ok(r.faded<r.plain&&r.first<r.last*.4,'Fade in: the first bar is quiet (some notes drop out) and it rises to full by the end '+JSON.stringify(r));
 ok(r.lanes.includes('chords|arp'),'the Arp level lane shows with the other lanes '+JSON.stringify(r.lanes));
 ok(r.back===r.plain,'one Undo takes the fade away');
 ok(r.shortDur<r.plainDur*.3,'Arp length at the bottom makes the notes short and plucky '+JSON.stringify([r.shortDur,r.plainDur]));
 const pop=await p.evaluate(()=>{ arpPopOpen('chords'); return [...document.querySelectorAll('#arpPop [data-arpauto]')].map(b=>b.textContent); });
 ok(pop.join()==='Fade in,Fade out,Level,Length','the Arp panel has Fade in, Fade out, Level and Length '+pop.join());
 const add=await p.evaluate(()=>{ arpPopClose(); AUT.addFor='chords'; autAddPop(); const on=[...document.querySelectorAll('#autAddPop [data-aadd="arp"],#autAddPop [data-aadd="arpgate"]')].map(b=>b.className); arpToggle('chords',false); autAddPop(); const off=$('#autAddPop [data-aadd="arp"]').className; return {on,off}; });
 ok(add.on.length===2&&add.on.every(c=>!/dim/.test(c))&&/dim/.test(add.off),'the add-a-lane list offers Arp level and Arp length while the arp is on '+JSON.stringify(add));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
