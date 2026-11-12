// 12 Nov, from the gap report: the blank song written by hand opened with 29 seconds of silence and nothing said so.
// Share has a Finish check: it renders the whole song and reports silence at the start (one click removes empty opening sections),
// clipping, loudness for where the song is going, mono loss and a cut-off ending. It changes nothing unless you press a fix.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 // a song whose intro has nothing in it
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; const m=S.song.mats.intro; if(m){ m.melody=[]; m.chords=m.chords.map(()=>restChord()); m.loop=[]; } ROWS.forEach(r=>{ if(S.beats.intro&&S.beats.intro[r]) S.beats.intro[r]=S.beats.intro[r].map(()=>0); }); S.bassOwn=S.bassOwn||{}; S.bassOwn[secKey(0)]=[]; S.transOn=false; xList().forEach(x=>{ if(x.notes) delete x.notes[secKey(0)]; }); invalidateViews(); S.piece=sectionView(S.sel); H.reset=true; histCheck(true); });
 await p.waitForTimeout(1700);
 ok(await p.evaluate(()=>S.song.sections[0].type==='intro'&&finSecEmpty(0)&&!finSecEmpty(1)),'the song’s intro is empty and its verse is not');
 await p.evaluate(()=>{ pitchShow(); }); await p.waitForTimeout(400);
 const c0=await p.evaluate(()=>({card:!!$('#finCard'),btn:($('#finGo')||{}).textContent,txt:$('#finCard').innerText}));
 ok(c0.card&&c0.btn==='Check the song'&&/silence at the start/.test(c0.txt),'Share has a Finish check card with a Check the song button');
 const before=await p.evaluate(()=>JSON.stringify(projectData()).length);
 await p.click('#finGo'); await p.waitForFunction(()=>!FIN.busy&&FIN.res,null,{timeout:120000}); await p.waitForTimeout(300);
 const r=await p.evaluate(()=>({F:FIN.res.F,lufs:FIN.res.a.lufs,items:finItems(FIN.res.F,FIN.res.a).map(x=>(x.ok?'ok ':'!  ')+x.t+(x.fix?' ['+x.fix+']':'')),html:$('#finCard').innerText,len:JSON.stringify(projectData()).length,introSec:barList('song').filter(b=>b.si===0).map(barTiming).reduce((a,x)=>a+x.dur,0)}));
 console.log(JSON.stringify(r.items)); console.log(JSON.stringify(r.F));
 ok(Math.abs(r.F.start-r.introSec)<1.2,'it measures the silence at the start ('+r.F.start.toFixed(1)+' s; the empty intro lasts '+r.introSec.toFixed(1)+' s)');
 ok(r.items.some(x=>/^!  It starts with \d+ seconds of silence \[Remove Intro\]$/.test(x)),'and says so, offering to remove the empty intro');
 ok(r.items.some(x=>/clipping|loudest moment/.test(x))&&r.items.some(x=>/mono speaker/.test(x))&&r.items.some(x=>/ending/.test(x))&&r.items.some(x=>/LUFS|expects|needs/.test(x)),'it also reports clipping, loudness, mono and the ending');
 ok(r.len===before,'checking changes nothing in the song');
 ok(/things? to look at/i.test(r.html)&&/Check again/i.test(r.html),'the card shows the list and offers Check again');
 // the fix: one click, one undo step
 const past0=await p.evaluate(()=>H.past.length);
 await p.click('#finCard [data-fintrim]'); await p.waitForTimeout(700);
 const t=await p.evaluate(()=>({first:S.song.sections[0].type,past:H.past.length,label:H.past[H.past.length-1].label,card:$('#finCard').innerText}));
 ok(t.first!=='intro'&&t.past===past0+1&&t.label==='Remove the empty start','Remove Intro takes the empty opening away as one undo step (“'+t.label+'”)');
 ok(/Check the song/.test(t.card),'and the card asks to be run again for the changed song');
 await p.click('#finGo'); await p.waitForFunction(()=>!FIN.busy&&FIN.res,null,{timeout:120000});
 const r2=await p.evaluate(()=>finItems(FIN.res.F,FIN.res.a).map(x=>(x.ok?'ok ':'!  ')+x.t));
 ok(r2.includes('ok It starts straight away'),'checked again, it starts straight away ('+r2[0]+')');
 // the pieces, on made-up measurements
 const u=await p.evaluate(()=>{ const mk=o=>finItems(Object.assign({start:.05,monoDrop:-.5,peakDb:-3,tailDb:-80,dur:100},o),{lufs:-14}).filter(x=>!x.ok).map(x=>x.k); return {clean:mk({}),clip:mk({peakDb:-.1}),mono:mk({monoDrop:-5}),tail:mk({tailDb:-30}),quiet:finItems({start:.05,monoDrop:0,peakDb:-3,tailDb:-80},{lufs:-22}).filter(x=>!x.ok).map(x=>x.k)}; });
 ok(u.clean.length===0&&u.clip.join()==='clip'&&u.mono.join()==='mono'&&u.tail.join()==='tail'&&u.quiet.join()==='quiet','each problem is reported on its own, and a clean song gets none ('+JSON.stringify(u)+')');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
