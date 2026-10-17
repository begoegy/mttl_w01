/* Device integration UI. No synthetic sensor values or simulated switching. */
(function(){
  'use strict';
  const el=id=>document.getElementById(id),key='mttl.preview.display.v1';
  let language='ko',preferences={},snapshot=null,deviceSnapshot=null,configuration=null,composing=false,wifiMessage='integrationPending',controlBusy=false;
  let events=null,eventConnected=false;
  let relaySnapshot=null;
  function applyRelay(){if(!relaySnapshot||!deviceSnapshot)return;deviceSnapshot.channels.forEach((c,i)=>{c.on=relaySnapshot.known&(1<<i)?!!(relaySnapshot.on&(1<<i)):null;});if(relaySnapshot.fault)deviceSnapshot.hardware_ready=false;}
  const pendingChannels=new Set();let controlTail=Promise.resolve();
  const optimistic=new Map();
  const channelNodes=[];
  // Bounded browser-session diagnostics, never configuration or credentials.
  const logRows=[];let lastObserved=null;
  function renderLog(){const box=el('log');box.textContent=logRows.length?logRows.map(row=>'['+row.at+'] '+(language==='en'?row.en:row.ko)).join('\n'):(language==='en'?'Browser-session events only; not stored in device.':'브라우저 세션 로그 · 기기에 저장되지 않습니다.');if(el('autoscroll').checked)box.scrollTop=box.scrollHeight;}
  function logEvent(ko,en){logRows.push({at:new Date().toLocaleTimeString(),ko,en});if(logRows.length>200)logRows.shift();renderLog();}
  function observe(){
    const current={available:!!snapshot,network:snapshot?.state,mqtt:deviceSnapshot?.mqtt_state,uptime:snapshot?.uptime_ms,on:deviceSnapshot?.channels?.map(c=>c.on)};
    if(lastObserved){
      if(current.available!==lastObserved.available)logEvent(current.available?'기기 API 연결 복구':'기기 API 응답 없음',current.available?'Device API recovered':'Device API unavailable');
      if(current.available&&lastObserved.available&&current.uptime<lastObserved.uptime)logEvent('기기 재부팅 감지','Device reboot observed');
      if(Number.isInteger(current.network)&&current.network!==lastObserved.network)logEvent('Wi-Fi 상태: '+t('netState'+current.network),'Wi-Fi state: '+MttlI18n.text('en','netState'+current.network));
      if(Number.isInteger(current.mqtt)&&current.mqtt!==lastObserved.mqtt)logEvent('MQTT 상태 코드: '+current.mqtt,'MQTT state code: '+current.mqtt);
      if(current.on&&lastObserved.on)current.on.forEach((on,i)=>{if(typeof on==='boolean'&&on!==lastObserved.on[i])logEvent('채널 '+(i+1)+' 확인 상태: '+(on?'ON':'OFF'),'Channel '+(i+1)+' confirmed state: '+(on?'ON':'OFF'));});
    }else if(current.available)logEvent('기기 상태 관찰 시작','Device state observation started');
    lastObserved=current;
  }
  function startEvents(){
    // Setup needs SDK scan allocations; do not reserve a live sensor stream.
    if(snapshot?.state!==5){if(events)events.close();events=null;eventConnected=false;relaySnapshot=null;return;}
    if(events||typeof EventSource==='undefined')return;
    events=new EventSource('/api/events');
    events.onopen=()=>{eventConnected=true;logEvent('실시간 상태 연결됨','Live state stream connected');};events.onerror=()=>{if(eventConnected)logEvent('실시간 연결 끊김 · 재연결 대기','Live stream interrupted; waiting for reconnect');eventConnected=false;relaySnapshot=null;};
    events.addEventListener('relay',event=>{try{const d=JSON.parse(event.data);if(![d.known,d.on,d.fault].every(Number.isInteger)||d.known<0||d.known>15||d.on<0||d.on>15||d.fault<0)throw Error('Relay');relaySnapshot=d;applyRelay();render();}catch(_){eventConnected=false;}});
    events.addEventListener('device',event=>{try{const d=JSON.parse(event.data);if(!Array.isArray(d.channels)||d.channels.length!==4)throw Error('Device');deviceSnapshot=d;applyRelay();render();}catch(_){eventConnected=false;}});
  }
  try{preferences=JSON.parse(localStorage.getItem(key)||'{}');if(preferences.language==='en')language='en';}catch(_){preferences={};}
  const t=(name,args={})=>MttlI18n.text(language,name,args);
  function save(){preferences.language=language;try{localStorage.setItem(key,JSON.stringify(preferences));}catch(_){}}
  function goodName(v){return typeof v==='string'&&v.trim()&&new TextEncoder().encode(v.trim()).length<=64&&!/[\x00-\x1f\x7f]/.test(v);}
  function name(i){const actual=i<0?deviceSnapshot?.name:deviceSnapshot?.channels?.[i]?.name;const value=actual||(i<0?preferences.deviceName:preferences.channelNames?.[i]);return goodName(value)?value:t(i<0?'deviceDefault':i===0?'channelOneDefault':'channelDefault',{number:i+1});}
  function reading(value,scale,unit){return Number.isInteger(value)?(value/scale).toFixed(scale===10?1:3)+' '+unit:'— '+unit;}
  function settingsBody(c,password=''){
    const port=Number(c.port);if(!Number.isInteger(port)||port<1||port>65535)throw Error('Port');
    const fields=[c.device,...c.channels,'','',c.broker,c.user,password,c.topic];if(fields.length!==11)throw Error('Channels');
    const bytes=[5,c.language,c.enabled?1:0,port&255,port>>>8];const limits=[64,64,64,64,64,32,64,253,64,64,64];
    fields.forEach((field,i)=>{const data=new TextEncoder().encode(field||'');if(data.length>limits[i])throw Error('Length');bytes.push(data.length,...data);});
    const p=c.protection||{enabled:7,temperature:90,current:16000,power:3000};
    if(!Number.isInteger(p.enabled)||p.enabled<0||p.enabled>7||!Number.isInteger(p.temperature)||p.temperature<50||p.temperature>90||!Number.isInteger(p.current)||p.current<1000||p.current>16000||!Number.isInteger(p.power)||p.power<100||p.power>3000)throw Error('Protection');
    bytes.push(p.enabled,p.temperature&255,p.temperature>>>8,p.current&255,p.current>>>8,p.power&255,p.power>>>8);
    const hex=c.schedule_hex||MttlSchedule.encode(MttlSchedule.defaults());MttlSchedule.decode(hex);bytes.push(...hex.match(/../g).map(x=>parseInt(x,16)));return Uint8Array.from(bytes);
  }
  async function storeConfiguration(c,password='',keep=true){if(![2,5].includes(snapshot?.state))throw Error('Not ready');const body=settingsBody(c,password);try{await request('/api/config',{method:'POST',headers:{'Content-Type':'application/octet-stream','X-MTTL-Keep-Password':keep?'1':'0'},body});for(let n=0;n<12;n++){await new Promise(resolve=>setTimeout(resolve,250));const saved=await request('/api/config');if(saved.persistent&&saved.device===c.device&&JSON.stringify(saved.channels)===JSON.stringify(c.channels)&&saved.language===c.language&&saved.broker===c.broker&&saved.port===c.port&&saved.user===c.user&&saved.enabled===c.enabled&&(!c.schedule_hex||saved.schedule_hex===c.schedule_hex)&&(!c.protection||['enabled','temperature','current','power'].every(k=>saved.protection?.[k]===c.protection[k]))){configuration=saved;deviceSnapshot=null;return true;}}throw Error('Flash save not confirmed');}finally{body.fill(0);}}
  function loadProtection(){const p=configuration?.protection||{enabled:7,temperature:90,current:16000,power:3000};['temperature','current','power'].forEach((k,i)=>{el('protect-'+k).value=String(k==='current'?p[k]/1000:p[k]);el('protect-'+k+'-enabled').checked=!!(p.enabled&(1<<i));});protectionValues();}
  function protectionValues(){['temperature','current','power'].forEach(k=>{el('protect-'+k+'-value').textContent=(k==='current'?Number(el('protect-'+k).value).toFixed(1):el('protect-'+k).value)+' '+({temperature:'°C',current:'A',power:'W'}[k]);});}
  function hexText(hex){if(typeof hex!=='string'||hex.length>64||!/^(?:[0-9a-f]{2})*$/.test(hex))return '';return new TextDecoder().decode(Uint8Array.from(hex.match(/../g)||[],x=>parseInt(x,16)));}
  async function request(path,options={}){
    const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),5000);
    try{const response=await fetch(path,{...options,signal:controller.signal,cache:'no-store',credentials:'omit'});if(!response.ok)throw Error('API');return await response.json();}finally{clearTimeout(timer);}
  }
  function edit(i=-1){el('device-name-input').value=name(-1);for(let n=0;n<4;n++)el('channel-name-'+(n+1)).value=name(n);el('name-editor').hidden=false;el(i<0?'device-name-input':'channel-name-'+(i+1)).focus();}
  function render(){
    scheduleEditor.ready();
    observe();
    el('protection-apply').disabled=!configuration||![2,5].includes(snapshot?.state)||protectionSaving;
    {const p=deviceSnapshot?.protection,names=language==='en'?['Temperature','Current','Power']:['온도','전류','전력'];const items=bits=>names.filter((_,i)=>bits&(1<<i)).join(', ');el('protection-state').textContent=p?((language==='en'?'Last trip: ':'최근 차단: ')+(p.reason?items(p.reason):(language==='en'?'None':'없음'))+' · '+(language==='en'?'Sensor errors: ':'센서 오류: ')+(p.error?items(p.error):(language==='en'?'None':'없음'))):t('apiUnavailable');}
    ['all-on','all-off'].forEach(id=>el(id).disabled=controlBusy||!deviceSnapshot?.hardware_ready||!snapshot?.hardware_ready||snapshot?.state!==5);
    document.querySelector('.note').textContent=deviceSnapshot?.hardware_ready?(language==='en'?'Experimental relay and sensor firmware. Switch states reflect commands, not measured contacts.':'릴레이·계측 시험용 펌웨어입니다. 스위치 상태는 명령 결과이며 실제 접점 피드백은 아닙니다.'):t('liveNote');
    el('device-name').textContent=name(-1);
    for(let i=0;i<4;i++){
      if(!channelNodes[i]){
      const card=document.createElement('div');card.className='channel';
      const header=document.createElement('div');header.className='channel-head';
      const title=document.createElement('h2');title.textContent=name(i);
      const pencil=document.createElement('button');pencil.className='edit-name';pencil.textContent='✎';pencil.setAttribute('aria-label',t('editChannel',{number:i+1}));pencil.onclick=()=>edit(i);header.append(pencil,title);card.append(header);
      const sensors=[];
      for(const [label,unit,field,scale] of [['power','W','power_mw',1000],['meter','kWh','meter_wh',1000],['current','A','current_ma',1000],['temperature','°C','temp_dc',10]]){
        const row=document.createElement('div');row.className='sensor';const text=document.createElement('span'),value=document.createElement('b');text.textContent=t(label);value.textContent=reading(deviceSnapshot?.channels?.[i]?.[field],scale,unit);row.append(text,value);card.append(row);
        sensors.push({text,value,label,unit,field,scale});
      }
      const button=document.createElement('button');button.className='switch';button.onclick=()=>{const on=optimistic.has(i)?optimistic.get(i):deviceSnapshot?.channels?.[i]?.on;return control(i,on===true?0:1);};card.append(button);el('channels').append(card);
      channelNodes[i]={title,pencil,button,sensors};
      }
      const node=channelNodes[i];node.title.textContent=name(i);node.pencil.setAttribute('aria-label',t('editChannel',{number:i+1}));
      for(const sensor of node.sensors){sensor.text.textContent=t(sensor.label);const value=reading(deviceSnapshot?.channels?.[i]?.[sensor.field],sensor.scale,sensor.unit);if(sensor.value.textContent!==value)sensor.value.textContent=value;}
      node.button.disabled=pendingChannels.has(i)||pendingChannels.has(4)||!deviceSnapshot?.hardware_ready||!snapshot?.hardware_ready||snapshot?.state!==5;
      const on=optimistic.has(i)?optimistic.get(i):deviceSnapshot?.channels?.[i]?.on,value=on===true?'ON':on===false?'OFF':(language==='en'?'Turn on':'켜기');if(node.button.textContent!==value)node.button.textContent=value;
      const style=on===true?'switch is-on':'switch';if(node.button.className!==style)node.button.className=style;
    }
    const states=Array.from({length:4},(_,i)=>optimistic.has(i)?optimistic.get(i):deviceSnapshot?.channels?.[i]?.on);
    const all=snapshot?.state===5&&states.every(on=>typeof on==='boolean')?(states.every(Boolean)?'ON':states.some(Boolean)?'PARTIAL':'OFF'):'—';
    const values=[reading(deviceSnapshot?.power_mw,1000,'W'),reading(deviceSnapshot?.meter_wh,1000,'kWh'),reading(deviceSnapshot?.voltage_dv,10,'V'),reading(deviceSnapshot?.current_ma,1000,'A'),all];document.querySelectorAll('.summary b').forEach((node,i)=>node.textContent=values[i]);
    const date=value=>Number.isSafeInteger(value)&&value>0?new Date(value).toLocaleString(language==='ko'?'ko-KR':'en-US'):'—';
    el('mac').textContent=/^(?:[0-9a-f]{2}:){5}[0-9a-f]{2}$/i.test(snapshot?.mac||'')?snapshot.mac:'—';el('clock').textContent=date(snapshot?.unix_ms);el('connected').textContent=date(snapshot?.connected_unix_ms);
    el('online').textContent=snapshot?t('netState'+snapshot.state):t('apiUnavailable');
    const ip=snapshot?.ip;el('ip').textContent=Number.isInteger(ip)&&ip>0?[ip>>>24,(ip>>>16)&255,(ip>>>8)&255,ip&255].join('.'):'—';
    el('connected-ssid').textContent=snapshot?hexText(snapshot.ssid_hex)||'—':'—';
    el('rssi').textContent=(snapshot&&Number.isInteger(snapshot.rssi)?snapshot.rssi:'—')+' dBm';
    el('uptime').textContent=snapshot?t('seconds',{count:Math.floor(snapshot.uptime_ms/1000)}):'—';
    el('wifi-status').textContent=t(wifiMessage);
    el('mqtt-status').textContent=t('netState'+([0,1,3,3,3,5,6][deviceSnapshot?.mqtt_state]??0));
  }
  function translate(){
    scheduleEditor.translate();
    document.documentElement.lang=language;el('language').textContent=language==='ko'?'English':'Korean';
    document.title='MTTL-W01';for(let i=1;i<=4;i++)el('channel-name-label-'+i).textContent=t('channelName',{number:i});
    for(const [attr,target] of [['data-i18n',null],['data-i18n-placeholder','placeholder'],['data-i18n-aria','aria-label']])document.querySelectorAll('['+attr+']').forEach(n=>target?n.setAttribute(target,t(n.getAttribute(attr))):n.textContent=t(n.getAttribute(attr)));
    document.querySelector('.badge').textContent=t('deviceIntegration');document.querySelector('.note').textContent=t('liveNote');
    el('scan').textContent=t('realScan');document.querySelector('#wifi-form button.primary').textContent=t('realConnect');
    el('current-firmware-version').textContent='2.0.12';el('firmware-status').textContent=t('integrationPending');el('mqtt-status').textContent=t('integrationPending');el('firmware-selected').textContent=t('noSelectedFile');
    document.querySelector('#mqtt-form button.primary').textContent=language==='en'?'Save to device':'기기에 저장';
    renderLog();render();
  }
  el('language').onclick=()=>{language=language==='ko'?'en':'ko';save();translate();};
  let initialTabPending=true;
  function selectTab(id){document.querySelectorAll('section').forEach(s=>s.classList.toggle('active',s.id===id));document.querySelectorAll('[role=tab]').forEach(b=>b.setAttribute('aria-selected',String(b.getAttribute('aria-controls')===id)));}
  document.querySelectorAll('[role=tab]').forEach(button=>button.onclick=()=>{initialTabPending=false;selectTab(button.getAttribute('aria-controls'));});
  el('edit-names').onclick=()=>edit();el('cancel-names').onclick=()=>{el('name-editor').hidden=true;};
  el('name-form').oncompositionstart=()=>{composing=true;};el('name-form').oncompositionend=()=>{composing=false;};
  el('name-form').onsubmit=async event=>{event.preventDefault();if(composing)return;const device=el('device-name-input').value,channels=Array.from({length:4},(_,i)=>el('channel-name-'+(i+1)).value);if(![device,...channels].every(goodName)){el('name-status').textContent=t('nameInvalid');return;}if(configuration){try{await storeConfiguration({...configuration,device:device.trim(),channels:channels.map(x=>x.trim()),language:language==='en'?1:0});el('name-status').textContent=language==='en'?'Saved in device flash':'기기 플래시에 저장했습니다.';}catch(_){el('name-status').textContent=language==='en'?'Device save failed':'기기에 저장하지 못했습니다.';return;}}else el('name-status').textContent=t('browserNames');preferences.deviceName=device.trim();preferences.channelNames=channels.map(x=>x.trim());save();render();};
  ['all-on','all-off','firmware-picker','firmware-verify','firmware-update'].forEach(id=>el(id).disabled=true);
  async function control(channel,on){
    if(pendingChannels.has(channel)||pendingChannels.has(4)||(channel===4&&pendingChannels.size))return;
    pendingChannels.add(channel);for(let i=0;i<4;i++)if(channel===4||i===channel)optimistic.set(i,!!on);controlBusy=true;render();
    const previous=controlTail;let release,released=false;controlTail=new Promise(resolve=>{release=()=>{if(!released){released=true;resolve();}};});
    await previous;
    try{
      await request('/api/control',{method:'POST',headers:{'Content-Type':'application/octet-stream'},body:Uint8Array.of(channel,on)});
      // Firmware accepts bounded queued commands; do not wait for the preceding
      // pulse or SSE confirmation before sending another channel's command.
      release();
      for(let n=0;n<(eventConnected?50:16);n++){
        const d=eventConnected?deviceSnapshot:await request('/api/device');if(!Array.isArray(d?.channels)||d.channels.length!==4)throw Error('Device');deviceSnapshot=d;
        if((channel===4?d.channels:[d.channels[channel]]).every(c=>c.on===!!on))return;
        await new Promise(resolve=>setTimeout(resolve,eventConnected?100:200));
      }
      throw Error('Command not confirmed');
    }catch(_){try{const actual=await request('/api/device');if(Array.isArray(actual.channels)&&actual.channels.length===4)deviceSnapshot=actual;}catch(_){}logEvent('제어 실패 · 마지막 확인 상태 표시','Control failed; showing last confirmed state');}finally{for(let i=0;i<4;i++)if(channel===4||i===channel)optimistic.delete(i);pendingChannels.delete(channel);controlBusy=pendingChannels.size>0;release();render();}
  }
  el('all-on').onclick=()=>control(4,1);el('all-off').onclick=()=>control(4,0);
  let protectionSaving=false;
  const scheduleEditor=MttlSchedule.mount({language:()=>language,available:()=>!!configuration&&[2,5].includes(snapshot?.state),save:hex=>storeConfiguration({...configuration,schedule_hex:hex})});
  ['temperature','current','power'].forEach(k=>el('protect-'+k).oninput=protectionValues);
  el('protection-form').onsubmit=async event=>{event.preventDefault();if(!configuration||protectionSaving)return;
    const p={enabled:0,temperature:Number(el('protect-temperature').value),current:Math.round(Number(el('protect-current').value)*1000),power:Number(el('protect-power').value)};
    ['temperature','current','power'].forEach((k,i)=>{if(el('protect-'+k+'-enabled').checked)p.enabled|=1<<i;});protectionSaving=true;render();
    try{await storeConfiguration({...configuration,protection:p});el('protection-save-status').textContent=language==='en'?'Saved in device flash':'기기 플래시에 저장했습니다.';}catch(_){el('protection-save-status').textContent=language==='en'?'Save failed; settings not confirmed':'저장 실패 · 적용 여부를 확인하지 못했습니다.';}finally{protectionSaving=false;render();}
  };
  document.querySelectorAll('#mqtt-form input,#mqtt-form button').forEach(node=>node.disabled=true);
  el('firmware-form').onsubmit=event=>event.preventDefault();
  el('mqtt-form').onsubmit=async event=>{event.preventDefault();if(!configuration)return;const password=el('mqtt-password').value;el('mqtt-password').value='';try{await storeConfiguration({...configuration,broker:el('broker').value,port:Number(el('port').value),user:el('user').value,enabled:!!el('enabled').checked},password,!password);el('mqtt-status').textContent=language==='en'?'Saved in flash; plaintext IPv4 broker only':'플래시에 저장됨 · IPv4 비암호화 브로커 전용';}catch(_){el('mqtt-status').textContent=t('apiUnavailable');}};
  el('scan').onclick=async()=>{
    el('scan').disabled=true;wifiMessage='scanProgress';render();
    try{await request('/api/scan',{method:'POST'});let result;
      for(let n=0;n<12;n++){await new Promise(resolve=>setTimeout(resolve,500));result=await request('/api/scan');if(!result.pending)break;}
      if(result.pending){wifiMessage='scanTimeout';return;}
      if(result.failed){wifiMessage='scanFailed';return;}
      if(!Array.isArray(result.networks)||result.networks.length>16)throw Error('Scan');
      el('ssid').replaceChildren();for(const ap of result.networks){const ssid=hexText(ap.ssid_hex);if(!ssid)continue;const option=document.createElement('option');option.value=ssid;option.textContent=ssid+' ('+ap.rssi+' dBm)';el('ssid').append(option);}wifiMessage='scanDone';
    }catch(_){wifiMessage='apiUnavailable';}finally{el('scan').disabled=false;render();}
  };
  el('wifi-form').onsubmit=async event=>{event.preventDefault();const ssid=el('ssid').value,password=el('wifi-password').value;el('wifi-password').value='';if(!ssid||new TextEncoder().encode(ssid).length>32){wifiMessage='ssidInvalid';render();return;}
    try{await request('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({ssid,password}).toString()});wifiMessage='connectAccepted';}catch(_){wifiMessage='apiUnavailable';}render();
  };
  async function poll(){try{const result=await request('/api/status');if(!Number.isInteger(result.state)||result.state<0||result.state>6||!Number.isInteger(result.uptime_ms))throw Error('State');snapshot=result;}catch(_){snapshot=null;if(!controlBusy)deviceSnapshot=null;}if(snapshot){startEvents();if(!controlBusy&&!eventConnected){try{const d=await request('/api/device');if(!Array.isArray(d.channels)||d.channels.length!==4)throw Error('Device');if(!controlBusy&&!eventConnected)deviceSnapshot=d;}catch(_){if(!controlBusy)deviceSnapshot=null;}}if(!configuration){try{const c=await request('/api/config');if(!Array.isArray(c.channels)||c.channels.length!==4||typeof c.broker!=='string')throw Error('Config');configuration=c;el('broker').value=c.broker;el('port').value=c.port;el('user').value=c.user;el('enabled').checked=c.enabled;loadProtection();scheduleEditor.load(c.schedule_hex||MttlSchedule.encode(MttlSchedule.defaults()));}catch(_){configuration=null;}}}document.querySelectorAll('#mqtt-form input,#mqtt-form button').forEach(node=>node.disabled=!configuration||![2,5].includes(snapshot?.state));render();setTimeout(poll,eventConnected?20000:1000);}
  selectTab('wifi');translate();
  const initialPoll=poll;
  poll=async function(){await initialPoll();if(initialTabPending&&snapshot){selectTab(typeof location!=='undefined'&&location.hash==='#firmware'?'firmware':snapshot.state===5?'device':'wifi');initialTabPending=false;}};
  poll();
})();
