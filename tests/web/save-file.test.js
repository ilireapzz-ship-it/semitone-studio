// 11 Nov: in the app, Save after Start blank wrote over the song last opened ("meh"). A new song must not be saved into the old file:
// Save asks where to save. Saving the song that is that file still writes straight to it.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const errs=[], fails=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const open=async init=>{ const p=await b.newPage(); p.on('pageerror',e=>errs.push(e.message)); await p.addInitScript(()=>{ window.__nlog=[]; const L={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(L[id]=L[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(L[id]||[]).forEach(f=>f(o)); }); await p.goto(url); await p.waitForTimeout(1200); await p.evaluate(d=>__fire('st_init',d),init); await p.waitForTimeout(800); return p; };
 const p0=await open({plugin:false,host:'',project:'',recent:[]}); const meh=await p0.evaluate(()=>{ S.project={id:null,name:'meh'}; return songStateJSON(); }); await p0.close();
 const file=['C:','Users','Jamie','Documents','Semitone Studio','Songs','meh.semitone'].join(String.fromCharCode(92)); // a Windows path
 const lastSave=p=>p.evaluate(()=>{ const s=__nlog.filter(x=>x[0]==='st_song_save').pop(); return s?{saveAs:s[1].saveAs,name:s[1].name}:null; });
 // the song that is the file: Save writes to it
 let p=await open({plugin:false,host:'',projectB64:Buffer.from(meh).toString('base64'),songFile:file,songFileSame:true,recent:[file]});
 await p.click('#stKeep'); await p.waitForTimeout(200); await p.evaluate(()=>nativeSongSave()); let s=await lastSave(p);
 ok(s&&s.saveAs===false&&s.name==='meh','saving the song you opened writes straight to its file '+JSON.stringify(s));
 // Start blank from Home, then Save: asks instead of writing over meh
 await p.evaluate(()=>homeShow()); await p.waitForTimeout(200); await p.click('#hmNew'); await p.waitForTimeout(300); await p.click('#stBlank'); await p.waitForTimeout(400);
 await p.evaluate(()=>nativeSongSave()); s=await lastSave(p); const nf=await p.evaluate(()=>NATIVE.songFile);
 ok(s&&s.saveAs===true&&!nf,'after Start blank, Save asks where to save (it wrote over meh) '+JSON.stringify({s,nf}));
 // even if the page still thinks a file is open, a song with another name is never written into it
 await p.evaluate(f=>{ NATIVE.songFile=f; S.project={id:null,name:'Untitled song'}; renderProjectBar(); },file); await p.evaluate(()=>nativeSongSave()); s=await lastSave(p);
 ok(s&&s.saveAs===true,'a song with a different name is never saved into the open file '+JSON.stringify(s));
 console.log(fails.length?fails.length+' FAILED':'all passed'); console.log(errs.slice(0,6)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
