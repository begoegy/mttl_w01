(function(){
  'use strict';
  const byId=id=>document.getElementById(id),boot=Date.now(),events=[],states=[false,false,false,false];
  const PREF_KEY='mttl.preview.display.v1';
  function validName(value){return typeof value==='string'&&value.trim().length>0&&new TextEncoder().encode(value.trim()).length<=64&&!/[\x00-\x1f\x7f]/.test(value);}
  let preferences={language:'ko',deviceName:null,channelNames:[null,null,null,null]};
  try{
    const saved=JSON.parse(localStorage.getItem(PREF_KEY)||'null');
    if(saved&&typeof saved==='object'){
      if(saved.language==='ko'||saved.language==='en')preferences.language=saved.language;
      if(validName(saved.deviceName))preferences.deviceName=saved.deviceName.trim();
      if(Array.isArray(saved.channelNames)&&saved.channelNames.length===4)preferences.channelNames=saved.channelNames.map(name=>validName(name)?name.trim():null);
    }
  }catch(error){/* Storage may be unavailable in a local-file browser. */}
  const t=(key,args={})=>MttlI18n.text(preferences.language,key,args);
  const scheduleEditor=MttlSchedule.mount({language:()=>preferences.language,available:()=>true,save:async hex=>localStorage.setItem('mttl.preview.schedule',hex)});
  try{scheduleEditor.load(localStorage.getItem('mttl.preview.schedule')||MttlSchedule.encode(MttlSchedule.defaults()));}catch(_){scheduleEditor.load(MttlSchedule.encode(MttlSchedule.defaults()));}
  function persist(){try{localStorage.setItem(PREF_KEY,JSON.stringify(preferences));return true;}catch(error){return false;}}
  const statuses={'wifi-status':null,'mqtt-status':{key:'disabled'},'firmware-status':{key:'chooseFile'},'name-status':null};
  function status(id,key,error=false){statuses[id]=key?{key,error}:null;renderStatuses();}
  function renderStatuses(){for(const [id,value] of Object.entries(statuses)){byId(id).className='status'+(value?.error?' error':'');byId(id).textContent=value?t(value.key):'';}}
  function renderLogs(){
    byId('log').textContent=events.map(event=>event.at.toLocaleTimeString(preferences.language==='en'?'en-US':'ko-KR')+'  '+t(event.key,event.args)).join('\n');
    if(byId('autoscroll').checked)byId('log').scrollTop=byId('log').scrollHeight;
  }
  function log(key,args={}){events.push({at:new Date(),key,args});if(events.length>64)events.shift();renderLogs();}
  function tab(id){document.querySelectorAll('section').forEach(s=>s.classList.toggle('active',s.id===id));document.querySelectorAll('[role=tab]').forEach(b=>b.setAttribute('aria-selected',String(b.getAttribute('aria-controls')===id)));}
  document.querySelectorAll('[role=tab]').forEach(b=>b.addEventListener('click',()=>tab(b.getAttribute('aria-controls'))));
  const deviceName=()=>preferences.deviceName||t('deviceDefault');
  const channelName=i=>preferences.channelNames[i]||t(i===0?'channelOneDefault':'channelDefault',{number:i+1});
  let connectedAt=new Date(),connectedSsid='SIMULATED_AP',signal=-43;
  function time(){const locale=preferences.language==='en'?'en-US':'ko-KR';byId('clock').textContent=new Date().toLocaleString(locale);byId('connected').textContent=connectedAt.toLocaleString(locale);byId('uptime').textContent=t('seconds',{count:Math.floor((Date.now()-boot)/1000)});}
  function renderNetwork(){byId('connected-ssid').textContent=connectedSsid||'—';byId('rssi').textContent=(signal===null?'—':signal)+' dBm';byId('online').textContent=t('online');}
  function render(){
    byId('device-name').textContent=deviceName();
    const container=byId('channels');container.replaceChildren();
    states.forEach((on,i)=>{
      const card=document.createElement('div');card.className='channel'+(on?' on':'');
      const header=document.createElement('div');header.className='channel-head';
      const titleGroup=document.createElement('div');titleGroup.className='channel-title';
      const title=document.createElement('h2');title.textContent=channelName(i);titleGroup.append(title);
      const edit=document.createElement('button');edit.className='edit-name';edit.textContent='✎';edit.setAttribute('aria-label',t('editChannel',{number:i+1}));edit.setAttribute('title',t('editChannel',{number:i+1}));edit.addEventListener('click',()=>editNames(i));header.append(edit,titleGroup);card.append(header);
      [['power',on?'10.0 W':'0.0 W'],['meter',(0.1+i*0.05).toFixed(3)+' kWh'],['current',on?'0.100 A':'0.000 A'],['temperature',(24+i).toFixed(1)+' °C']].forEach(([key,value])=>{const row=document.createElement('div');row.className='sensor';const label=document.createElement('span'),data=document.createElement('b');label.textContent=t(key);data.textContent=value;row.append(label,data);card.append(row);});
      const button=document.createElement('button');button.className='switch';button.textContent=t(on?'turnOff':'turnOn');button.addEventListener('click',()=>{states[i]=!states[i];log('log_relay',{number:i+1,state:states[i]?'ON':'OFF'});render();});card.append(button);container.append(card);
    });
    const count=states.filter(Boolean).length;byId('total').textContent=(count*10).toFixed(1)+' W';byId('current').textContent=(count*0.1).toFixed(3)+' A';byId('all').textContent=count===4?'ON':count?t('partial'):'OFF';
  }
  function editNames(channel=null){
    byId('device-name-input').value=deviceName();for(let i=0;i<4;i++)byId('channel-name-'+(i+1)).value=channelName(i);
    byId('name-editor').hidden=false;status('name-status',null);
    byId(channel===null?'device-name-input':'channel-name-'+(channel+1)).focus();
  }
  byId('edit-names').addEventListener('click',()=>editNames());
  byId('cancel-names').addEventListener('click',()=>{byId('name-editor').hidden=true;status('name-status',null);});
  let composingNames=false;
  byId('name-form').addEventListener('compositionstart',()=>{composingNames=true;});
  byId('name-form').addEventListener('compositionend',()=>{composingNames=false;});
  byId('name-form').addEventListener('submit',event=>{
    event.preventDefault();if(composingNames)return;
    const device=byId('device-name-input').value.trim(),channels=Array.from({length:4},(_,i)=>byId('channel-name-'+(i+1)).value.trim());
    if(![device,...channels].every(validName)){status('name-status','nameInvalid',true);return;}
    preferences.deviceName=device;preferences.channelNames=channels;const stored=persist();render();status('name-status',stored?'namesSaved':'namesSession');log('log_names');
  });
  byId('all-on').addEventListener('click',()=>{states.fill(true);log('log_allOn');render();});
  byId('all-off').addEventListener('click',()=>{states.fill(false);log('log_allOff');render();});
  byId('hold').addEventListener('click',()=>{log('log_provision');status('wifi-status','apReady');tab('wifi');});
  byId('scan').addEventListener('click',()=>{byId('ssid').replaceChildren();['SIMULATED_AP','SIMULATED_BEDROOM'].forEach(name=>{const option=document.createElement('option');option.value=option.textContent=name;byId('ssid').append(option);});log('log_scan');});
  byId('wifi-form').addEventListener('submit',event=>{
    event.preventDefault();const ssid=byId('ssid').value;
    if(!ssid||new TextEncoder().encode(ssid).length>32){status('wifi-status','ssidInvalid',true);return;}
    connectedSsid=ssid;signal=-43;connectedAt=new Date();byId('wifi-password').value='';renderNetwork();time();status('wifi-status','wifiConnected');log('log_wifi');
  });
  byId('mqtt-form').addEventListener('submit',event=>{event.preventDefault();byId('mqtt-password').value='';status('mqtt-status',byId('enabled').checked?'enabled':'disabled');log(byId('enabled').checked?'log_mqttOn':'log_mqttOff');});
  function protectionValues(){for(const key of ['temperature','current','power']){const input=byId('protect-'+key);byId('protect-'+key+'-value').textContent=(key==='current'?Number(input.value).toFixed(1):input.value)+' '+({temperature:'°C',current:'A',power:'W'}[key]);}}
  for(const key of ['temperature','current','power'])byId('protect-'+key).addEventListener('input',protectionValues);
  let firmwareGeneration=0,verifiedFirmware=null,firmwareBusy=false,firmwareMetadata=null;
  function renderFirmware(){if(!firmwareMetadata)return;byId('firmware-size').textContent=firmwareMetadata.size.toLocaleString(preferences.language==='en'?'en-US':'ko-KR')+' bytes';byId('firmware-version').textContent=firmwareMetadata.versions.join(', ')||t('unknown');byId('firmware-checksum').textContent='0x'+firmwareMetadata.checksum.toString(16).padStart(8,'0')+' · '+t('match');}
  function resetFirmware(){if(firmwareBusy)return;firmwareGeneration++;verifiedFirmware=null;firmwareMetadata=null;byId('firmware-update').disabled=true;byId('firmware-info').hidden=true;byId('firmware-progress').hidden=true;byId('firmware-progress').value=0;status('firmware-status','chooseFile');}
  function renderSelectedFile(){byId('firmware-selected').textContent=byId('firmware-file').files?.[0]?.name||t('noSelectedFile');}
  byId('firmware-picker').addEventListener('click',()=>{if(!firmwareBusy)byId('firmware-file').click();});
  byId('firmware-file').addEventListener('change',()=>{resetFirmware();renderSelectedFile();});byId('firmware-expected').addEventListener('input',resetFirmware);
  function failure(code){const error=Error(t(code));error.code=code;return error;}
  byId('firmware-verify').addEventListener('click',async()=>{
    if(firmwareBusy)return;resetFirmware();const generation=firmwareGeneration,file=byId('firmware-file').files[0],expected=byId('firmware-expected').value.trim();byId('firmware-verify').disabled=true;
    try{
      if(!file)throw failure('noFile');if(file.size<44||file.size>MttlFirmware.FILE_CAP)throw failure('fwSize');if(!globalThis.crypto||!crypto.subtle)throw failure('fwCrypto');
      status('firmware-status','inspecting');const bytes=new Uint8Array(await file.arrayBuffer()),metadata=MttlFirmware.validate(bytes),digest=await MttlFirmware.sha256(bytes,crypto);MttlFirmware.expectedHash(digest,expected);
      if(generation!==firmwareGeneration)return;verifiedFirmware={generation,metadata};firmwareMetadata=metadata;
      byId('firmware-name').textContent=file.name;byId('firmware-format').textContent=metadata.format;byId('firmware-hash').textContent=digest;renderFirmware();byId('firmware-info').hidden=false;byId('firmware-update').disabled=false;status('firmware-status','inspectionPassed');log('log_fwPassed');
    }catch(error){if(generation===firmwareGeneration){status('firmware-status',error.code&&MttlI18n.messages.ko[error.code]?error.code:'fwRead',true);log('log_fwFailed');}}
    finally{byId('firmware-verify').disabled=false;}
  });
  byId('firmware-form').addEventListener('submit',async event=>{
    event.preventDefault();if(firmwareBusy||!verifiedFirmware||verifiedFirmware.generation!==firmwareGeneration)return;firmwareBusy=true;['firmware-file','firmware-expected','firmware-verify','firmware-update'].forEach(id=>byId(id).disabled=true);byId('firmware-progress').hidden=false;
    try{for(const [percent,key] of [[25,'uploading'],[70,'rechecking'],[90,'rebooting'],[100,'updateComplete']]){byId('firmware-progress').value=percent;status('firmware-status',key);await new Promise(resolve=>setTimeout(resolve,450));}log('log_fwComplete');verifiedFirmware=null;}
    finally{firmwareBusy=false;['firmware-file','firmware-expected','firmware-verify'].forEach(id=>byId(id).disabled=false);}
  });
  function applyLanguage(){
    scheduleEditor.translate();
    document.documentElement.lang=preferences.language;document.title=t('title');byId('language').textContent=preferences.language==='ko'?'English':'Korean';
    byId('current-firmware-version').textContent='2.0.12';
    for(const [attribute,target] of [['data-i18n',null],['data-i18n-placeholder','placeholder'],['data-i18n-aria','aria-label']])document.querySelectorAll('['+attribute+']').forEach(element=>{const text=t(element.getAttribute(attribute));if(target)element.setAttribute(target,text);else element.textContent=text;});
    for(let i=1;i<=4;i++)byId('channel-name-label-'+i).textContent=t('channelName',{number:i});
    render();renderNetwork();renderStatuses();renderFirmware();renderSelectedFile();renderLogs();time();
  }
  byId('language').addEventListener('click',()=>{preferences.language=preferences.language==='ko'?'en':'ko';persist();applyLanguage();});
  log('log_started');applyLanguage();setInterval(time,1000);
})();
