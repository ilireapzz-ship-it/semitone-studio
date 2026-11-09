// 9 Nov: a new song starts from a real song of the era (progressions.json "tracks": description, tags, section plan, switch-ups) chosen by mood and vibe,
// and borrows its section keys (kept through save/open), feel switches, arrangement, the singer's range and chord sound; per-section chord habits;
// a style-fit check; descriptions whose tag words list other songs; never melodies or words.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; });
 const data=await p.evaluate(()=>{ const T=TL.prog.tracks, ids=['lana-btd','lana-uv','lana-honeymoon','lana-nfr','lana-bb'], all=ids.flatMap(id=>T[id]); const words=s=>(s.desc||'').split(/\s+/).length;
   return {n:all.length,desc:all.filter(s=>s.desc).length,maxw:Math.max(...all.map(words)),quotes:all.filter(s=>/["“”]/.test(s.desc||'')).length,plans:all.filter(s=>s.plan&&s.plan.length).length,sw:all.filter(s=>s.switch&&s.switch.length).length,
     tagsInDesc:all.filter(s=>(s.mood||[]).some(m=>new RegExp('\\b'+m+'\\b','i').test(s.desc||''))).length}; });
 ok(data.n===83&&data.desc===83&&data.maxw<=40&&data.quotes===0,'every Lana track has a short description in our own words (no quotes) '+JSON.stringify(data));
 ok(data.plans>=78&&data.sw>=35,'tracks carry a plan per section and their switch-ups');
 ok(data.tagsInDesc>=70,'mood tags appear as words in the descriptions, so they can be clicked');

 // Born to Die: verse in the relative major, chorus in E minor; kept through saving and opening and a key move
 const bt=await p.evaluate(()=>{ loadStyle(STYLES.find(x=>x.id==='lana-btd'),true); refStart('Born to Die'); const v=()=>S.song.mats.verse.chords.slice(0,4).map(c=>pcName(c.root)), c=()=>(S.song.mats.chorus?S.song.mats.chorus.chords:[]).map(c=>c.pcs).flat();
   const o={key:S.key,mode:S.mode,sk:JSON.stringify(S.secKey),verse:v()}; const inG=S.song.mats.verse.chords.every(ch=>ch.pcs.every(pc=>[7,9,11,0,2,4,6].includes(pc))); o.inG=inG; o.chorusInEm=c().every(pc=>[4,6,7,9,11,0,2].includes(pc));
   const d=projectData(); applyProject(d); o.after=v().join(); o.kept=JSON.stringify(S.secKey);
   songTranspose(2); o.moved=S.song.mats.verse.chords.every(ch=>ch.pcs.every(pc=>[9,11,1,2,4,6,8].includes(pc))); histUndo(); return o; });
 ok(bt.key===4&&/aeolian/.test(bt.mode)&&/"verse":\{"iv":3,"mode":"ionian"\}/.test(bt.sk)&&bt.inG&&bt.chorusInEm,'Born to Die gives an E minor song whose verse sits in G major, as the original does '+JSON.stringify(bt));
 ok(bt.after===bt.verse.join()&&bt.kept===bt.sk,'the verse keeps its own key after saving and opening');
 ok(bt.moved,'moving the song up a tone moves the verse with it (A major)');
 const sl=await p.evaluate(()=>{ const vi=S.song.sections.findIndex(s=>s.type==='verse'), ci=S.song.sections.findIndex(s=>s.type==='chorus'); selectSection(vi); const a=[...xScale()].sort((x,y)=>x-y).join(); selectSection(ci); const b=[...xScale()].sort((x,y)=>x-y).join(); return {a,b}; });
 ok(sl.a===sl.b,'scale lock in a relative-major verse uses the same notes as the song (same key signature) '+JSON.stringify(sl));

 // West Coast: straight verses, a half-time chorus, chorus energy drops
 const wc=await p.evaluate(()=>{ loadStyle(STYLES.find(x=>x.id==='lana-uv'),true); refStart('West Coast'); const sn=t=>(S.beats[t].snare||[]).map((v,i)=>v?i:-1).filter(i=>i>=0).join(','); const e=t=>(S.song.sections.find(s=>s.type===t)||{}).energy; return {verse:sn('verse'),chorus:sn('chorus'),ev:e('verse'),ec:e('chorus'),on:S.energyOn}; });
 ok(wc.verse==='4,12'&&wc.chorus==='8'&&wc.ec<wc.ev&&wc.on,'West Coast: straight verses, half-time chorus that drops in energy '+JSON.stringify(wc));
 // a piano-only song: no drums anywhere, piano chords
 const hp=await p.evaluate(()=>{ loadStyle(STYLES.find(x=>x.id==='lana-nfr'),true); const s=styleSongs(S.style).find(x=>/^Hope Is a Dangerous/.test(x.song)); refStart(s.song); return {mute:Object.keys((S.secMute||{}).drums||{}).length,n:S.song.sections.length,sound:S.parts.chords.sound}; });
 ok(hp.mute===hp.n&&hp.sound==='i:piano','Hope Is a Dangerous Thing: drums out of every section, piano chords '+JSON.stringify(hp));

 // mood steers which real song is used
 const mood=await p.evaluate(()=>{ const st=STYLES.find(x=>x.id==='lana-btd'), L=styleSongs(st).concat(famSongs(st)); const share=em=>{ S.emotion=em; let hit=0; for(let i=0;i<200;i++){ S.style=st; styleRoll(st); const e=L.find(x=>x.song===S.refSong.song)||{}; if((e.mood||[]).includes('melancholy')) hit++; } return hit/200; }; const a=share('melancholy'), c=share(''); S.emotion=''; return {a,c}; });
 ok(mood.a>mood.c+.15,'feeling melancholy picks melancholy songs more often '+JSON.stringify(mood));

 // per-section chord habits: verses loop two chords sometimes, choruses never (as in the songs)
 const lp=await p.evaluate(()=>{ const st=STYLES.find(x=>x.id==='lana-btd'); let v=0,c=0; for(let i=0;i<30;i++){ S.emotion=''; loadStyle(st,true); if(new Set(S.song.mats.verse.loop).size<=2) v++; if(S.song.mats.chorus&&new Set(S.song.mats.chorus.loop).size<=2) c++; } return {v,c}; });
 ok(lp.v>=3&&lp.c===0,'verses loop two chords as often as the era\'s verses do; choruses don\'t '+JSON.stringify(lp));

 // the card: tag words list songs, one starts a new take in one undo step; fit check
 await p.evaluate(()=>{ loadStyle(STYLES.find(x=>x.id==='lana-btd'),true); refStart('Born to Die'); shellDock('style'); }); await p.waitForTimeout(300);
 const card=await p.evaluate(()=>({words:[...document.querySelectorAll('.ref-card .ref-w')].map(b=>b.textContent),fit:[...document.querySelectorAll('.ref-fit li')].map(l=>l.className)}));
 ok(card.words.length>=3&&card.fit.length>=3&&card.fit.every(c=>c==='ok'),'the Built from card shows the description with clickable tag words and a passing fit check '+JSON.stringify(card));
 await p.click('.ref-card .ref-w'); await p.waitForTimeout(150);
 const pop=await p.evaluate(()=>({open:!$('#recipePop').hidden,n:document.querySelectorAll('#recipePop [data-refsong]').length}));
 ok(pop.open&&pop.n>=2,'clicking a tag word lists the era\'s songs with that tag '+JSON.stringify(pop));
 const h0=await p.evaluate(()=>H.past.length); await p.click('#recipePop [data-refsong]'); await p.waitForTimeout(250);
 ok(await p.evaluate(h0=>H.past.length===h0+1&&/^Undo: Start from/.test($('#histUndo').title),h0),'starting from that song is one undo step');
 const fx=await p.evaluate(()=>{ const el=$('#bpm'); el.value=190; el.dispatchEvent(new Event('input')); el.dispatchEvent(new Event('change')); const r=(fitReport()||[]).find(x=>x.fix==='tempo'); if(r) fitFix('tempo'); return {had:!!r,now:S.bpm}; });
 ok(fx.had&&fx.now<=130,'a tempo far outside the era is flagged and Back in range fixes it '+JSON.stringify(fx));

 // the new-song page: one line with the description; a new feel picks a new song
 const st=await p.evaluate(async()=>{ startShow(); await new Promise(r=>setTimeout(r,100)); const a=!!document.querySelector('#startView .ref-line .ref-desc'); recipeSetSong('emotion','hopeful'); await new Promise(r=>setTimeout(r,100)); const line=!!document.querySelector('#startView .ref-line'); $('#startView').hidden=true; return {a,line}; });
 ok(st.a&&st.line,'the new-song page says which song it is built from, with its description');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
