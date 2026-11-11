// 12 Nov (Jamie: "EQ still has visual bugs when playing music and it should be improved to be like a studio EQ").
// While music played the spectrum was drawn on the curve's 36 dB scale, so all but the loudest note fell off the bottom.
// The analyser has its own 60 dB scale, a rounded and smoothed line that rises fast and falls back slowly, a level "ideal" strip,
// and every band's own shape is filled under the summed curve. The hint steps aside once you have moved a band.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const r=await p.evaluate(()=>{ S.demo=false; const rb=$('#restoreBar'); if(rb) rb.hidden=true; selectSection(1); shellDock('mix'); eqOpen('lead','song'); EQ.over=null;
   // a playing sound: one strong note at 440 Hz, overtones 20 to 45 dB under it, a noise floor 70 dB under
   const lvl=f=>{ const h=[1,2,3,4,5,6].map(k=>440*k), d=[0,-20,-28,-34,-40,-45]; let m=-100; h.forEach((hz,k)=>{ if(Math.abs(Math.log2(f/hz))<.045) m=Math.max(m,-30+d[k]); }); return m; };
   const post=EQF120.map(lvl), D={post,pre:post.slice(),ov:null,ovCh:null,src:'live'}; D.mask=[]; EQ.cache.lead=D; EQ.ref=null; EQ.sm=null; eqDraw();
   const cv=$('#eqCv'), H=cv.clientHeight, V=eqView(D,false), ys=V.post.map(v=>eqSy(v,H)), i=k=>EQF120.findIndex(f=>f>=440*k), old36=db=>eqY(db-(Math.max(...post)-EQ.range*.6),H);
   const out={H,top:+Math.min(...ys).toFixed(0),h2:+ys[i(2)].toFixed(0),h5:+ys[i(5)].toFixed(0),oldH5:+old36(post[i(5)]).toFixed(0),inView:ys.filter(y=>y<H).length};
   // ballistics: the sound stops; the picture falls back, it does not vanish
   EQ.sm=null; const a=eqView(D,true)[ 'post' ][i(1)]; const quiet={post:EQF120.map(()=>-140),pre:EQF120.map(()=>-140),ov:null}; const b1=eqView(quiet,true).post[i(1)]; EQ.sm.t-=250; const b2=eqView(quiet,true).post[i(1)]; EQ.sm.t-=250; const b3=eqView(quiet,true).post[i(1)];
   out.fall=[a,b1,b2,b3].map(v=>+v.toFixed(1)); EQ.sm=null;
   // a rise is quick
   const c1=eqView(quiet,true).post[i(1)], c2=eqView(D,true).post[i(1)], c3=eqView(D,true).post[i(1)]; out.rise=[c1,c2,c3].map(v=>+v.toFixed(1)); EQ.sm=null;
   return out; });
 console.log(JSON.stringify(r));
 ok(r.top>8&&r.top<r.H*.3,'the loudest note sits near the top of the graph, not cut off (y '+r.top+' of '+r.H+')');
 ok(r.h5<r.H&&r.h2<r.H,'overtones 20 and 40 dB under it are in view too (y '+r.h2+' and '+r.h5+')');
 ok(r.oldH5>r.H,'on the curve’s own 36 dB scale that overtone was off the bottom (y '+r.oldH5+'): this is the bug that is gone');
 ok(r.fall[1]>r.fall[0]-12&&r.fall[2]<r.fall[1]&&r.fall[3]<r.fall[2]&&r.fall[3]>r.fall[0]-40,'when the sound stops the picture falls back steadily instead of vanishing ('+r.fall.join(' → ')+' dB)');
 ok(r.rise[2]>r.rise[1]&&r.rise[1]>r.rise[0]+40,'and rises quickly when it starts ('+r.rise.join(' → ')+' dB)');
 // the ideal strip is level across the middle
 const idl=await p.evaluate(()=>{ const cv=$('#eqCv'), g=cv.getContext('2d'), W=cv.clientWidth, H=cv.clientHeight, top=x=>{ for(let y=2;y<H;y++){ const d=g.getImageData(Math.round(x),y,1,1).data; if(d[1]>d[0]+12&&d[1]>d[2]+4&&d[3]>12) return y; } return -1; }; return [300,600,1500,3000].map(f=>top(eqX(f,W))); });
 ok(idl.every(y=>y>0)&&Math.max(...idl)-Math.min(...idl)<=3,'the ideal band is a level strip across the middle of the range (top edge at y '+idl.join(', ')+')');
 // each band's own shape is filled; the hint goes once a band is yours
 const bf=await p.evaluate(()=>{ const eq=eqOf('song','lead'); eq.on=true; eq.bands=[{id:'q1',t:'bell',f:6000,g:9,q:1,s:12,on:true,who:'You'}]; EQ.cache.lead={post:EQF120.map(()=>-140),pre:EQF120.map(()=>-140),ov:null,ovCh:null,src:'live',mask:[]}; EQ.ref=-20; eqLive('song','lead'); eqRender(); const cv=$('#eqCv'), g=cv.getContext('2d'), W=cv.clientWidth, H=cv.clientHeight, a=g.getImageData(Math.round(eqX(6000,W)),Math.round((eqY(0,H)+eqY(9,H))/2),1,1).data, o=g.getImageData(Math.round(eqX(300,W)),Math.round((eqY(0,H)+eqY(9,H))/2),1,1).data, tip=document.querySelector('.eq-tip'); return {inBand:a[3],outside:o[3],tip:tip?tip.hidden:null}; });
 ok(bf.inBand>bf.outside+10,'the band’s own shape is filled between the 0 line and its curve (alpha '+bf.inBand+' inside, '+bf.outside+' beside it)');
 ok(bf.tip!==false,'the “click the curve” hint steps aside once a band is yours');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
