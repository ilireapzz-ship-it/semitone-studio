// The piano roll tool bar: default order, drag to reorder, the Tool Pool window, right-click and ⚙ entry points.
const path=require('path'), {chromium}=require('playwright');
let fails=0; const ok=(c,m,x)=>{ if(!c){ fails++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
(async()=>{ const b=await chromium.launch(), p=await b.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 await p.addInitScript(()=>{ window.__noStart=1; try{ if(!sessionStorage.getItem('kept')) localStorage.clear(); }catch(e){} }); await p.goto('file://'+path.join(__dirname,'..','..','web','semitone-studio.html')); await p.waitForTimeout(1200);
 const prep=()=>p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; shellDock('roll'); setLayer('melody'); S.ctxOn=false; partHear=()=>{}; }); await prep(); await p.waitForTimeout(300);
 const order=()=>p.evaluate(()=>[...new Set([...document.querySelectorAll('#rollRail [data-rid]')].map(e=>e.dataset.ri+':'+e.dataset.rid))].map(x=>x.split(':')[1]));
 // default order: Select, Slice, Articulation mode first; the rest as before
 const o=await order(); ok(o.slice(0,5).join()==='select,slice,art,draw,erase','default order starts Select, Slice, Articulation',o);
 ok(o.join()==='select,slice,art,draw,erase,|,snap,quant,|,up,down,legato,dup,all,|,fit','rest of the default kept',o);
 // drag Fit to the top of the bar
 const pt=sel=>p.evaluate(s=>{ const r=document.querySelector(s).getBoundingClientRect(); return {x:r.left+r.width/2,y:r.top+r.height/2,t:r.top}; },sel);
 let a=await pt('#rollRail [data-rid="fit"]'), z=await pt('#rollRail [data-rid="select"]');
 await p.mouse.move(a.x,a.y); await p.mouse.down(); await p.mouse.move(a.x,a.y-20,{steps:3}); await p.mouse.move(z.x,z.t+3,{steps:6}); await p.mouse.up(); await p.waitForTimeout(150);
 ok((await order())[0]==='fit','drag reorders the bar',await order());
 // a drag does not also press the tool
 ok(await p.evaluate(()=>!document.querySelector('#rollRail [data-rid="fit"]').classList.contains('x')),'');
 // a plain click still works: Draw tool
 await p.click('#rollRail [data-rid="draw"]'); ok(await p.evaluate(()=>S.rollTool==='draw'),'click still picks a tool');
 // right-click the bar: Tool Pool item
 a=await pt('#rollRail [data-rid="quant"]'); await p.mouse.click(a.x,a.y,{button:'right'}); await p.waitForTimeout(80);
 const items=await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.textContent.trim()));
 ok(items[0]==='Tool Pool…'&&items.some(t=>/Put Quantize back/.test(t)),'right-click offers Tool Pool',items);
 await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].find(b=>b.textContent.trim()==='Tool Pool…').click()); await p.waitForTimeout(100);
 ok(await p.evaluate(()=>!$('#toolPool').hidden&&document.querySelectorAll('#tpBar .tp-card').length===16),'Tool Pool opens with the bar listed');
 // pool lists the extra tools; click + adds Copy at the end
 ok(await p.evaluate(()=>['copy','paste','del','semiUp','left','undo'].every(id=>$(`#tpPool [data-pid="${id}"]`))),'pool holds tools not on the bar');
 await p.evaluate(()=>$('#tpPool [data-pid="copy"] [data-tp="add"]').click()); await p.waitForTimeout(60);
 ok((await order()).slice(-1)[0]==='copy'&&await p.evaluate(()=>!$('#tpPool [data-pid="copy"]')),'+ adds a tool to the bar');
 // drag Paste from the pool onto the rail between select and slice
 await p.evaluate(()=>{ const c=$('#tpPool [data-pid="paste"]'), L=$('#tpPool'); L.scrollTop+=c.getBoundingClientRect().top-L.getBoundingClientRect().top-L.clientHeight/2; }); a=await pt('#tpPool [data-pid="paste"]'); z=await pt('#rollRail [data-rid="slice"]');
 await p.mouse.move(a.x,a.y); await p.mouse.down(); await p.mouse.move(a.x-30,a.y,{steps:3}); await p.mouse.move(z.x,z.t+2,{steps:8}); await p.mouse.up(); await p.waitForTimeout(120);
 let O=await order(); ok(O[O.indexOf('slice')-1]==='paste','drag from the pool onto the bar',O);
 // drag Legato off the bar into the pool removes it
 a=await pt('#rollRail [data-rid="legato"]'); z=await pt('#tpPool');
 await p.mouse.move(a.x,a.y); await p.mouse.down(); await p.mouse.move(a.x+30,a.y,{steps:3}); await p.mouse.move(z.x,z.y,{steps:8}); await p.mouse.up(); await p.waitForTimeout(120);
 ok(!(await order()).includes('legato')&&await p.evaluate(()=>!!$('#tpPool [data-pid="legato"]')),'drag into the pool takes it off the bar');
 // move buttons and the undo chip
 O=await order(); const i=O.indexOf('quant'); await p.evaluate(i=>$(`#tpBar [data-ri="${i}"] [data-tp="up"]`).click(),i); await p.waitForTimeout(40);
 ok((await order())[i-1]==='quant','↑ moves a tool up');
 await p.click('#tpUndo'); ok((await order())[i]==='quant','Undo puts the bar back');
 // search filters
 await p.fill('#tpFind','semitone'); ok(await p.evaluate(()=>[...document.querySelectorAll('#tpPool .tp-card:not([hidden])')].map(c=>c.dataset.pid).sort().join()==='semiDown,semiUp'),'search finds tools');
 await p.fill('#tpFind','');
 // the new tools work: semitone up on the selection
 await p.evaluate(()=>$('#tpPool [data-pid="semiUp"] [data-tp="add"]').click());
 const su=await p.evaluate(()=>{ const a=rollArr().slice(0,2); S.selSet=new Set(a); const m=a.map(n=>n.m); $('#rollRail [data-rid="semiUp"]').click(); return a.map((n,i)=>n.m-m[i]); }); ok(su.every(d=>d===1),'Semitone up tool moves notes',su);
 // Articulation mode off the bar turns it off
 await p.evaluate(()=>{ if(!S.artMode) $('#artModeBtn').click(); }); await p.evaluate(()=>$(`#tpBar [data-rid="art"] [data-tp="rm"]`).click()); ok(await p.evaluate(()=>!S.artMode&&!$('#artModeBtn')),'removing Articulation mode switches it off');
 // presets
 await p.evaluate(()=>document.querySelector('#toolPool [data-p="starter"]').click()); ok((await order()).join()==='select,draw,erase,slice,|,snap,|,undo,redo,|,fit','Starter set');
 await p.evaluate(()=>document.querySelector('#toolPool [data-p="default"]').click()); ok((await order()).slice(0,3).join()==='select,slice,art','Default set');
 // Esc closes; the gear menu and Options both open it
 await p.focus('#tpFind'); await p.keyboard.press('Escape'); ok(await p.evaluate(()=>$('#toolPool').hidden),'Esc closes the pool');
 await p.evaluate(()=>{ const b=$('#menuBar [data-mb="all"]'); b.click(); }); await p.waitForTimeout(60);
 const top=await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].map(b=>b.textContent.trim())); ok(top.includes('Tool Pool…'),'⚙ menu has Tool Pool',top);
 await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].find(b=>b.textContent.trim()==='Tool Pool…').click()); await p.waitForTimeout(80); ok(await p.evaluate(()=>!$('#toolPool').hidden),'⚙ opens the Tool Pool');
 ok(await p.evaluate(()=>mbItems('Options').some(x=>Array.isArray(x)&&x[0]==='Tool Pool…')),'Options has Tool Pool');
 ok(await p.evaluate(()=>{ const P=$('#toolPool').getBoundingClientRect(); return ['#tpBar','#tpPool'].every(s=>{ const r=$(s).getBoundingClientRect(); return r.left>=P.left&&r.right<=P.right+1; }); }),'both columns sit inside the window');
 // the panel fits a small window
 await p.setViewportSize({width:900,height:600}); await p.waitForTimeout(100); await p.evaluate(()=>{ tpClose(); tpOpen(); });
 ok(await p.evaluate(()=>{ const r=$('#toolPool').getBoundingClientRect(); return r.left>=0&&r.right<=innerWidth&&r.top>=0&&r.bottom<=innerHeight; }),'pool fits at 900×600');
 // the order is kept after a reload
 await p.evaluate(()=>{ railMove(0,1); sessionStorage.setItem('kept','1'); }); const before=await order(); await p.reload(); await p.waitForTimeout(1200); await prep(); await p.waitForTimeout(200);
 ok((await order()).join()===before.join(),'order kept after reopening',[await order(),before]);
 ok(!errs.length,'no page errors',errs); await b.close(); console.log(fails?`tool-pool: ${fails} failed`:'tool-pool ok'); process.exit(fails?1:0); })();
