// New song is a whole new song with its own band; Pause really stops after a restart; clicking the piano roll moves the play marker.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch({args:['--autoplay-policy=no-user-gesture-required']}); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1500);
 const bands=await p.evaluate(()=>{ S.demo=false; loadStyle(STYLES.find(x=>x.id==='sadcore'),true); xAdd('counter'); S.lyrics={0:'old words'}; const out=[]; for(let i=0;i<6;i++){ histCheck(true); H.quietUntil=0; H.nextLabel='New song'; newProject(); histCheck(true); out.push({band:[S.parts.lead.sound,S.parts.chords.sound,S.parts.bass.sound,...xList().map(x=>x.role)].join(','),ly:Object.keys(S.lyrics).length,key:S.key}); } return out; });
 const pool=await p.evaluate(()=>STYLE_POOL.sadcore);
 ok(new Set(bands.map(x=>x.band)).size>=3,'six new songs get at least three different bands '+JSON.stringify(bands.map(x=>x.band)));
 ok(bands.every(x=>{ const [l,c,bs]=x.band.split(','); return pool.chords.includes(c)&&pool.bass.includes(bs)&&pool.lead.includes(l); }),'every sound fits the style');
 ok(bands.every(x=>x.ly===0)&&new Set(bands.map(x=>x.key)).size>1,'a new song leaves the old words behind and can be in another key');
 const u=await p.evaluate(()=>{ S.lyrics={1:'keep me'}; const before=JSON.stringify([S.parts.chords.sound,xList().map(x=>x.role),S.lyrics,S.key]); histCheck(true); H.quietUntil=0; H.nextLabel='New song'; newProject(); histCheck(true); histUndo(); return before===JSON.stringify([S.parts.chords.sound,xList().map(x=>x.role),S.lyrics,S.key]); });
 ok(u,'one Undo brings the last song back, band and words');
 await p.evaluate(()=>{ window.__n=0; const o=scheduleBar; window.scheduleBar=function(){ window.__n++; return o.apply(this,arguments); }; ensureAudio(); play('song'); }); await p.waitForTimeout(700);
 await p.evaluate(()=>{ stripMS('bass','m'); stripMS('bass','m'); }); await p.waitForTimeout(700); await p.evaluate(()=>$('#songBtn').click()); await p.waitForTimeout(200);
 const n0=await p.evaluate(()=>__n); await p.waitForTimeout(2500); const n1=await p.evaluate(()=>__n); ok(n1===n0,'after muting during playback, Pause stops every bar (none scheduled after) '+(n1-n0));
 await p.evaluate(()=>shellDock('roll')); await p.waitForTimeout(200);
 const a=await p.evaluate(()=>{ rollMark(2); return S.playFrom; }); await p.evaluate(()=>play('section')); await p.waitForTimeout(400); const c=await p.evaluate(()=>{ rollMark(6); return S.playFrom; }); await p.evaluate(()=>stopPlay());
 ok(a===2&&c===6,'clicking an empty spot in the piano roll moves the marker, stopped or playing '+a+','+c);
 ok(!errs.length,'no page errors '+errs.join(' | ')); await b.close(); if(fails.length) process.exit(1); })();
