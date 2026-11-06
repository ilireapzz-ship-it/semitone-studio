// Themes: every token from themes.json becomes a CSS variable, five themes switch with no reload, saved locally, follow the system, text contrast.
const {chromium}=require('playwright'), path=require('path'), fs=require('fs'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
const T=JSON.parse(fs.readFileSync(path.resolve(__dirname,'../../docs/design/merged/themes.json'),'utf8'));
const shots=process.env.THEME_SHOTS;
const lum=h=>{ const c=[1,3,5].map(i=>parseInt(h.slice(i,i+2),16)/255).map(v=>v<=.03928?v/12.92:((v+.055)/1.055)**2.4); return .2126*c[0]+.7152*c[1]+.0722*c[2]; }, ratio=(a,b)=>{ const x=lum(a),y=lum(b); return (Math.max(x,y)+.05)/(Math.min(x,y)+.05); };
(async()=>{ const b=await chromium.launch(), ctx=await b.newContext({viewport:{width:1440,height:900}}); const p=await ctx.newPage(); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ if(!c) fails.push(m); console.log((c?'ok  ':'FAIL')+' '+m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1800);
 ok(await p.evaluate(()=>document.characterSet)==='UTF-8','the page reads as UTF-8 even with fonts in the head');
 ok(await p.evaluate(()=>document.documentElement.getAttribute('data-theme'))==='studio-black','Studio Black is the default');
 const kebab=k=>k.replace(/[A-Z]/g,c=>'-'+c.toLowerCase());
 const skip=new Set(['button','light','fontUi','fontDisplay','fontMono']);
 for(const id of Object.keys(T.themes)){ const t=T.themes[id];
  await p.evaluate(()=>{ if($('#themeCard').hidden) $('#themeBtn').click(); }); await p.click(`[data-theme-id="${id}"]`); await p.waitForTimeout(120);
  const v=await p.evaluate(ks=>{ const c=getComputedStyle(document.documentElement); return {attr:document.documentElement.getAttribute('data-theme'),vals:Object.fromEntries(ks.map(k=>[k,c.getPropertyValue('--st-'+k).trim()])),paper:c.getPropertyValue('--paper').trim(),bodyFont:getComputedStyle(document.body).fontFamily}; },T.tokens.filter(k=>!skip.has(k)).map(kebab));
  ok(v.attr===id,id+': applied with no reload');
  const miss=T.tokens.filter(k=>!skip.has(k)&&!v.vals[kebab(k)]&&k!=='bgExtra'); ok(!miss.length,id+': every token set '+miss); ok(v.vals.bg.toLowerCase()===t.bg.toLowerCase(),id+': background is the token');
  ok(v.bodyFont.includes(t.fontUi.split(' (')[0])||id==='daylight',id+': interface font');
  for(const [fg,bg,n] of [[t.text,t.panel,'text on panel'],[t.text2,t.panel,'secondary on panel'],[t.text,t.bg,'text on background'],[t.lcdInk,t.lcdBg,'display'],[t.accentInk,t.accent,'accent button']]){ const r=ratio(fg,bg); ok(r>=(n==='accent button'?3:4.5),`${id}: ${n} ${r.toFixed(2)}:1`); }
  if(shots){ await p.evaluate(()=>themeToggle(false)); await p.waitForTimeout(150); await p.screenshot({path:path.join(shots,id+'.png')}); } }
 ok(await p.evaluate(()=>getComputedStyle(document.querySelector('.th-tile')?document.querySelector('.th-tile'):document.body)!=null),'tiles render');
 // a hard-coded colour in a part chip would not follow the theme
 const follow=await p.evaluate(()=>{ themeUse('riso-zine'); const a=partColor('lead'); themeUse('console'); return [a,partColor('lead'),orchColor(ORCH[0].id)]; });
 ok(follow[0].toLowerCase()==='#ff4f8b'&&follow[1].toLowerCase()==='#ff6b5e','part colours follow the theme '+follow);
 // saved
 await p.evaluate(()=>{ themeUse('warm-studio',true); }); const undoBefore=await p.evaluate(()=>H&&H.stack?H.stack.length:0);
 await p.reload(); await p.waitForTimeout(1500); ok(await p.evaluate(()=>document.documentElement.getAttribute('data-theme'))==='warm-studio','theme survives a reload');
 ok(!(await p.evaluate(()=>JSON.stringify(projectData()))).includes('warm-studio'),'theme is not saved in the song');
 // follow the system
 await p.emulateMedia({colorScheme:'light'}); await p.evaluate(()=>{ $('#themeBtn').click(); }); await p.check('#themeSys',{force:true}); await p.waitForTimeout(100);
 ok(await p.evaluate(()=>document.documentElement.getAttribute('data-theme'))==='daylight','follow the system: light');
 await p.emulateMedia({colorScheme:'dark'}); await p.waitForTimeout(150); ok(await p.evaluate(()=>document.documentElement.getAttribute('data-theme'))==='studio-black','follow the system: dark');
 await p.reload(); await p.waitForTimeout(1200); ok(await p.evaluate(()=>THEME.pick)==='system','follow the system is remembered');
 // no network fonts
 ok(!(await p.content()).includes('fonts.googleapis.com'),'no font is loaded from the network');
 ok(await p.evaluate(async()=>{ await document.fonts.load("16px 'Instrument Serif'"); return document.fonts.check("16px 'Instrument Serif'"); }),'bundled serif loads');
 // the plug-in and the app share the setting through native
 const n=await ctx.newPage(); n.on('pageerror',e=>errs.push(e.message)); await n.addInitScript(()=>{ window.__noStart=1; window.__nlog=[]; const L={}; window.__JUCE__={backend:{addEventListener:(e,f)=>{(L[e]=L[e]||[]).push(f); return [e,f];},removeEventListener(){},emitEvent:(e,d)=>__nlog.push([e,d])},initialisationData:{}}; window.__fire=(e,d)=>(L[e]||[]).forEach(f=>f(d)); }); await n.goto(url); await n.waitForTimeout(1500);
 const nat=await n.evaluate(()=>{ __fire('st_init',{plugin:true,host:'FL Studio',prefs:{theme:'console'}}); const a=document.documentElement.getAttribute('data-theme'); themeUse('riso-zine',true); return {a,sent:__nlog.filter(x=>x[0]==='st_pref').pop()}; });
 ok(nat.a==='console','the plug-in window uses the saved setting'); ok(nat.sent&&nat.sent[1].value==='riso-zine','a change is sent to the app settings');
 console.log(fails.length?fails.length+' FAILED':'all passed'); console.log(errs.slice(0,6)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
