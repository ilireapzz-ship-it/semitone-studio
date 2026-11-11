// 12 Nov, from the gap report: the built-in sounds differ by up to 20 dB on the same notes, so swapping a sound upset the balance.
// After a swap Semitone measures the part alone with the old and the new sound on your own notes and offers "Match the level".
// Nothing changes until you press it; the fader does not move; the engine gets the same level; picking another sound clears it.
const {chromium}=require('playwright'); const url='file://'+require('path').resolve(__dirname,'../../web/semitone-studio.html');
(async()=>{ const b=await chromium.launch(); const p=await b.newPage({viewport:{width:1440,height:900}}); const errs=[], fails=[]; p.on('pageerror',e=>errs.push(e.message)); const ok=(c,m)=>{ console.log((c?'ok   ':'FAIL ')+m); if(!c) fails.push(m); };
 await p.addInitScript(()=>{ window.__noStart=1; try{ localStorage.clear(); }catch(e){} }); await p.goto(url); await p.waitForTimeout(1500);
 await p.evaluate(()=>{ S.demo=false; const r=$('#restoreBar'); if(r) r.hidden=true; selectSection(1); shellDock('mix'); soundSet('lead','bright'); MATCH.q=null; clearTimeout(MATCH.t); });
 await p.waitForTimeout(600);
 const waitOffer=async()=>{ for(let i=0;i<40;i++){ await p.waitForTimeout(250); const s=await p.evaluate(()=>({busy:MATCH.busy,q:!!MATCH.q,last:MATCH.last,btn:!!$('#toastBtn')&&!$('#toast').hidden,txt:$('#toast').textContent})); if(!s.busy&&!s.q&&(s.last||s.btn)) return s; } return null; };
 // a much quieter sound: the offer appears, and nothing has changed yet
 const before=await p.evaluate(()=>({vol:S.parts.lead.vol,trim:JSON.stringify(S.volTrim||{}),past:H.past.length}));
 await p.evaluate(()=>{ MATCH.last=null; soundSet('lead','i:nylon'); });
 const o=await waitOffer(); console.log(JSON.stringify(o));
 ok(o&&o.btn&&/quieter than/.test(o.txt)&&/Match the level/.test(o.txt),'after swapping to a much quieter sound, Semitone offers to match the level: “'+(o?o.txt:'')+'”');
 ok(o&&o.last&&o.last.d>=5,'it measured the difference on the song’s own notes (+'+(o&&o.last?o.last.d:'?')+' dB needed)');
 const mid=await p.evaluate(()=>({sg:S.parts.lead.sg||0,vol:S.parts.lead.vol}));
 ok(mid.sg===0&&mid.vol===before.vol,'nothing changes until you press the button');
 await p.click('#toastBtn'); await p.waitForTimeout(400);
 const done=await p.evaluate(()=>({sg:S.parts.lead.sg||0,vol:S.parts.lead.vol,trim:JSON.stringify(S.volTrim||{}),label:H.past[H.past.length-1].label,strip:(document.querySelector('#cMix .strip[data-ch="lead"] .pt small')||{}).textContent||'',saved:JSON.parse(JSON.stringify(projectData())).parts.lead.sg||0}));
 ok(done.sg===o.last.d&&/level matched/.test(done.label),'pressing it sets the sound’s level (+'+done.sg+' dB) as one undo step: “'+done.label+'”');
 ok(done.vol===before.vol&&done.trim===before.trim,'the fader and its trim have not moved');
 ok(/level \+/.test(done.strip),'the mixer strip says so: “'+done.strip+'”');
 ok(done.saved===done.sg,'and it is saved with the song');
 // it really is as loud as before now, and the engine gets the same level
 const chk=await p.evaluate(async()=>{ const a=await partLoud('lead','bright',0), b=await partLoud('lead','i:nylon',S.parts.lead.sg); const info=nativeSoundInfo(), np=(info.parts||[]).find(x=>x.ch===(info.parts||[])[0].ch); const lv=(info.parts||[]).map(x=>x.level); return {a,b,lv,want:stripVol('song','lead')*sndGain('song','lead')}; });
 ok(chk.a!=null&&chk.b!=null&&Math.abs(chk.a-chk.b)<2,'measured again, the new sound is within 2 dB of the old one ('+(chk.a||0).toFixed(1)+' and '+(chk.b||0).toFixed(1)+' dB)');
 ok(chk.lv.some(v=>Math.abs(v-chk.want)<1e-6)&&chk.want>1.5,'the app’s engine is sent the same level ('+chk.want.toFixed(2)+')');
 // Undo takes it back; picking another sound clears it; a similar sound makes no offer
 await p.keyboard.press('Control+z'); await p.waitForTimeout(400);
 ok(await p.evaluate(()=>!(S.parts.lead.sg)),'Undo takes the match back');
 await p.keyboard.press('Control+Shift+z'); await p.waitForTimeout(400);
 await p.evaluate(()=>{ MATCH.last=null; $('#toast').hidden=true; soundSet('lead','bright'); });
 ok(await p.evaluate(()=>!(S.parts.lead.sg)),'picking another sound clears the old match at once');
 const o2=await waitOffer(); ok(o2&&o2.last&&Math.abs(o2.last.d)<3&&!o2.btn,'going back from the matched sound needs no offer: it was already as loud ('+(o2&&o2.last?o2.last.d:'?')+' dB)');
 // from an unmatched quiet sound to a loud one: offered the other way
 await p.evaluate(()=>{ MATCH.last=null; $('#toast').hidden=true; soundSet('lead','i:nylon'); }); await waitOffer(); await p.evaluate(()=>{ MATCH.last=null; $('#toast').hidden=true; $('#toast').textContent=''; soundSet('lead','bright'); });
 const o4=await waitOffer(); ok(o4&&o4.last&&o4.last.d<=-5&&o4.btn&&/louder than/.test(o4.txt),'from a quiet sound left unmatched to a loud one, it offers to bring the loud one down ('+(o4&&o4.last?o4.last.d:'?')+' dB)');
 await p.evaluate(()=>{ MATCH.last=null; $('#toast').hidden=true; soundSet('lead','soft'); }); const o3=await waitOffer();
 ok(o3&&o3.last&&Math.abs(o3.last.d)<3&&!o3.btn,'a sound that is about as loud makes no offer ('+(o3&&o3.last?o3.last.d:'?')+' dB)');
 console.log(JSON.stringify(errs)); await b.close(); if(fails.length||errs.length){ console.log('FAILED',fails); process.exit(1); } })();
