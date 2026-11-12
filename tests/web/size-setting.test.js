// 12 Nov, from the gap report: on a 2560x1440 screen the text and knobs are tiny. The theme card has a Size setting (100/115/125/150%).
// At a larger size everything still works where you point: a note drawn lands on the pitch under the pointer, nothing runs off the
// window, the setting is remembered, and a window too small for a size never uses it.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const errs=[], fails=[]; const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 const open=async(w,h,pre)=>{ const p=await b.newPage({viewport:{width:w,height:h}}); p.on('pageerror',e=>errs.push(e.message)); await p.addInitScript(pre=>{ window.__noStart=1; try{ localStorage.clear(); if(pre) localStorage.setItem('ss_zoom',pre); }catch(e){} },pre||''); await p.goto(url); await p.waitForTimeout(1500); await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; }); return p; };
 const p=await open(2560,1440);
 const base=await p.evaluate(()=>({btn:$('#lensMix').getBoundingClientRect().height,font:parseFloat(getComputedStyle($('#lensMix')).fontSize)}));
 await p.click('#themeBtn'); await p.waitForTimeout(300);
 const row=await p.evaluate(()=>[...document.querySelectorAll('#themeCard [data-zoom]')].map(x=>x.textContent+(x.disabled?' off':'')+(x.getAttribute('aria-pressed')==='true'?' *':'')));
 ok(row.join(' ')==='100% * 115% 125% 150%','the theme card has a Size row: '+row.join(' '));
 await p.click('#themeCard [data-zoom="150"]'); await p.waitForTimeout(600);
 const z=await p.evaluate(()=>({zoom:document.documentElement.style.zoom,ls:localStorage.getItem('ss_zoom'),btn:$('#lensMix').getBoundingClientRect().height,over:document.documentElement.scrollWidth>innerWidth+2,pressed:(document.querySelector('#themeCard [data-zoom="150"]')||{getAttribute(){}}).getAttribute('aria-pressed')}));
 ok(z.zoom==='1.5'&&z.ls==='150'&&z.pressed==='true','choosing 150% is applied at once and remembered');
 ok(z.btn>base.btn*1.3,'the controls are larger on screen (a header button is '+base.btn.toFixed(0)+' px, now '+z.btn.toFixed(0)+' px high)');
 ok(!z.over,'and nothing runs off the side of the window');
 await p.keyboard.press('Escape'); await p.mouse.click(1200,700); await p.waitForTimeout(200);
 // pointing still works: draw notes in the piano roll where the rows are
 await p.evaluate(()=>{ blankSong(); selectSection(1); shellDock('roll'); setLayer('melody'); S.ctxOn=false; renderRoll(); }); await p.waitForTimeout(400);
 const pts=await p.evaluate(()=>{ const L=S.rl, r=$('#roll').getBoundingClientRect(), k=r.width/L.W; return [[67,0],[72,2],[60,4]].map(([m,beat])=>({m,x:r.left+(beat*(L.W/L.TB)+5)*k,y:r.top+(L.top+(L.hi-m)*L.rh+L.rh/2)*k})); });
 for(const q of pts){ await p.mouse.dblclick(q.x,q.y); await p.waitForTimeout(280); }
 const got=await p.evaluate(()=>matOfSel().melody.map(n=>n.m+'@'+n.start));
 ok(got.join()==='67@0,72@2,60@4','at 150% a note drawn lands on the pitch and beat under the pointer ('+got.join(' ')+')');
 // a click on a control lands on that control
 await p.click('#lensMix'); await p.waitForTimeout(400); ok(await p.evaluate(()=>HEAD.lens)==='mix','and buttons answer where they are drawn');
 await p.click('#themeBtn'); await p.waitForTimeout(200); await p.click('#themeCard [data-zoom="100"]'); await p.waitForTimeout(400);
 ok(await p.evaluate(()=>document.documentElement.style.zoom==='')&&Math.abs(await p.evaluate(()=>$('#lensMix').getBoundingClientRect().height)-base.btn)<1,'100% puts everything back');
 // remembered on the next start; a small window never uses a size that would cramp it
 const big=await open(2560,1440,'125'); ok(await big.evaluate(()=>document.documentElement.style.zoom)==='1.25','the size is used again the next time the app opens');
 const small=await open(1440,900,'150'); const s=await small.evaluate(()=>{ themeToggle(true); return {zoom:document.documentElement.style.zoom,off:[...document.querySelectorAll('#themeCard [data-zoom]')].filter(x=>x.disabled).map(x=>x.textContent).join(' '),over:document.documentElement.scrollWidth>innerWidth+2}; });
 ok(s.zoom===''&&/150%/.test(s.off)&&!s.over,'in a 1440-wide window 150% is not used (it would cramp the screen) and its button says so (unavailable: '+s.off+')');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
