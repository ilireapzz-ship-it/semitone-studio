// 12 Nov, seen in the real app's mixer: every strip said "A little quiet: +22 dB" after a short play. The strip's level was the last
// second or so of its meter, so stopping on a dying note read as far too quiet. It is now the level while the part is sounding:
// tails and gaps don't count. And 20 dB is not "a little".
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ S.demo=false; shellDock('mix'); const k='song:chords', was=S.playing; MT[k]={p:[-100,-100],r:[-100,-100],h:[-100,-100],ht:[0,0],max:-100,clip:false}; delete LZ[k]; S.playing=true;
   const feed=(db,n)=>{ for(let i=0;i<n;i++){ MT[k].r=[db,db-1]; lzTick(); } };
   feed(-16,300); const steady=LZ[k].avg;            // the part playing
   feed(-34,20); feed(-44,20); feed(-55,20);          // its last note dying away, then Stop
   const tail=LZ[k].avg; feed(-100,50); const gap=LZ[k].avg;   // silence
   feed(-28,600); const quieter=LZ[k].avg;            // a quieter section does move it, in a few seconds
   const rg=lzRange('chords'); LZ[k]={avg:(rg[0]+rg[1])/2-5,n:40}; const v1=lzVerdict('chords'); LZ[k]={avg:(rg[0]+rg[1])/2-20,n:40}; const v2=lzVerdict('chords'); LZ[k]={avg:(rg[0]+rg[1])/2+12,n:40}; const v3=lzVerdict('chords'); LZ[k]={avg:(rg[0]+rg[1])/2,n:40}; const v4=lzVerdict('chords');
   S.playing=was; delete LZ[k]; return {steady,tail,gap,quieter,v1:v1.t,v2:v2.t,v3:v3.t,v4:v4.t}; });
 console.log(JSON.stringify(r));
 ok(Math.abs(r.steady+16)<.5,'while the part plays, its level reads '+r.steady.toFixed(1)+' dB');
 ok(Math.abs(r.tail-r.steady)<1,'a dying last note does not drag it down ('+r.tail.toFixed(1)+' dB)');
 ok(Math.abs(r.gap-r.steady)<1,'nor does silence ('+r.gap.toFixed(1)+' dB)');
 ok(r.quieter<r.steady-6,'a quieter stretch that lasts does move it ('+r.quieter.toFixed(1)+' dB)');
 ok(/^A little quiet: \+5 dB$/.test(r.v1),'5 dB under is “'+r.v1+'”');
 ok(/^Quiet: \+20 dB$/.test(r.v2),'20 dB under is “'+r.v2+'”, not “a little”');
 ok(/^Loud: −?-?12 dB$/.test(r.v3),'12 dB over is “'+r.v3+'”');
 ok(/ideal zone/.test(r.v4),'in range is “'+r.v4+'”');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
