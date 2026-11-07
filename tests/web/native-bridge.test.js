const {chromium}=require('playwright');
(async()=>{ const b=await chromium.launch({args:['--autoplay-policy=no-user-gesture-required']}); const p=await b.newPage({viewport:{width:1300,height:900}}); const errs=[]; p.on('pageerror',e=>errs.push('PAGEERR '+e.message+' '+(e.stack||'').split('\n')[1]));
 await p.addInitScript(()=>{ window.__noStart=1; window.__nlog=[]; const H={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(H[id]=H[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(H[id]||[]).forEach(f=>f(o)); });
 await p.goto('file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html')+''); await p.waitForTimeout(1500);
 console.log('ready sent:',await p.evaluate(()=>__nlog.map(x=>x[0]).join(',')));
 await p.evaluate(()=>__fire('st_init',{plugin:true,host:'FL Studio',project:''})); await p.waitForTimeout(2600);
 console.log('after init:',await p.evaluate(()=>{ const ev=__nlog.map(x=>x[0]); const tl=__nlog.filter(x=>x[0]==='st_timeline').pop(); return ev.join(',')+' | bar: '+$('#nativeBar').textContent.replace(/\s+/g,' ').slice(0,120)+' | timeline len '+tl[1].length+' notes '+tl[1].notes.length/5+' channels '+[...new Set(tl[1].notes.filter((_,i)=>i%5===2))].join(','); }));
 // host playing
 await p.evaluate(()=>{ for(let i=0;i<5;i++) __fire('st_host',{playing:true,ppq:20+i,bpm:100,hostTransport:true}); }); console.log('host bar:',await p.evaluate(()=>$('#nbState').textContent+' | tempo btn hidden '+$('#nbTempo').hidden+' '+$('#nbTempo').textContent+' | sel '+S.sel));
 await p.evaluate(()=>$('#nbTempo').click()); console.log('bpm matched',await p.evaluate(()=>S.bpm));
 // section scope
 await p.evaluate(()=>{ const e=$('#nbScope'); e.value='sec'; e.dispatchEvent(new Event('change',{bubbles:true})); }); await p.waitForTimeout(900); console.log('sec timeline:',await p.evaluate(()=>{ const tl=__nlog.filter(x=>x[0]==='st_timeline').pop()[1]; return 'len '+tl.length+' offset '+tl.offset; }));
 // state + restore
 const st=await p.evaluate(()=>{ const s=__nlog.filter(x=>x[0]==='st_state').pop(); return s?s[1].json:null; }); console.log('state bytes',st&&st.length);
 // save + drag
 await p.evaluate(async()=>{ await downloadsCap.save({filename:'x.mid',data:new Uint8Array([1,2,3])}); }); 
 await p.evaluate(()=>{ const c=document.querySelector('.dx-drag'); const dt=new DataTransfer(); c.dispatchEvent(new DragEvent('dragstart',{bubbles:true,cancelable:true,dataTransfer:dt})); });
 console.log('save/drag:',await p.evaluate(()=>__nlog.filter(x=>x[0]==='st_save'||x[0]==='st_drag').map(x=>x[0]+':'+x[1].name+':'+x[1].b64.length).join(' ')));
 console.log('mout panel:',await p.evaluate(()=>document.querySelector('.mout-box').textContent.slice(0,90)));
 // restore into a fresh page
 const p2=await b.newPage(); await p2.addInitScript(()=>{ window.__noStart=1; window.__nlog=[]; const H={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(H[id]=H[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(H[id]||[]).forEach(f=>f(o)); });
 await p2.goto('file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html')+''); await p2.waitForTimeout(1200); const before=await p2.evaluate(()=>S.seed);
 await p2.evaluate(j=>__fire('st_init',{plugin:true,host:'FL Studio',projectB64:btoa(unescape(encodeURIComponent(j)))}),st); await p2.waitForTimeout(300); console.log('restored seed',before,'->',await p2.evaluate(()=>S.seed+' bpm '+S.bpm));
 // standalone transport
 const p3=await b.newPage(); await p3.addInitScript(()=>{ window.__noStart=1; window.__nlog=[]; const H={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(H[id]=H[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(H[id]||[]).forEach(f=>f(o)); });
 await p3.goto('file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html')+''); await p3.waitForTimeout(1200); await p3.evaluate(()=>__fire('st_init',{plugin:false,host:'',project:''})); await p3.evaluate(()=>{ location.hash='#compose'; play('song'); }); await p3.waitForTimeout(300); await p3.evaluate(()=>stopPlay());
 console.log('standalone:',await p3.evaluate(()=>__nlog.filter(x=>x[0]==='st_transport').map(x=>JSON.stringify(x[1])).join(' ')+' | bar '+$('#nativeBar').textContent.replace(/\s+/g,' ').slice(0,60)));
 await p.screenshot({path:require('path').join(require('os').tmpdir(),'native.png'),clip:{x:0,y:0,width:1300,height:160}});
 console.log(errs.slice(0,6)); await b.close(); })();
