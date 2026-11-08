// Click track (docs/design/panels §2): C / ⇧C, the "when" modes, on the song's grid (tempo, metre, swing), your sample, never in exports, flash on beat.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.removeItem('ss_click'); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; ensureAudio(); });
 const t0=await p.evaluate(()=>({btn:!!$('#clkBtn'),label:$('#clkBtn').textContent,when:CLK.when,on:$('#clkBtn').classList.contains('on')}));
 ok(t0.btn&&t0.when==='rec'&&/Click · rec/.test(t0.label)&&t0.on,'one Click button in the transport, on “only while recording” by default ('+t0.label+')');
 await p.click('#arr'); await p.keyboard.press('c'); const c1=await p.evaluate(()=>[CLK.when,$('#clkBtn').getAttribute('aria-pressed')]);
 await p.keyboard.press('c'); const c2=await p.evaluate(()=>CLK.when);
 ok(c1[0]==='off'&&c1[1]==='false'&&c2==='rec','C turns it off and back on ('+c1[0]+' → '+c2+')');
 await p.keyboard.press('Shift+C'); const c3=await p.evaluate(()=>CLK.when); await p.keyboard.press('Shift+C'); const c4=await p.evaluate(()=>CLK.when);
 ok(c3==='always'&&c4==='count','⇧C cycles when: rec → '+c3+' → '+c4);
 // grid positions per metre and pattern
 const pos=await p.evaluate(()=>({q:clkPositions('4/4','accent').map(x=>x.x+(x.a?'!':'')).join(' '),six:clkPositions('6/8','accent').map(x=>x.x).join(' '),twelve:clkPositions('12/8','every').map(x=>x.x).join(' '),e8:clkPositions('4/4','eighths').length,e68:clkPositions('6/8','eighths').length,bars:clkPositions('3/4','bars').length}));
 ok(pos.q==='0! 1 2 3','4/4 accent bar 1: '+pos.q); ok(pos.six==='0 1.5'&&pos.twelve==='0 1.5 3 4.5','6/8 and 12/8 click the dotted-quarter pulse ('+pos.six+' | '+pos.twelve+')');
 ok(pos.e8===8&&pos.e68===6&&pos.bars===1,'8ths clicks every eighth (8 in 4/4, 6 in 6/8); Bars only clicks the downbeat');
 // what gets scheduled: record the click times instead of sounding them
 const sched=await p.evaluate(()=>{ const got=[], keep=clkVoice; clkVoice=(t,a)=>got.push([+(t-1000).toFixed(4),a]); const run=(o)=>{ got.length=0; Object.assign(CLK,o.clk||{}); Object.assign(S,o.s||{}); LIVE.rec=!!o.rec; S.rendering=!!o.render; clkBar({si:0,b:0},1000,o.spb||.5,0); return got.map(x=>x[0]+(x[1]?'!':'')).join(' '); };
   ensureAudio(); const r={ recOnly:run({clk:{when:'rec',pattern:'accent'},s:{meter:'4/4',swing:50}}), recOn:run({clk:{when:'rec'},rec:true}), always:run({clk:{when:'always'}}), fast:run({clk:{when:'always'},spb:.25}), export_:run({clk:{when:'always'},render:true}),
     waltz:run({clk:{when:'always'},s:{meter:'3/4'}}), groove:run({clk:{when:'always',pattern:'groove'},s:{meter:'4/4',swing:66}}), straight8:run({clk:{when:'always',pattern:'eighths'},s:{swing:66}}), count:run({clk:{when:'count',pattern:'accent'}}), sw:+(swingT(.5)*.5).toFixed(4) };
   clkVoice=keep; LIVE.rec=false; S.rendering=false; S.swing=50; S.meter='4/4'; return r; });
 ok(sched.recOnly===''&&sched.recOn==='0! 0.5 1 1.5','“only while recording” is silent in playback and clicks while recording: '+JSON.stringify([sched.recOnly,sched.recOn]));
 ok(sched.always==='0! 0.5 1 1.5'&&sched.fast==='0! 0.25 0.5 0.75','follows the tempo of each bar ('+sched.fast+' at twice the speed)');
 ok(sched.waltz==='0! 0.5 1','metre change: 3 clicks in a 3/4 bar');
 ok(sched.export_==='','never in exports (silent while rendering)'); ok(sched.count==='','“count-in only” stays silent in the song');
 const off=sched.groove.split(' ')[1]; ok(Math.abs(parseFloat(off)-sched.sw)<1e-3&&sched.sw>.25,`Follow groove lands the off-beat on the swing (${off} s, swing at ${sched.sw})`);
 ok(sched.straight8.split(' ')[1]==='0.25','plain 8ths are straight even with swing on');
 // count-in uses the click sound, 1 or 2 bars
 const ci=await p.evaluate(async()=>{ const got=[], keep=clkVoice; clkVoice=(t,a)=>got.push(a); CLK.count=2; CLK.pattern='accent'; S.bpm=240; let done=false; clkCountIn(()=>done=true); await new Promise(r=>setTimeout(r,2400)); clkVoice=keep; CLK.count=1; return {n:got.length,acc:got.filter(Boolean).length,done}; });
 ok(ci.n===8&&ci.acc===2&&ci.done,`a 2-bar count-in: ${ci.n} clicks, ${ci.acc} accents, then recording starts`);
 // flash on the beat
 const fl=await p.evaluate(async()=>{ CLK.flash=true; const n0=CLK.flashes||0; clkFlash(ac.currentTime+.02,true); await new Promise(r=>setTimeout(r,60)); const acc=$('#trLcd').classList.contains('clk-fa'); return {n:(CLK.flashes||0)-n0,acc}; });
 ok(fl.n===1&&fl.acc,'the time display flashes on the beat, brass on the accent');
 // your sample, with an accent sample for beat 1
 const smp=await p.evaluate(async()=>{ const wav=(f)=>{ const sr=22050,n=2200,b=new ArrayBuffer(44+n*2),v=new DataView(b),w=(o,s)=>[...s].forEach((c,i)=>v.setUint8(o+i,c.charCodeAt(0))); w(0,'RIFF');v.setUint32(4,36+n*2,true);w(8,'WAVE');w(12,'fmt ');v.setUint32(16,16,true);v.setUint16(20,1,true);v.setUint16(22,1,true);v.setUint32(24,sr,true);v.setUint32(28,sr*2,true);v.setUint16(32,2,true);v.setUint16(34,16,true);w(36,'data');v.setUint32(40,n*2,true); for(let i=0;i<n;i++) v.setInt16(44+i*2,i<200?0:Math.round(Math.sin(i*f)*20000*Math.exp(-(i-200)/400)),true); let s=''; new Uint8Array(b).forEach(x=>s+=String.fromCharCode(x)); return 'data:audio/wav;base64,'+btoa(s); };
   CLK.sample=wav(.3); CLK.sampleAcc=wav(.6); CLK.sound='sample'; CLK.buf={}; await clkDecode('main'); await clkDecode('acc'); const used=[], k=ac.createBufferSource.bind(ac); ac.createBufferSource=()=>{ const s=k(); const st=s.start.bind(s); s.start=(t,o,d)=>{ used.push([s.buffer===CLK.buf.acc.b?'acc':s.buffer===CLK.buf.main.b?'main':'other',+o.toFixed(4)]); st(t,o,d); }; return s; };
   clkVoice(ac.currentTime+.05,true); clkVoice(ac.currentTime+.3,false); ac.createBufferSource=k; clkSave(); const saved=JSON.parse(localStorage.getItem('ss_click')); return {used,trim:+(200/22050).toFixed(4),saved:!!saved.sample&&!!saved.sampleAcc&&saved.sound==='sample'}; });
 ok(smp.used[0][0]==='acc'&&smp.used[1][0]==='main','your accent sample on beat 1, your sample on the others: '+JSON.stringify(smp.used));
 ok(Math.abs(smp.used[1][1]-smp.trim)<.002,'your sample is trimmed to where its sound starts ('+smp.used[1][1]+' s)');
 ok(smp.saved,'click settings, samples included, are kept with your settings');
 // menu + Record's Click segment share the setting
 await p.click('#clkMore'); await p.waitForTimeout(80); const mn=await p.evaluate(()=>({open:!$('#clkMenu').hidden,when:document.querySelectorAll('#clkMenu [data-clkset^="when:"]').length,snd:document.querySelectorAll('#clkMenu [data-clkset^="sound:"]').length,pat:document.querySelectorAll('#clkMenu [data-clkset^="pattern:"]').length,foot:/Never in exports/.test($('#clkMenu').textContent)}));
 ok(mn.open&&mn.when===4&&mn.snd===7&&mn.pat===5&&mn.foot,'the ▾ menu: 4 whens, 7 sounds + your sample, 5 patterns, the never-in-exports line');
 await p.click('#clkMenu [data-clkset="when:off"]'); await p.keyboard.press('Escape');
 await p.evaluate(()=>shellDock('record')); await p.click('[data-clkseg="always"]'); ok(await p.evaluate(()=>CLK.when==='always'&&$('#clkBtn').classList.contains('on')),'Record’s Click segment and the transport button are the same setting');
 await p.screenshot({path:'click-track.png'});
 ok(!errs.length,'no page errors '+errs.slice(0,3).join(' | '));
 await b.close(); if(fails.length){ console.log(fails.length+' failed'); process.exit(1); } })();
