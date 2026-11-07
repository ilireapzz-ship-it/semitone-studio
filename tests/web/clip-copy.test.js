// Right-click a section's clip: Copy, then Paste here (empty spot), Paste and replace, or Paste on top. Undo, saving, shortcuts.
const path=require('path'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto('file://'+path.join(__dirname,'..','..','web','semitone-studio.html')); await p.waitForTimeout(1200);
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; S.ctxOn=false; partHear=()=>{}; window.__T=()=>new Promise(r=>setTimeout(r,60));
  window.menu=(sel)=>{ const el=document.querySelector(sel), r=el.getBoundingClientRect(); el.dispatchEvent(new MouseEvent('contextmenu',{bubbles:true,cancelable:true,clientX:r.left+r.width/2,clientY:r.top+r.height/2})); return [...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.querySelector('span').textContent); };
  window.pick=(t)=>{ const b=[...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>x.querySelector('span').textContent===t); if(!b) throw new Error('no menu item '+t); b.click(); };
  window.sig=(p,si)=>JSON.stringify(clipRead(p,si).map(n=>[n.m,+n.start.toFixed(3),+n.dur.toFixed(3)]).sort()); });
 const clip=(p,si)=>`#arrRows [data-arrpart="${p}"][data-arrsec="${si}"]`;
 // copy the Verse 1 melody; menu shows Copy
 let m=await p.evaluate(s=>menu(s),clip('lead',1)); ok(m[0]==='Copy Melody in Verse 1','clip menu starts with Copy',m);
 const pre=await p.evaluate(()=>({v1:sig('lead',1),v2:sig('lead',3),c1:sig('lead',2),c2:sig('lead',4),i:sig('lead',0)}));
 await p.evaluate(()=>pick('Copy Melody in Verse 1'));
 m=await p.evaluate(s=>menu(s),clip('lead',2)); ok(m.includes('Paste and replace')&&m.includes('Paste on top'),'a full section offers Replace and On top',m);
 await p.evaluate(()=>pick('Paste and replace')); await p.waitForTimeout(80);
 let r=await p.evaluate(()=>({v1:sig('lead',1),v2:sig('lead',3),c1:sig('lead',2),c2:sig('lead',4),own:matKey(S.song.sections[2])}));
 ok(r.c1===pre.v1,'Paste and replace puts the verse melody in Chorus 1',r); ok(r.c2===pre.c2&&r.v1===pre.v1&&r.v2===pre.v2,'other sections untouched',[r,pre]); ok(r.own!=='chorus','chorus 1 got its own notes',r.own);
 // saving and reopening keeps it
 const back=await p.evaluate(()=>{ const d=JSON.parse(JSON.stringify(projectData())); H.restoring=true; applyProject(d); H.restoring=false; return {c1:sig('lead',2),c2:sig('lead',4)}; });
 ok(back.c1===pre.v1&&back.c2===pre.c2,'survives saving and opening',back);
 // undo puts the chorus back in one step
 await p.evaluate(async()=>{ histCheck(true); $('#histUndo').click(); await __T(); }); ok(await p.evaluate(c=>sig('lead',2)===c,pre.c1),'one Undo puts Chorus 1 back');
 // paste on top: the chorus keeps its notes and gains the verse's
 await p.evaluate(s=>{ menu(s); pick('Paste on top'); },clip('lead',2)); await p.waitForTimeout(60);
 r=await p.evaluate(()=>clipRead('lead',2)); const set=new Set(r.map(n=>n.m+':'+n.start.toFixed(3)));
 ok(JSON.parse(pre.c1).every(n=>set.has(n[0]+':'+n[1].toFixed(3)))&&JSON.parse(pre.v1).every(n=>set.has(n[0]+':'+n[1].toFixed(3))),'Paste on top keeps both');
 // an 8-bar verse into a 4-bar intro is cut to fit
 await p.evaluate(s=>{ const m=menu(s); pick(m.includes('Paste and replace')?'Paste and replace':'Paste Melody from Verse 1 here'); },clip('lead',0)); ok(await p.evaluate(()=>clipRead('lead',0).every(n=>n.start<4*METERS[S.meter].B)&&clipRead('lead',0).length>0),'cut to fit a shorter section');
 // bass copies between sections
 await p.evaluate(s=>{ menu(s); pick('Copy Bass in Verse 1'); },clip('bass',1)); const bv=await p.evaluate(()=>sig('bass',1));
 m=await p.evaluate(s=>menu(s),clip('lead',2)); ok(m.includes('Paste and replace'),'bass notes paste onto melody too',m);
 await p.evaluate(s=>{ menu(s); pick('Paste and replace'); },clip('bass',2)); ok(await p.evaluate(v=>sig('bass',2)===v,bv),'bass Paste and replace');
 // an empty added track: the blank spot takes a paste
 await p.evaluate(()=>{ trackAddEmpty('lead'); }); await p.waitForTimeout(150);
 const xid=await p.evaluate(()=>xList().slice(-1)[0].id);
 ok(await p.evaluate(x=>!!document.querySelector(`[data-arrblank="${x}"][data-arrsec="1"]`),xid),'empty spots on an added track are pasteable');
 await p.evaluate(s=>{ menu(s); pick('Copy Melody in Verse 1'); },clip('lead',1));
 m=await p.evaluate(x=>menu(`[data-arrblank="${x}"][data-arrsec="1"]`),xid); ok(m[0]==='Paste Melody from Verse 1 here','empty spot offers Paste here',m);
 await p.evaluate(()=>pick('Paste Melody from Verse 1 here')); ok(await p.evaluate(x=>sig(x,1)===sig('lead',1),xid),'pasted into the empty track');
 // drums only paste onto drums
 await p.evaluate(s=>{ menu(s); pick('Copy Drums in Verse 1'); },clip('drums',1)).catch(async()=>{ const nm=await p.evaluate(()=>trName('drums')); await p.evaluate(([s,n])=>{ menu(s); pick(`Copy ${n} in Verse 1`); },[clip('drums',1),nm]); });
 m=await p.evaluate(s=>menu(s),clip('lead',2)); ok(m.some(t=>/drum beat only/.test(t))&&!m.includes('Paste and replace'),'a beat does not paste onto melody',m);
 const dv=await p.evaluate(()=>JSON.stringify(S.beats.verse));
 await p.evaluate(s=>{ menu(s); pick('Paste and replace'); },clip('drums',2)); ok(await p.evaluate(v=>JSON.stringify(S.beats.chorus)===v,dv),'drum beat Paste and replace');
 // chords copy as notes but nothing pastes onto chords
 m=await p.evaluate(s=>menu(s),clip('chords',1)); ok(/^Copy /.test(m[0])&&!m.includes('Paste and replace'),'chords: copy only',m);
 // shortcuts: click a clip, ⌘C, click another, ⌘V
 await p.evaluate(()=>ctxClose()); await p.click(clip('lead',1)); await p.keyboard.press('Meta+c'); await p.click(clip('lead',5)); await p.keyboard.press('Meta+v'); await p.waitForTimeout(80);
 ok(await p.evaluate(()=>sig('lead',5)===JSON.stringify(JSON.parse(sig('lead',1)).filter(n=>n[1]<4*METERS[S.meter].B).sort())),'⌘C and ⌘V on clips',await p.evaluate(()=>[sig('lead',5),CLIP.clip&&CLIP.clip.label]));
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?`clip-copy: ${fails} failed`:'clip-copy ok'); process.exit(fails?1:0); })();
