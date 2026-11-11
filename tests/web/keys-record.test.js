// 11 Nov (Jamie: "write the midi IN semitone as if you plugged in a midi and were a person playing it"): the Record screen's Keys mode records into
// any part from the computer keyboard. It used to crash before starting (a removed checkbox), so nothing was recorded. Melody and bass keep the notes
// you play, chords become the chord held in each bar, drums become the section's beat; the note keys don't fire the tool shortcuts while you play.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1456,height:819}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; blankSong(); S.bpm=150; selectSection(1); LIVE.countIn=false; recShow(); REC.mode='keys'; recRender(); });
 const take=async(into,keys,hold=180,gap=60)=>{ await p.evaluate(i=>{ document.querySelector(`[data-krinto="${i}"]`).click(); },into); await p.click('#rvGo'); await p.waitForTimeout(250);
   for(const k of keys){ if(Array.isArray(k)){ for(const x of k) await p.keyboard.down(x); await p.waitForTimeout(hold*3); for(const x of k) await p.keyboard.up(x); } else { await p.keyboard.down(k); await p.waitForTimeout(hold); await p.keyboard.up(k); } await p.waitForTimeout(gap); }
   const live=await p.evaluate(()=>({say:($('#krSay')||{}).textContent||'',dots:document.querySelectorAll('.kr-roll i').length})); await p.click('#rvGo'); await p.waitForTimeout(300); return live; };
 // a blank song's piano roll still shows its keys and rows (11 Nov: the range was NaN, so it drew nothing)
 const er=await p.evaluate(()=>{ const o={}; for(const l of ['melody','chords']){ shellDock('roll'); trPick(null); setLayer(l); selectSection(1); renderRoll(); o[l]=[S.rl&&S.rl.lo,S.rl&&S.rl.hi]; } recShow(); REC.mode='keys'; recRender(); return o; });
 ok(Object.values(er).every(([lo,hi])=>Number.isFinite(lo)&&Number.isFinite(hi)&&hi-lo>=12),'an empty piano roll shows its keys and rows '+JSON.stringify(er));
 // the panel
 const ui=await p.evaluate(()=>({into:[...document.querySelectorAll('[data-krinto]')].map(b=>b.textContent),map:document.querySelectorAll('.kr-map .kr-k').length}));
 ok(ui.into.join()==='Melody,Chords,Bass,Drums'&&ui.map>=12,'Keys shows the parts to record into and which key plays what '+JSON.stringify(ui));
 // melody
 const l1=await take('lead',['a','d','g','h','g','f','d']);
 const mel=await p.evaluate(()=>matOf(S.song.sections[1]).melody.map(n=>n.m));
 ok(/Recording Melody/.test(l1.say)&&l1.dots>=5,'while recording it says so and the notes appear '+JSON.stringify(l1));
 ok(mel.length>=5&&mel[0]===60&&mel.includes(67),'the melody you played is in Verse 1 '+JSON.stringify(mel));
 // the note keys don't change the tool
 const tool=await p.evaluate(()=>S.rollTool||'select'); ok(tool==='select','playing D and G didn’t switch to Erase or toggle snap ('+tool+')');
 // bass, an octave down by default
 const l2=await take('bass',['a','a','g','g','h','h','f','f']);
 const bass=await p.evaluate(()=>((S.bassOwn||{})[secKey(1)]||[]).map(n=>n.m));
 ok(bass.length>=6&&bass[0]===36&&bass.every(m=>m<60),'the bass line you played is in Verse 1, in the bass register '+JSON.stringify(bass));
 // chords: hold three notes in each bar
 const l3=await take('chords',[['a','d','g']],400,400);
 const ch=await p.evaluate(()=>{ const m=matOf(S.song.sections[1]); return (m.chords||[]).slice(0,2).map(c=>c&&(c.sym+':'+(c.pad||[]).join('/'))); });
 ok(ch[0]&&/^C(?!m)/.test(ch[0])&&/48\/52\/55/.test(ch[0]),'a held C E G becomes a C chord with your voicing '+JSON.stringify(ch));
 // drums
 const l4=await take('drums',['a','d','s','d','a','d','s','d'],80,120);
 const dr=await p.evaluate(()=>{ const t=S.song.sections[1].type, b=S.beats[t]; return {kick:b.kick.filter(Boolean).length,snare:b.snare.filter(Boolean).length,hat:b.hat.filter(Boolean).length}; });
 ok(dr.kick>=1&&dr.snare>=1&&dr.hat>=2,'A, S and D hit kick, snare and hat, and they become the verse beat '+JSON.stringify(dr));
 // one undo step per take
 const labs=await p.evaluate(()=>H.past.slice(-3).map(x=>x.label)); ok(labs.some(l=>/Played Drums into Verse 1/.test(l)),'each take is an undo step '+JSON.stringify(labs));
 // the key: these takes are in C major; the blank song kept its style's key
 await p.evaluate(()=>recRender()); const adv=await p.evaluate(()=>({k:PC[S.key]+' '+S.mode,txt:(document.querySelector('.kr-key')||{}).textContent||'',btn:!!document.querySelector('[data-krkey]')}));
 ok(adv.k==='C ionian'||(/C major/.test(adv.txt)&&adv.btn),'the panel notices the notes are in C major and offers to make it the key '+JSON.stringify(adv));
 if(adv.btn){ await p.click('[data-krkey]'); await p.waitForTimeout(200); }
 const rk=await p.evaluate(()=>({k:S.key,mode:S.mode,mel:matOf(S.song.sections[1]).melody.map(n=>n.m).slice(0,3),ch:matOf(S.song.sections[1]).chords[0].sym,deg:(document.querySelector('.cs-deg,.clane-wrap')||{}).textContent||''}));
 ok(rk.k===0&&rk.mode==='ionian'&&rk.mel.join()==='60,64,67'&&/^C/.test(rk.ch),'one click makes C major the key; no notes move '+JSON.stringify(rk));
 // punch in: a second take later in the section keeps the first one
 const before=await p.evaluate(()=>matOf(S.song.sections[1]).melody.filter(n=>n.start<4).length);
 await p.evaluate(()=>{ document.querySelector('[data-krinto="lead"]').click(); }); await p.click('#rvGo'); await p.waitForTimeout(3600); for(const k of ['k','j','h']){ await p.keyboard.down(k); await p.waitForTimeout(180); await p.keyboard.up(k); await p.waitForTimeout(60); } await p.click('#rvGo'); await p.waitForTimeout(300);
 const pin=await p.evaluate(()=>{ const M=matOf(S.song.sections[1]).melody; return {early:M.filter(n=>n.start<4).length,late:M.filter(n=>n.start>=8).map(n=>n.m)}; });
 ok(pin.early===before&&pin.early>0&&pin.late.includes(72),'a second take in bar 3 keeps the notes you played in bar 1 '+JSON.stringify({before,...pin}));
 // no false alarm: a C major melody in a C major song (in the app it was called E minor)
 const fa=await p.evaluate(()=>{ blankSong(); S.key=0; S.mode='ionian'; const m=matOf(S.song.sections[1]); m.melody=[60,64,67,69,67,65,64,62,60,64,67,72].map((v,i)=>({m:v,start:i,dur:1})); return keyAdvice(); });
 ok(fa===null,'a C major melody in a C major song gets no key suggestion '+JSON.stringify(fa));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
