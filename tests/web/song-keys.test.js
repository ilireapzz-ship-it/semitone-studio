// 9 Nov: a new song's key, mode and tempo come from the style's real songs (data/progressions.json), not one stored key per style
// (16 of 34 styles were all E, so every Lana Del Rey song started in E). Styles with no song data draw from the whole library.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ const out={}; const roll=(st,n)=>{ const keys=new Set(), modes=new Set(), bpm=[], fams=new Set(); for(let i=0;i<n;i++){ S.style=st; S.tags=[]; styleRoll(st); keys.add(S.key); modes.add(S.mode); bpm.push(S.bpm); if(st.mMajor&&st.mMinor) fams.add(JSON.stringify(S.matrix)===JSON.stringify(toArr(MINORISH.includes(S.mode)?st.mMinor:st.mMajor))); } return {keys:keys.size,modes:[...modes],lo:Math.min(...bpm),hi:Math.max(...bpm),fam:[...fams]}; };
   STYLES.forEach(st=>{ out[st.id]=roll(st,60); out[st.id].songs=styleSongs(st).length; });
   const known=new Set(); const sad=STYLES.find(x=>x.id==='sadcore'); styleSongs(sad).forEach(s=>known.add(songKeyOf(s).pc)); const seen=new Set(); for(let i=0;i<40;i++){ S.style=sad; styleRoll(sad); seen.add(S.key); }
   out._sadOnlyKnown=[...seen].every(k=>known.has(k));
   out._parse=['E minor','C major / A minor','G mixolydian / A minor','Eb dorian / D# minor','F# minor','Bb major','nonsense'].map(k=>{ const q=songKeyOf({key:k}); return q?q.pc+':'+q.mode:null; });
   const L={}; for(let i=0;i<12;i++){ loadStyle(sad,true); L[S.key]=1; } out._loadStyle=Object.keys(L).length;
   const F={}; for(let i=0;i<12;i++){ freshSong(1000+i*7919); F[S.key]=1; } out._fresh=Object.keys(F).length; return out; });
 const lana=['sadcore','lana-btd','lana-uv','lana-honeymoon','lana-nfr','lana-bb'].map(id=>r[id]);
 ok(lana.every(x=>x.keys>=5),'every Lana Del Rey style now lands on many keys: '+lana.map(x=>x.keys).join(', '));
 ok(Object.entries(r).filter(([k])=>!k.startsWith('_')).every(([,x])=>x.keys>=3),'no style is stuck on one key '+JSON.stringify(Object.fromEntries(Object.entries(r).filter(([k])=>!k.startsWith('_')).map(([k,x])=>[k,x.keys]))));
 ok(r._sadOnlyKnown,'a style with songs only lands on keys its songs use');
 ok(Object.entries(r).filter(([k])=>!k.startsWith('_')).every(([,x])=>x.fam.every(Boolean)),'the chord table matches the major or minor that was picked');
 ok(Object.entries(r).filter(([k])=>!k.startsWith('_')).every(([,x])=>x.lo>=50&&x.hi<=200&&x.hi>x.lo),'tempos vary and stay between 50 and 200 BPM');
 ok(r._parse.join()==='4:aeolian,0:ionian,7:mixolydian,3:dorian,6:aeolian,10:ionian,','key names from the song data read correctly '+r._parse.join());
 ok(r._loadStyle>=3,'picking the same style again gives different keys ('+r._loadStyle+' in 12)');
 ok(r._fresh>=3,'New song picks a new key from the style ('+r._fresh+' in 12)');
 const w=await p.evaluate(()=>{ const o={}; ['lana-btd','lana-uv','lana-honeymoon','lana-nfr','lana-bb'].forEach(id=>{ const st=STYLES.find(x=>x.id===id), ton=new Set(); let rep=0, prev=null; for(let i=0;i<200;i++){ S.style=st; S.tags=[]; styleRoll(st); const n=S.key+(MINORISH.includes(S.mode)?'m':''); if(n===prev) rep++; prev=n; ton.add(S.key); } o[id]={songs:styleSongs(st).length,ton:ton.size,rep}; }); const AK=TL.prog.tracks; o.tracks=['lana-btd','lana-uv','lana-honeymoon','lana-nfr','lana-bb'].reduce((a,id)=>a+AK[id].length,0); o.alt=Object.values(AK).filter(Array.isArray).flat().filter(e=>e.c==='low'&&e.alt).length; return o; });
 ok(w.tracks===83&&w['lana-btd'].songs===24&&w['lana-nfr'].songs===14,'every track on the five Lana albums is in the data (83), each counted once '+JSON.stringify(w));
 ok(['lana-btd','lana-uv','lana-honeymoon','lana-nfr','lana-bb'].every(id=>w[id].ton>=10),'with keys borrowed now and then from her other eras, each Lana era reaches 10+ of the 12 keys');
 ok(['lana-btd','lana-uv','lana-honeymoon','lana-nfr','lana-bb'].every(id=>w[id].rep===0),'picking a style again never gives the same key twice in a row');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
