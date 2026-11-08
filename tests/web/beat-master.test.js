// Drum rows (drop, add, delete), the master bus and meters, sidechain, the tighter beat header.
const {chromium}=require('playwright'), path=require('path'), fs=require('fs'), os=require('os');
const wav=()=>{ const sr=44100,n=4410,b=Buffer.alloc(44+n*2); b.write('RIFF',0); b.writeUInt32LE(36+n*2,4); b.write('WAVEfmt ',8); b.writeUInt32LE(16,16); b.writeUInt16LE(1,20); b.writeUInt16LE(1,22); b.writeUInt32LE(sr,24); b.writeUInt32LE(sr*2,28); b.writeUInt16LE(2,32); b.writeUInt16LE(16,34); b.write('data',36); b.writeUInt32LE(n*2,40); for(let i=0;i<n;i++) b.writeInt16LE(Math.round(20000*Math.sin(i/8)*(1-i/n)),44+i*2); return b.toString('base64'); };
(async()=>{ const br=await chromium.launch(), p=await br.newPage({viewport:{width:1440,height:900}}), errs=[]; p.on('pageerror',e=>errs.push(e.message));
 let bad=0; const ok=(c,m,x)=>{ if(!c){ bad++; console.log('FAIL',m,x===undefined?'':JSON.stringify(x)); } };
 await p.addInitScript(()=>{window.__noStart=1});
 await p.goto('file://'+path.resolve(__dirname,'../../web/semitone-studio.html')); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ const v=document.querySelector('.startview'); if(v) v.hidden=true; shellDock('beat'); });
 // header: nothing but one row of controls
 const head=await p.evaluate(()=>{ const a=$('#beatPanel .beat-top').getBoundingClientRect(), s=$('#seq').getBoundingClientRect(), bt=$('#beatLoop').getBoundingClientRect(), g=$('#beatTabs').getBoundingClientRect(); return {gap:a.top-$('#beatPanel').getBoundingClientRect().top, same:Math.abs(bt.bottom-g.bottom)<30, h:a.height, seq:s.top-a.bottom}; });
 ok(head.gap<24&&head.same&&head.h<70,'beat header is one row',head);
 const drop=async(sel,name='My Clap.wav')=>{ await p.evaluate(async({sel,b64,name})=>{ const u=Uint8Array.from(atob(b64),c=>c.charCodeAt(0)), f=new File([u],name,{type:'audio/wav'}), dt=new DataTransfer(); dt.items.add(f); const el=document.querySelector(sel), o={bubbles:true,cancelable:true,dataTransfer:dt}; el.dispatchEvent(new DragEvent('dragover',o)); el.dispatchEvent(new DragEvent('drop',o)); },{sel,b64:wav(),name}); await p.waitForTimeout(400); };
 await drop('.seq-row[data-row="clap"] .rl');
 const a=await p.evaluate(()=>({smp:(drumSmp('clap')||{}).name,btn:document.querySelector('.seq-row[data-row="clap"] [data-load]').textContent,rows:ROWS.length}));
 ok(a.smp==='My Clap'&&/My Clap/.test(a.btn)&&a.rows===7,'drop on a row replaces its sound',a);
 await drop('#seqAdd','Big Tom.wav');
 const b=await p.evaluate(()=>({rows:ROWS.slice(),name:rowInfo(ROWS[ROWS.length-1])[0],on:!!drumSmp(ROWS[ROWS.length-1]),html:$('#seq').textContent.includes('Big Tom')}));
 ok(b.rows.length===8&&b.name==='Big Tom'&&b.on&&b.html,'drop under the list adds a row',b);
 // the new row plays, in MIDI too, and comes back from a save
 const c=await p.evaluate(()=>{ const r=ROWS[7]; S.beats[S.beatSel][r][0]=2; renderSeq(); const bar=barList('song').find(x=>x.type===S.beatSel); const hit=drumPerf(bar).some(e=>e.r===r); const d=JSON.parse(JSON.stringify(projectData())); return {hit,gm:gmOf(r),saved:d.drumRows.length,sm:!!d.drumSmp[r],name:d.rowNames[r]}; });
 ok(c.hit&&c.gm>0&&c.saved===8&&c.sm&&c.name==='Big Tom','new row plays and saves',c);
 ok(await p.evaluate(()=>{ try{ buildMidi(); return true; }catch(e){ return e.message; } })===true,'MIDI export with a custom row');
 // click a row, delete it, undo
 await p.click('.seq-row[data-row="clap"] .rl-n'); await p.waitForTimeout(100);
 ok(await p.isVisible('#rowPop [data-rowdo="del"]'),'clicking the name offers delete');
 await p.click('#rowPop [data-rowdo="del"]'); await p.waitForTimeout(250);
 const dl=await p.evaluate(()=>({has:ROWS.includes('clap'),row:!!document.querySelector('.seq-row[data-row="clap"]'),back:!!document.querySelector('[data-rowback="clap"]'),ev:barList('song').flatMap(drumPerf).some(e=>e.r==='clap')}));
 ok(!dl.has&&!dl.row&&dl.back&&!dl.ev,'row is gone and silent',dl);
 await p.evaluate(()=>{ S.beats.verse.snare[3]=1; S.drumRows=ROWS.filter(x=>x!=='snare'); buildBeats(S.groove); });
 ok(await p.evaluate(()=>barList('song').flatMap(drumPerf).every(e=>e.r!=='snare')),'fills skip a deleted snare');
 await p.evaluate(()=>{ S.drumRows=[...ROWS,'snare']; renderSeq(); });
 await p.click('[data-rowback="clap"]'); await p.waitForTimeout(150);
 ok(await p.evaluate(()=>ROWS.indexOf('clap'))>=0,'add back');
 // master volume in the transport, mixer master pinned
 await p.evaluate(()=>shellDock('mix')); await p.waitForTimeout(200);
 await p.evaluate(()=>{ const r=$('#trVol'); r.value=.5; r.oninput(); r.onchange(); }); await p.waitForTimeout(350);
 const m=await p.evaluate(()=>({v:strip('song','master').vol,head:$('#trVolOut').textContent,strip:document.querySelector('#cMix .strip[data-ch="master"] .st-gain').textContent,pin:!!document.querySelector('.mix-master .strip[data-ch="master"]'),inGrid:!!document.querySelector('.strips .strip[data-ch="master"]'),vus:document.querySelectorAll('#cMix .vu').length,strips:document.querySelectorAll('#cMix .strip').length}));
 ok(m.v===.5&&/6\.0 dB/.test(m.head)&&m.head===m.strip,'one master volume',m); ok(m.pin&&!m.inGrid&&m.vus===m.strips&&m.vus>4,'master pinned, a meter on every strip',m);
 // meters move while it plays
 await p.evaluate(()=>{ S.parts.drums.on=true; play('song'); }); await p.waitForTimeout(1600);
 const v=await p.evaluate(()=>({m:MT['song:master']&&MT['song:master'].max,d:MT['song:drums']&&MT['song:drums'].max,pk:document.querySelector('#cMix .strip[data-ch="drums"] .st-peak').textContent,w:getComputedStyle(document.querySelector('.tr-vol .vu i')).getPropertyValue('--p')}));
 await p.evaluate(()=>stopPlay());
 ok(v.m>-60&&v.d>-60&&v.pk!=='−∞'&&parseFloat(v.w)>0,'meters read the song',v);
 await p.click('#cMix .strip[data-ch="drums"] .st-peak'); ok(await p.evaluate(()=>MT['song:drums'].max===-100),'click resets the peak');
 // sidechain
 await p.click('#cMix .strip[data-ch="bass"] [data-stmore]'); await p.waitForTimeout(80); await p.evaluate(()=>[...document.querySelectorAll('#ctxMenu [data-ci]')].find(x=>/Sidechain to Kick/.test(x.textContent)).click()); await p.waitForTimeout(150);
 const sc=await p.evaluate(()=>({d:strip('song','bass').duck,ctl:(rtOpen('bass',document.querySelector('#cMix [data-stmore="bass"]')),document.querySelectorAll('#rtCard [data-rtsc], #rtCard [data-rtsca]').length),none:(rtClose(),true)}));
 ok(sc.d&&sc.d.row==='kick'&&sc.ctl===2&&sc.none,'sidechain on a strip',sc);
 const db=await p.evaluate(async()=>{ const sr=22050, sec=2.5, off=new OfflineAudioContext(2,sr*sec,sr), real=ac, rm=master; ac=off; master=newMaster('song'); try{ const o=off.createConstantSource(); o.offset.value=.3; o.connect(getBus('song','bass')); o.start(); PLAY_T0=.1; const bar=barList('song').find(x=>drumPerf(x).some(e=>e.r==='kick')); duckSchedule(bar,.1,.5,0); const kick=drumPerf(bar).filter(e=>e.r==='kick').map(e=>.1+e.x*.5+e.dt); const out=master.buses['song:bass'].duck; const mp=off.createGain(); out.connect(mp); const dest=off.createGain(); const buf=await off.startRendering(); const d=buf.getChannelData(0), rms=(a,b)=>{ let e=0; for(let i=a;i<b;i++) e+=d[i]*d[i]; return Math.sqrt(e/(b-a)); }, t=kick[0], i=Math.floor((t+.012)*sr); return {during:rms(i,i+40),before:rms(Math.floor(.02*sr),Math.floor(.08*sr)),hits:kick.length}; } finally { ac=real; master=rm; PLAY_T0=null; } });
 ok(db.during<db.before*.65,'the part really dips at a kick',db);
 const nd=await p.evaluate(()=>{ const q=nativeDuck(); return {n:q.length,beats:q[0].beats.length,ch:q[0].ch,info:nativeSoundInfo().duck.length}; });
 ok(nd.n===1&&nd.beats>4&&nd.ch===2&&nd.info===1,'sidechain reaches the engine',nd);
 // undo restores a deleted row
 await p.evaluate(()=>{ H.quietUntil=0; rowDelete('kick'); }); await p.waitForTimeout(200);
 ok(await p.evaluate(()=>!ROWS.includes('kick')&&!strip('song','bass').duck),'deleting the key row clears the sidechain');
 await p.evaluate(()=>$('#histUndo').click()); await p.waitForTimeout(500);
 ok(await p.evaluate(()=>ROWS.includes('kick')&&strip('song','bass').duck&&strip('song','bass').duck.row==='kick'),'undo brings the row and sidechain back');
 ok(await p.isVisible('#fsBtn'),'full screen button');
 ok(!errs.length,'no page errors',errs.slice(0,3));
 await br.close(); console.log(bad?`${bad} failed`:'beat-master ok'); process.exit(bad?1:0); })();
