// A big song must reach the page without going through JUCE's slow text escaping, and arrive unchanged.
const path=require('path'), fs=require('fs'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
const root=path.join(__dirname,'..','..'), cpp=fs.readFileSync(path.join(root,'Source','PluginEditor.cpp'),'utf8');
ok(!/setProperty \("project", proc\.getProjectState/.test(cpp),'the song is not sent as plain text at start');
ok(/setProperty \("projectB64", forPage/.test(cpp)&&/setProperty \("jsonB64", forPage/.test(cpp),'start and File Open both use the safe form');
(async()=>{ const b=await chromium.launch(), p=await b.newPage(), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.goto('file://'+path.join(root,'web','semitone-studio.html')); await p.waitForTimeout(800);
 const text='{"name":"Café ♪ 歌 \\\\ \'quote\'"}', enc=Buffer.from(text,'utf8').toString('base64');
 ok(await p.evaluate(e=>nB64(e),enc)===text,'accents, symbols and quotes arrive unchanged');
 ok(await p.evaluate(()=>nB64('')==='')&&await p.evaluate(()=>nB64(undefined)===''),'no song stays empty');
 const ms=await p.evaluate(()=>{ const s=btoa('x'.repeat(4e6)), t=performance.now(); nB64(s); return performance.now()-t; }); ok(ms<400,'a 4 MB song decodes quickly',ms);
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?fails+' failed':'startup ok'); process.exit(fails?1:0); })();
