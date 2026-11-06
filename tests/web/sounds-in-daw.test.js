const {chromium}=require('playwright');
(async()=>{ const b=await chromium.launch({args:['--autoplay-policy=no-user-gesture-required']}); const p=await b.newPage({viewport:{width:1300,height:900}}); const errs=[]; p.on('pageerror',e=>errs.push('PAGEERR '+e.message+' '+(e.stack||'').split('\n')[1])); p.on('console',m=>{ if(m.type()==='warning'||m.type()==='error') errs.push(m.text()); });
 await p.addInitScript(()=>{ window.__noStart=1; window.__nlog=[]; const H={}; window.__JUCE__={backend:{emitEvent:(id,o)=>__nlog.push([id,o]),addEventListener:(id,fn)=>{(H[id]=H[id]||[]).push(fn);}}}; window.__fire=(id,o)=>(H[id]||[]).forEach(f=>f(o)); });
 await p.goto('file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html')+''); await p.waitForTimeout(1500);
 await p.evaluate(()=>__fire('st_init',{plugin:true,host:'FL Studio',project:'',soundMode:0,sr:44100})); await p.waitForTimeout(3500);
 const lay=await p.evaluate(()=>{ const L=__nlog.filter(x=>x[0]==='st_audio_layout'); return {count:L.length,last:L.pop()[1],secs:S.song.sections.map(s=>s.type+s.bars)}; });
 console.log('layouts sent',lay.count,'items',JSON.stringify(lay.last.items),lay.secs.join(' '));
 const t0=Date.now();
 await p.evaluate(l=>__fire('st_audio_need',{ids:[...new Set(l.items.map(x=>x.id))],sr:44100,seq:l.seq}),lay.last);
 await p.waitForFunction(()=>!NSND.busy&&!NSND.queue.length&&__nlog.some(x=>x[0]==='st_audio_clip'),null,{timeout:120000}); await p.waitForTimeout(500);
 console.log('render+send ms',Date.now()-t0,'status',await p.evaluate(()=>$('#nbSnd').textContent));
 const info=await p.evaluate(()=>{ const C=__nlog.filter(x=>x[0]==='st_audio_clip').map(x=>x[1]); const by={}; C.forEach(c=>{ (by[c.id]=by[c.id]||[]).push(c); });
   return Object.entries(by).map(([id,cs])=>{ const f=cs[0]; let sum=0,n=0,mx=0; cs.forEach(c=>{ const bin=atob(c.b64); for(let i=0;i<bin.length;i+=2){ let v=bin.charCodeAt(i)|(bin.charCodeAt(i+1)<<8); if(v>32767) v-=65536; sum+=v*v; n++; mx=Math.max(mx,Math.abs(v)); } });
     return id.slice(0,6)+' sr'+f.sr+' frames'+f.frames+' parts'+cs.length+'/'+f.parts+' marks'+f.marks.length+' secs'+(f.frames/f.sr).toFixed(1)+' rms'+Math.sqrt(sum/n).toFixed(0)+' peak'+mx+' totalB64 '+(cs.reduce((a,c)=>a+c.b64.length,0)/1e6).toFixed(1)+'MB'; }); });
 console.log(info.join('\n'));
 // stable: no new layout without changes
 const n1=await p.evaluate(()=>__nlog.filter(x=>x[0]==='st_audio_layout').length); await p.waitForTimeout(2500); const n2=await p.evaluate(()=>__nlog.filter(x=>x[0]==='st_audio_layout').length); console.log('stable layouts',n1,n2);
 // change a chord in section 0's material → which ids change?
 const before=lay.last.items.map(x=>x.id);
 await p.evaluate(()=>{ S.bpm=S.bpm+0; S.song.sections[0].energy=0.31; }); await p.waitForTimeout(1500);
 const after=await p.evaluate(()=>__nlog.filter(x=>x[0]==='st_audio_layout').pop()[1].items.map(x=>x.id));
 console.log('changed ids after section-0 energy edit:',after.map((x,i)=>x!==before[i]?'*':'.').join(''));
 // mode select
 await p.evaluate(()=>{ const e=$('#nbSound'); e.value='2'; e.dispatchEvent(new Event('change',{bubbles:true})); }); await p.waitForTimeout(300); console.log('midi-only:',await p.evaluate(()=>JSON.stringify(__nlog.filter(x=>x[0]==='st_soundmode').pop()[1])+' layout items '+__nlog.filter(x=>x[0]==='st_audio_layout').pop()[1].items.length));
 await p.screenshot({path:require('path').join(require('os').tmpdir(),'nsnd.png'),clip:{x:0,y:0,width:1300,height:120}});
 console.log(errs.slice(0,8)); await b.close(); })();
