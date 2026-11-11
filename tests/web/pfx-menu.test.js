// 12 Nov (Jamie: "+ plug-in on a strip" doesn't open the plug-in): with 243 effect plug-ins the list was placed ~6000px above the screen.
// A long menu stays on screen as a scrolling list, with a search box: type, Enter picks the first match, and the effect goes on the strip.
const {chromium}=require('playwright'), path=require('path'); const url='file://'+path.resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:2558,height:1412}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 const at=await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; window.__e=[]; hostsHere=()=>true; nEmit=(n,d)=>__e.push([n,d]); HOSTS.effects=Array.from({length:243},(_,i)=>({id:'VST3-FX-'+i,name:(i===150?'FabFilter Pro-Q 4':i===40?'Valhalla Room':'Effect '+String(i).padStart(3,'0')),maker:'Test'})); const m=[...document.querySelectorAll('button')].find(x=>x.textContent.trim()==='Mix'&&x.offsetParent&&x.getBoundingClientRect().top<60); if(m) m.click(); renderStrips('song'); const btn=document.querySelector('[data-pfxadd="lead"]'), q=btn.getBoundingClientRect(); return {x:q.left+q.width/2,y:q.top+q.height/2,w:q.width}; });
 ok(at.w>0,'the + plug-in button is there '+JSON.stringify(at));
 await p.mouse.click(at.x,at.y); await p.waitForTimeout(300);
 const m=await p.evaluate(()=>{ const m=$('#ctxMenu'), r=m.getBoundingClientRect(); return {top:Math.round(r.top),bottom:Math.round(r.bottom),h:Math.round(r.height),vh:innerHeight,items:m.querySelectorAll('[data-ci]').length,find:!!m.querySelector('.ctx-find input'),focus:document.activeElement&&document.activeElement.closest&&!!document.activeElement.closest('.ctx-find')}; });
 ok(m.top>=0&&m.bottom<=m.vh&&m.items===243&&m.h<=Math.min(440,m.vh*.75),'the list of 243 effects opens on screen as a scrolling list '+JSON.stringify(m));
 ok(m.find&&m.focus,'it has a search box, ready to type in');
 await p.keyboard.type('pro-q'); await p.waitForTimeout(150); const shown=await p.evaluate(()=>[...$('#ctxMenu').querySelectorAll('[data-ci]')].filter(b=>!b.hidden).map(b=>b.textContent));
 ok(shown.length===1&&/FabFilter Pro-Q 4/.test(shown[0]),'typing narrows it to the match '+JSON.stringify(shown));
 await p.keyboard.press('Enter'); await p.waitForTimeout(300);
 const added=await p.evaluate(()=>({fx:((S.hostFx||{}).lead||[]).map(x=>x.name),closed:$('#ctxMenu').hidden,chip:!!document.querySelector('[data-pfxopen="lead:0"]')}));
 ok(added.fx.join()==='FabFilter Pro-Q 4'&&added.closed&&added.chip,'Enter puts it on the Melody strip '+JSON.stringify(added));
 // a short menu has no search box
 await p.evaluate(()=>ctxOpen(100,100,[['One',()=>{}],['Two',()=>{}]])); const short=await p.evaluate(()=>!!$('#ctxMenu .ctx-find')); ok(!short,'short menus stay plain');
 console.log(fails.length?`${fails.length} failed`:'all passed'); console.log(JSON.stringify(errs)); await b.close(); process.exit(fails.length?1:0); })();
