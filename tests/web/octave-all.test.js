// 11 Nov (Jamie: "clicking on the up and down an octave tool to move all the notes on the open piano roll without needing to select any"):
// with nothing selected, the rail's Octave up/down move every note in the roll (the whole track when it shows the whole song), as one
// undo step. With notes selected they move just those. Sections that share notes move once.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1600,height:950}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('roll'); trPick(null); setLayer('melody'); S.selSet=new Set(); S.selNote=null; renderRoll(); H.quietUntil=0; histCheck(true); }); await p.waitForTimeout(300);
 const rail=async t=>{ const b_=await p.$(`#rollRail button[title^="${t}"]`); if(!b_) throw new Error('no rail button '+t); await b_.click(); await p.waitForTimeout(250); };
 const mel=si=>p.evaluate(si=>matOf(S.song.sections[si]).melody.map(n=>n.m),si);
 const v0=await mel(1), steps0=await p.evaluate(()=>H.past.length);
 await rail('Octave up'); let v1=await mel(1); const st=await p.evaluate(()=>({steps:H.past.length,label:H.past[H.past.length-1].label}));
 ok(v1.length===v0.length&&v1.every((m,i)=>m===v0[i]+12)&&st.steps===steps0+1&&/Melody up an octave in Verse 1/.test(st.label),'nothing selected: Octave up moves every melody note in the roll, one undo step '+JSON.stringify(st));
 await p.evaluate(()=>histUndo()); await p.waitForTimeout(200); ok((await mel(1)).join()===v0.join(),'Undo puts them back');
 // with two notes selected, only those move
 await p.evaluate(()=>{ const a=matOfSel().melody; S.selSet=new Set(a.slice(0,2)); renderRoll(); }); await rail('Octave down'); const v2=await mel(1);
 ok(v2[0]===v0[0]-12&&v2[1]===v0[1]-12&&v2.slice(2).join()===v0.slice(2).join(),'with notes selected, only those move');
 await p.evaluate(()=>histUndo()); await p.waitForTimeout(200);
 // the whole Bass track, as opened from its name: every section moves, one undo step
 const bassAll=()=>p.evaluate(()=>S.song.sections.map((_,i)=>{ const own=(S.bassOwn||{})[secKey(i)]; return (own||bassGenSec(i)).map(n=>n.m); }));
 const b0=await bassAll(); await p.evaluate(()=>{ S.selSet=new Set(); S.selNote=null; trSelectPart('bass'); }); await p.waitForTimeout(300);
 const whole=await p.evaluate(()=>rollWhole()); const s0=await p.evaluate(()=>H.past.length); await rail('Octave down'); const b1=await bassAll(); const s1=await p.evaluate(()=>({n:H.past.length,label:H.past[H.past.length-1].label}));
 const allDown=b0.every((sec,i)=>sec.length===b1[i].length&&sec.every((m,k)=>m-12===b1[i][k]||(m-12<12&&b1[i][k]===m)));
 ok(whole&&allDown&&s1.n===s0+1&&/Bass down an octave in the whole song/.test(s1.label),'whole Bass track: every section moves down, one undo step '+JSON.stringify({whole,s1,first:[b0[1].slice(0,3),b1[1].slice(0,3)]}));
 // the whole melody: Verse 1 and Verse 2 share notes and move once
 await p.evaluate(()=>{ histUndo(); }); await p.waitForTimeout(200); await p.evaluate(()=>{ S.selSet=new Set(); S.selNote=null; trSelectPart('lead'); }); await p.waitForTimeout(300);
 const shared=await p.evaluate(()=>{ const s=S.song.sections; const i=s.findIndex((x,k)=>k>1&&matKey(x)===matKey(s[1])); return i; });
 const m0=await mel(1); await rail('Octave up'); const m1=await mel(1), m1b=shared>0?await mel(shared):null;
 ok(m1.every((m,i)=>m===m0[i]+12)&&(!m1b||m1b.join()===m1.join()),'whole melody: shared verses move once, not twice '+JSON.stringify({shared,m0:m0.slice(0,3),m1:m1.slice(0,3)}));
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
