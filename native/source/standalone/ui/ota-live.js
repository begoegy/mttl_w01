/* FWR-only OTA. The device verifies format, checksum and flash readback. */
(function(){
 'use strict';
 const el=id=>document.getElementById(id);let ready=false,busy=false,result=null,detail='';
 const words={ko:['전원을 끄지 마세요. 지원: 독립 FWR, 순정 1.0.66, 로컬 패치 1.0.68. 복구 시 독립 웹·직접 HA 연결이 종료됩니다.','펌웨어 파일 (.fwr)','업데이트','업데이트 준비됨','업데이트 준비 안 됨','올바른 .fwr 파일을 선택해 주세요.','업로드 중… 전원을 끄지 마세요.','업데이트 완료. Wi-Fi로 다시 연결합니다.','결과를 확인할 수 없습니다. 재접속 후 확인하고 다시 시도하세요.','업데이트하고 재부팅할까요?\n1.0.66/1.0.68 복구 후에는 독립 웹·직접 HA 대신 기존 프로비저닝 또는 백엔드 연결이 필요합니다.','복구 완료. 기존 프로비저닝 또는 백엔드 연결을 확인하세요.'],en:['Keep power on. Supported: standalone FWR, stock 1.0.66 or local-patched 1.0.68. Restoring removes standalone web/direct HA.','Firmware file (.fwr)','Update','Ready to update','Not ready to update','Select a valid .fwr file.','Uploading… Keep power on.','Update completed. Reconnecting to Wi-Fi.','Result uncertain. Reconnect and check before retrying.','Update and restart?\nRestoring 1.0.66/1.0.68 removes standalone web/direct HA. Use original provisioning or backend connectivity afterwards.','Restore completed. Check original provisioning or backend connectivity.']};
 function text(){return words[document.documentElement.lang==='en'?'en':'ko'];}
 const pendingKey='mttl.ota.pending-build';
 function remember(id){try{if(id)localStorage.setItem(pendingKey,id);else localStorage.removeItem(pendingKey);}catch(_){}}
 function render(){const w=text();el('ota-hint').textContent=w[0];el('ota-file-label').textContent=w[1];el('firmware-update').textContent=w[2];el('firmware-status').textContent=w[result===null?(ready?3:4):result]+(detail?' '+detail:'');el('firmware-picker').disabled=busy;el('firmware-update').disabled=busy||!ready;el('firmware-selected').textContent=el('firmware-file').files[0]?.name||MttlI18n.text(document.documentElement.lang==='en'?'en':'ko','noSelectedFile');}
 const language=el('language').onclick;el('language').onclick=()=>{language();render();};
 el('firmware-picker').onclick=()=>el('firmware-file').click();el('firmware-file').onchange=()=>{result=null;detail='';render();};
 el('firmware-form').onsubmit=async event=>{
  event.preventDefault();if(busy||!ready)return;
  const file=el('firmware-file').files[0];
  if(!file||file.size<44||file.size>0x6a014||!file.name.toLowerCase().endsWith('.fwr')){result=5;render();return;}
  if(!confirm(text()[9]))return;
  const expected=file.name.match(/-build-([0-9a-f]{16})\.fwr$/i)?.[1]?.toLowerCase();remember(expected);
  busy=true;result=6;detail='';render();const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),180000);
  try{
   const response=await fetch('/api/firmware/upload',{method:'POST',headers:{'Content-Type':'application/x-mttl-fwr'},body:file,signal:controller.signal,credentials:'omit'});
   const raw=await response.text();let body;try{body=JSON.parse(raw);}catch(_){}
   if(!response.ok||!body?.written||!body?.activated){
    const message=raw.slice(0,2048).replace(/<[^>]*>/g,' ').replace(/[\x00-\x1f\x7f]/g,' ').replace(/\s+/g,' ').trim().slice(0,400);
    detail='HTTP '+response.status+(message?' · '+message:' · Empty response');throw Error('Server response');
   }
   result=body.restore_version?10:7;detail=body.restore_version||'';ready=false;if(body.restore_version)remember(null);
  }
  catch(error){result=8;ready=false;if(!detail)detail=error.name==='AbortError'?'Timeout (180 s)':'Network/connection error: '+String(error.message||error).slice(0,160);}
  finally{clearTimeout(timer);busy=false;render();}
 };
 fetch('/api/firmware/status',{cache:'no-store',credentials:'omit'}).then(response=>{if(!response.ok)throw Error('Status');return response.json();}).then(status=>{
  ready=status.ota_ready===true;const build=/^[0-9a-f]{16}$/.test(status.build_id||'')?status.build_id:null;
  if(el('current-build-id'))el('current-build-id').textContent=build||'—';
  let expected;try{expected=localStorage.getItem(pendingKey);}catch(_){}
  if(expected&&build){detail=(document.documentElement.lang==='en'?'Build verification: ':'빌드 확인: ')+(expected===build?'OK · ':'NOT MATCHED · ')+build;if(expected===build)remember(null);}
  render();
 }).catch(()=>render());render();
})();
