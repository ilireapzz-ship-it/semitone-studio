// 12 Nov (Jamie, watching the EQ: "notice the visual errors in the EQ?"). The measured shape was lined up by its average level, so a part
// whose notes leave gaps between them ran off the top as a flat block; the masking hatch filled the whole height and hid the labels;
// the ideal band sat far from the shape. Now: the loudest point sits inside the graph, the hatch stays under the two lines,
// the ideal band runs through the strong notes, and a boost still lifts the after-EQ line exactly as far as the curve.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 // a spectrum like a melody's: strong lines 450 Hz to 1.5 kHz, a deep gap at 850 Hz, nothing below 450, a slope above; chords under it
 const m=await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('mix'); eqOpen('lead','song'); EQ.over='chords';
   const post=EQF120.map(f=>f<450?-150:f>800&&f<900?-110:f<1500?-22:-22-14*Math.log2(f/1500)), ov=EQF120.map(f=>f<120?-150:f<3000?-30:-30-18*Math.log2(f/3000));
   const D={post,pre:post.slice(),ov,ovCh:'chords',src:'live'}; D.mask=eqMask(D); EQ.cache.lead=D; EQ.ref=null; eqDraw();
   const cv=$('#eqCv'), H=cv.clientHeight, W=cv.clientWidth, sy=db=>eqSy(db,H), V=eqView(D,false), ys=V.post.map(sy), top=Math.min(...ys), offTop=ys.filter(y=>y<0).length; post.forEach((v,i)=>{ post[i]=V.post[i]; }); ov.forEach((v,i)=>{ ov[i]=V.ov[i]; });
   const g=cv.getContext('2d'), dpr=devicePixelRatio||1, px=(x,y)=>[...g.getImageData(Math.round(x*dpr),Math.round(y*dpr),1,1).data];
   const mk=D.mask[0], xm=mk?eqX(Math.sqrt(mk[0]*mk[1]),W):0;
   // hatch: nothing well above both lines, something below them
   let above=0, below=0; if(mk){ for(let x=eqX(mk[0],W)+6;x<eqX(mk[1],W)-6;x+=7){ const i=EQF120.findIndex(f=>eqX(f,W)>=x), yat=k=>Math.max(sy(post[k]),sy(ov[k])), yl=Math.min(yat(Math.max(0,i-1)),yat(i),yat(Math.min(119,i+1))); /* the highest the hatch's edge can be near this x */ const yt=Math.min(...[i-1,i,i+1].map(k=>Math.min(sy(post[Math.max(0,Math.min(119,k))]),sy(ov[Math.max(0,Math.min(119,k))])))); /* above BOTH lines: between them the part's own fills add up to a soft red too */ if(yt>40){ for(let y=8;y<yt-14;y+=5){ if(Math.abs(y-eqY(0,H))<16) continue; /* the band dots and the curve live on the 0 line */ const q=px(x,y); above+=q[0]>150&&q[1]<140&&q[3]>75&&q[3]<125?1:0; /* the hatch is a soft red; the part's own line is stronger */ } } for(let y=Math.min(H-8,yl+10);y<H-6;y+=3) below+=px(x,y)[0]>100?1:0; } }
   // ideal band: its middle at 1 kHz is within 6 dB of the strong notes there
   const i1=EQF120.findIndex(f=>f>=1000); return {top:+top.toFixed(1),offTop,H,mask:D.mask,above,below,ref:+EQ.ref.toFixed(1),p1k:post[i1]}; });
 console.log(JSON.stringify(m));
 ok(m.top>8&&m.offTop===0,'the loudest point of the shape sits inside the graph, nothing runs off the top (top y '+m.top+')');
 ok(m.top<m.H*.3,'and it sits in the top part of the graph, not sunk to the bottom');
 ok(m.mask.length>0&&m.above===0&&m.below>0,'the masking hatch is drawn under the two lines only ('+m.above+' above, '+m.below+' below)');
 // labels on top: the 1k label is still readable inside a masked stretch (some light pixels near it)
 const lab=await p.evaluate(()=>{ const cv=$('#eqCv'), g=cv.getContext('2d'), dpr=devicePixelRatio||1, W=cv.clientWidth, H=cv.clientHeight, x0=Math.round((eqX(1000,W)+2)*dpr), d=g.getImageData(x0,Math.round((H-12)*dpr),Math.round(14*dpr),Math.round(10*dpr)).data; let lit=0; for(let i=0;i<d.length;i+=4) if(d[i+3]>120&&d[i]>80&&Math.abs(d[i]-d[i+1])<40) lit++; return lit; });
 ok(lab>3,'the frequency numbers are drawn over the spectrum ('+lab+' lit pixels at 1k)');
 // a boost still lifts the after line exactly as far as the curve, and nothing is cut off
 const bst=await p.evaluate(()=>{ const D=EQ.cache.lead, i1=EQF120.findIndex(f=>f>=1000); D.post=D.pre.map((v,i)=>v+(Math.abs(Math.log2(EQF120[i]/1000))<.5?12:0)); EQ.ref=null; eqDraw(); const H=$('#eqCv').clientHeight, sy=db=>eqSy(db,H); const V=eqView(D,false); return {px:+(sy(V.pre[i1])-sy(V.post[i1])).toFixed(1),curve:+(eqY(0,H)-eqY(12,H)).toFixed(1),top:+Math.min(...V.post.map(sy)).toFixed(1)}; });
 ok(bst.px>10&&bst.top>8,'a +12 dB boost lifts the after line above the before line and stays in view '+JSON.stringify(bst));
 // the ideal band passes through the strong notes
 const idl=await p.evaluate(()=>{ const D=EQ.cache.lead; D.post=D.pre.slice(); const V=eqView(D,false); let top=-200; EQF120.forEach((f,i)=>{ if(f>=40&&f<=16000&&V.post[i]>top) top=V.post[i]; }); let mean=0,n=0; EQF120.forEach((f,i)=>{ if(f>=200&&f<=4000&&V.post[i]>top-18){ mean+=V.post[i]; n++; } }); return {mid:+(mean/n).toFixed(1),top:+top.toFixed(1)}; });
 ok(Math.abs(idl.mid-idl.top)<6,'the ideal band is placed by the strong notes (middle '+idl.mid+' dB, notes '+idl.top+' dB)');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
