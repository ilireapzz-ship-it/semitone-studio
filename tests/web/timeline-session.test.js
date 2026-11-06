// One timeline with lenses, the five-tab drawer and the Session panel with its rail.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ if(!c) fails.push(m); console.log((c?'ok  ':'FAIL')+' '+m); };
 await p.addInitScript(()=>{ window.__noStart=1; }); await p.goto(url); await p.waitForTimeout(1800);
 const tabs=await p.evaluate(()=>[...document.querySelectorAll('.dock-tabs [data-drawer]')].map(x=>x.textContent)); ok(tabs.join()==='Notes,Words,Sound,Mix,Ideas','drawer tabs '+tabs);
 ok(await p.evaluate(()=>{ const a=[...document.querySelectorAll('#timeline .sec')].map(x=>x.getBoundingClientRect().left), c=[...document.querySelectorAll('#arrRows .arr-row.track')][0].querySelectorAll('.arr-cell'); return [...c].every((x,i)=>Math.abs(x.getBoundingClientRect().left-a[i])<=3); }),'sections and clips line up');
 ok(await p.evaluate(()=>$('#arrRows').textContent.includes('lyrics')),'Write shows lyrics under the melody');
 // click selects: drawer follows
 const c=await p.evaluate(()=>{ document.querySelector('[data-arrpart="drums"][data-arrsec="1"]').click(); return {t:$('#drSel').textContent,beat:!document.querySelector('.dock-pane[data-pane="beat"]').hidden}; }); ok(/Drums/.test(c.t)&&c.beat,'clicking a drum clip opens the Beat editor: '+c.t);
 await p.evaluate(()=>{ document.querySelector('[data-arrpart="lead"][data-arrsec="1"]').click(); });
 // session items
 const it=await p.evaluate(()=>({n:document.querySelectorAll('#ssFeed .ss-item').length,num:[...document.querySelectorAll('.ss-n')].map(x=>x.textContent),pin:document.querySelectorAll('#timeline [data-sspin]').length})); console.log(JSON.stringify(it)); ok(it.n>0&&it.num[0]==='1','Session lists numbered items'); ok(it.pin>0,'the same numbers pin on the timeline');
 await p.evaluate(()=>{ document.querySelector('#timeline [data-sspin]').click(); }); await p.waitForTimeout(80); ok(await p.evaluate(()=>!!document.querySelector('.ss-item.open')),'a pin opens its item');
 // pass on the timeline
 const pa=await p.evaluate(async()=>{ passOpen('antonoff'); await new Promise(r=>setTimeout(r,200)); return {lens:HEAD.lens,ab:!$('#trAB').hidden,ch:document.querySelectorAll('#arrRows .pass-ch').length,mu:document.querySelectorAll('#arrRows .pass-mute').length,marks:document.querySelectorAll('#arrRows .tl-mark').length,vis:$('#passNotes').getClientRects().length>0,dup:[...document.querySelectorAll('.pv-sec')].filter(x=>x.getClientRects().length).length,note:$('#trNote').textContent}; }); console.log(JSON.stringify(pa)); ok(pa.lens==='produce'&&pa.ab,'a pass opens Produce with A/B in the transport'); ok(pa.ch>0&&pa.marks>0,'changes are drawn on the song'); ok(pa.vis,'its moves are in the Session panel'); ok(pa.dup===0,'no second timeline');
 await p.click('#passA'); await p.waitForTimeout(150); ok(await p.evaluate(()=>document.querySelectorAll('#arrRows .pass-ch').length===0),'A shows your version, unmarked'); await p.click('#passB'); await p.waitForTimeout(150);
 await p.evaluate(()=>passDiscard()); await p.waitForTimeout(150); ok(await p.evaluate(()=>$('#trAB').hidden&&!document.querySelector('#arrRows .pass-ch')),'discard clears the marks');
 // old screens live in the drawer
 await p.evaluate(()=>feelOpen()); await p.waitForTimeout(120); ok(await p.evaluate(()=>$('#feelMain').getClientRects().length>0&&!$('#view-compose').hidden&&$('.dock').contains($('#feelMain'))),'Emotion & vibe is in the drawer');
 await p.evaluate(()=>{ $('#soundBtn').click(); }); await p.waitForTimeout(120); ok(await p.evaluate(()=>$('#soundMain').getClientRects().length>0&&$('.dock').contains($('#soundMain'))),'Real sounds is in the drawer');
 await p.click('[data-partpop="chords"]'); await p.waitForTimeout(80); ok(await p.evaluate(()=>!$('#recipePop').hidden),'a part name opens its sound card'); await p.keyboard.press('Escape');
 // rail, per lens
 await p.evaluate(()=>lensShow('write')); await p.click('#ssToggle'); await p.waitForTimeout(250); const w=await p.evaluate(()=>({rail:$('#ssRailWrap').getBoundingClientRect().width,panel:$('#songSide').getBoundingClientRect().width,left:$('.song-left').getBoundingClientRect().width,icons:document.querySelectorAll('#ssRail button').length})); console.log(JSON.stringify(w)); ok(w.panel===58&&w.icons>0,'collapses to a 58px rail');
 await p.evaluate(()=>lensShow('produce')); await p.waitForTimeout(120); ok(await p.evaluate(()=>!$('.song-body').classList.contains('sess-closed')),'open or closed is remembered per lens'); await p.evaluate(()=>lensShow('write')); await p.waitForTimeout(120); ok(await p.evaluate(()=>$('.song-body').classList.contains('sess-closed')),'Write stays closed');
 await p.evaluate(()=>document.querySelector('#ssRail button').click()); await p.waitForTimeout(150); ok(await p.evaluate(()=>!$('.song-body').classList.contains('sess-closed')&&SESS.filter!=='all'),'a rail icon opens that helper');
 await p.keyboard.press('Meta+Alt+s'); await p.waitForTimeout(150); ok(await p.evaluate(()=>$('.song-body').classList.contains('sess-closed')),'⌥⌘S collapses'); await p.keyboard.press('Meta+Alt+s');
 // describe a change
 await p.fill('#ssSay','make the chorus bigger'); await p.click('#ssSayBtn'); await p.waitForTimeout(150); ok(await p.evaluate(()=>$('#ssSayOut').textContent.length>0),'Describe a change answers in the panel');
 // big song
 const big=await p.evaluate(()=>{ const t0=performance.now(); for(let i=0;i<20;i++){ arrRender(); } return (performance.now()-t0)/20; }); console.log('render ms',big.toFixed(1)); ok(big<60,'timeline redraw stays quick');
 console.log(fails.length?fails.length+' FAILED':'all passed'); console.log(errs.slice(0,6)); await b.close(); process.exit(fails.length||errs.length?1:0); })();
