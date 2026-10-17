(function(root){
  'use strict';
  const ko={
    protection:'보호 설정',protectTemperature:'고온 차단 · 채널 중 최고 온도',protectCurrent:'과전류 차단 · 보정 후 전체 전류',protectPower:'고전력 차단 · 전체 전력',applyProtection:'적용',
    protectionHint:'온도는 첫 초과, 전체 전류·전력은 2회 연속 초과 시 전체 OFF합니다. 측정 주기는 약 10초입니다.',protectionNote:'자동 재켜기·잠금은 없습니다. 센서 읽기 실패는 오류만 표시합니다. 하드웨어 안전 보호 장치를 대체하지 않습니다.',
    title:'MTTL-W01 독립 펌웨어 화면 초안',simulation:'PC 모의 화면',language:'언어',
    note:'버튼과 설정 흐름을 확인하는 화면 초안입니다. 아래 값은 가상 데이터이며 실제 멀티탭·Wi-Fi·MQTT에는 연결하지 않습니다.',
    menu:'메뉴',device:'멀티탭',wifi:'Wi-Fi 설정',mqtt:'HA · MQTT',firmware:'펌웨어',logs:'로그',currentVersion:'현재 버전',
    deviceDefault:'거실 멀티탭',channelOneDefault:'거실 선풍기',channelDefault:'채널 {number}',
    rename:'이름 변경',editChannel:'채널 {number} 이름 변경',nameSettings:'기기 · 채널 이름',deviceName:'기기 이름',
    channelName:'채널 {number} 이름',nameHint:'이름은 저장 버튼을 눌렀을 때 반영됩니다. 각 이름은 UTF-8 64바이트 이내입니다.',
    saveNames:'이름 저장',cancel:'취소',nameInvalid:'기기와 채널 이름을 모두 입력해 주세요. 각 이름은 UTF-8 64바이트 이내이며 제어 문자는 사용할 수 없습니다.',
    namesSaved:'이름을 저장했습니다.',namesSession:'이름을 변경했습니다. 이 환경에서는 현재 화면에서만 유지됩니다.',
    connection:'연결',online:'Online · 모의',currentTime:'현재 시각',recentConnection:'최근 연결',uptime:'가동',seconds:'{count}초',
    totalPower:'전체 전력',meter:'누적 전력량',voltage:'전압',totalCurrent:'전체 전류',all:'전체',partial:'일부 켜짐',
    power:'전력',current:'전류',temperature:'온도',turnOff:'끄기',turnOn:'켜기',allOn:'전체 켜기',allOff:'전체 끄기',
    hold:'메인 버튼 10초 길게 누르기 모의',holdHint:'Wi-Fi 설정 모드 흐름 확인',
    wifiHint:'설정 AP 주소: 192.168.4.1 · Wi-Fi 목록을 검색하고 연결할 네트워크를 선택합니다.',
    scan:'Wi-Fi 검색 모의',network:'Wi-Fi SSID',scanFirst:'검색 후 선택',password:'암호',connect:'연결 모의',chooseFirmwareFile:'파일 선택',noSelectedFile:'선택된 파일 없음',
    apReady:'설정 AP 준비 · 192.168.4.1',ssidInvalid:'SSID를 선택해 주세요. SSID는 UTF-8 32바이트 이내여야 합니다.',
    wifiConnected:'연결·DHCP 완료 모의 · 192.0.2.10',mqttTitle:'Home Assistant · MQTT',broker:'Broker',port:'Port',
    username:'사용자',mqttEnable:'MQTT 연동 활성화',saveMock:'저장 모의',enabled:'Enabled · 연결 모의',disabled:'Disabled',
    firmwareHint:'현재: 독립 펌웨어 화면 초안 · 선택한 파일의 형식과 체크섬을 PC에서 검사합니다.',
    firmwareFile:'펌웨어 파일 (.fwr / Image2 .bin)',expectedHash:'예상 SHA-256 (선택)',hashPlaceholder:'배포자가 제공한 64자리 SHA-256',
    verifyFile:'파일 검사',updateMock:'업데이트 모의 실행',file:'파일',size:'크기',format:'형식',versionCandidates:'본문의 버전 후보',
    checksum:'체크섬',match:'일치',unknown:'확인 불가',progressLabel:'모의 업데이트 진행률',chooseFile:'파일을 선택하고 검사해 주세요.',
    firmwareStep1:'파일 형식·크기·진입점·체크섬 검사',firmwareStep2:'업로드 및 재검사 진행 모의',firmwareStep3:'재부팅·재연결 진행 모의',
    firmwareLimit:'형식 검사 통과는 실제 부팅 보증이 아닙니다. 이 화면은 파일을 서버나 기기에 전송하지 않으며, 실제 기록·재부팅은 연결되지 않았습니다.',
    noFile:'펌웨어 파일을 선택해 주세요.',fwSize:'파일 크기가 검사 범위를 벗어납니다.',fwCrypto:'이 브라우저 환경에서는 SHA-256 검사를 사용할 수 없습니다.',
    inspecting:'파일 검사 중…',inspectionPassed:'파일 형식·체크섬 검사 통과. 업데이트 진행을 모의 실행할 수 있습니다.',
    fwImage:'Image2 헤더·진입점·파일 길이·체크섬 검사에 실패했습니다.',fwHashSyntax:'예상 SHA-256은 64자리 16진수여야 합니다.',
    fwHashMismatch:'예상 SHA-256과 파일이 일치하지 않습니다.',fwRead:'파일을 읽거나 검사할 수 없습니다.',
    uploading:'업로드 진행 모의…',rechecking:'업로드 후 재검사 모의…',rebooting:'재부팅·재연결 모의…',
    updateComplete:'모의 업데이트 완료. 실제 기기와 펌웨어 버전은 변경되지 않았습니다.',
    eventLog:'이벤트 로그',autoscroll:'자동 스크롤',
    log_started:'화면 모의 실행 시작',log_relay:'채널 {number}: {state}',log_allOn:'전체 켜기',log_allOff:'전체 끄기',
    log_provision:'설정 모드 요청',log_scan:'Wi-Fi 검색 완료',log_wifi:'Wi-Fi 연결 모의 완료',log_mqttOn:'MQTT 활성화',log_mqttOff:'MQTT 비활성화',
    log_fwPassed:'펌웨어 파일 검사 통과',log_fwFailed:'펌웨어 파일 검사 실패',log_fwComplete:'펌웨어 업데이트 모의 완료',log_names:'기기·채널 이름 변경'
  };
  const en={
    protection:'Protection',protectTemperature:'Temperature · highest channel',protectCurrent:'Overcurrent · corrected total current',protectPower:'Power · total power',applyProtection:'Apply',
    protectionHint:'All OFF on the first high temperature reading, or two consecutive high total current/power rounds. Each channel is sampled about every 10 seconds.',protectionNote:'No automatic ON or lock. Sensor failures show errors only. This does not replace hardware safety protection.',
    title:'MTTL-W01 standalone firmware preview',simulation:'PC simulation',language:'Language',
    note:'A preview of controls and setup. Values are simulated; this page does not connect to a power strip, Wi-Fi or MQTT.',
    menu:'Menu',device:'Power strip',wifi:'Wi-Fi setup',mqtt:'HA · MQTT',firmware:'Firmware',logs:'Logs',currentVersion:'Current version',
    deviceDefault:'Living room power strip',channelOneDefault:'Living room fan',channelDefault:'Channel {number}',
    rename:'Edit names',editChannel:'Edit channel {number} name',nameSettings:'Device & channel names',deviceName:'Device name',
    channelName:'Channel {number} name',nameHint:'Names are applied when you press Save. Each name may contain up to 64 UTF-8 bytes.',
    saveNames:'Save names',cancel:'Cancel',nameInvalid:'Enter a name for the device and every channel. Each name must fit within 64 UTF-8 bytes and contain no control characters.',
    namesSaved:'Names saved.',namesSession:'Names updated. This environment keeps them only for the current page session.',
    connection:'Connection',online:'Online · simulated',currentTime:'Current time',recentConnection:'Last connected',uptime:'Uptime',seconds:'{count}s',
    totalPower:'Total Power',meter:'Meter',voltage:'Voltage',totalCurrent:'Total Current',all:'ALL',partial:'PARTIAL',
    power:'Power',current:'Current',temperature:'Temp',turnOff:'Turn OFF',turnOn:'Turn ON',allOn:'All ON',allOff:'All OFF',
    hold:'Simulate holding the main button for 10 seconds',holdHint:'Open Wi-Fi setup mode',
    wifiHint:'Setup AP address: 192.168.4.1 · Scan for Wi-Fi networks, then select one to connect.',
    scan:'Simulate Wi-Fi scan',network:'Wi-Fi SSID',scanFirst:'Scan to select',password:'Password',connect:'Simulate connection',chooseFirmwareFile:'Choose file',noSelectedFile:'No file selected',
    apReady:'Setup AP ready · 192.168.4.1',ssidInvalid:'Select an SSID. It must fit within 32 UTF-8 bytes.',
    wifiConnected:'Connection and DHCP simulated · 192.0.2.10',mqttTitle:'Home Assistant · MQTT',broker:'Broker',port:'Port',
    username:'Username',mqttEnable:'Enable MQTT integration',saveMock:'Simulate saving',enabled:'Enabled · simulated connection',disabled:'Disabled',
    firmwareHint:'Current: standalone firmware preview · Inspect the selected file format and checksum on this PC.',
    firmwareFile:'Firmware file (.fwr / Image2 .bin)',expectedHash:'Expected SHA-256 (optional)',hashPlaceholder:'64-digit SHA-256 provided by the publisher',
    verifyFile:'Inspect file',updateMock:'Simulate update',file:'File',size:'Size',format:'Format',versionCandidates:'Version candidates in the image',
    checksum:'Checksum',match:'matches',unknown:'Unknown',progressLabel:'Simulated update progress',chooseFile:'Choose a file, then inspect it.',
    firmwareStep1:'Inspect format, size, entry point and checksum',firmwareStep2:'Simulate upload and verification',firmwareStep3:'Simulate restart and reconnection',
    firmwareLimit:'Passing inspection does not guarantee boot compatibility. This page does not transmit the file to a server or device; flash writing and restart are not connected.',
    noFile:'Choose a firmware file.',fwSize:'The file size is outside the inspection budget.',fwCrypto:'SHA-256 inspection is unavailable in this browser context.',
    inspecting:'Inspecting file…',inspectionPassed:'Format and checksum passed. You can simulate the update process.',
    fwImage:'Image2 header, entry point, file length or checksum validation failed.',fwHashSyntax:'Expected SHA-256 must contain 64 hexadecimal digits.',
    fwHashMismatch:'The file does not match the expected SHA-256.',fwRead:'The file could not be read or inspected.',
    uploading:'Simulating upload…',rechecking:'Simulating verification after upload…',rebooting:'Simulating restart and reconnection…',
    updateComplete:'Simulation complete. The real device and firmware version have not changed.',
    eventLog:'Event log',autoscroll:'Auto-scroll',
    log_started:'Preview simulation started',log_relay:'Channel {number}: {state}',log_allOn:'All ON',log_allOff:'All OFF',
    log_provision:'Setup mode requested',log_scan:'Wi-Fi scan completed',log_wifi:'Wi-Fi connection simulation completed',log_mqttOn:'MQTT enabled',log_mqttOff:'MQTT disabled',
    log_fwPassed:'Firmware inspection passed',log_fwFailed:'Firmware inspection failed',log_fwComplete:'Firmware update simulation completed',log_names:'Device and channel names changed'
  };
  Object.assign(ko,{deviceIntegration:'기기 연결 초안',liveNote:'Wi-Fi API 연결용 초안입니다. 계측·릴레이·MQTT·OTA는 아직 연결되지 않았습니다. Wi-Fi 설정은 RAM에, 이름은 브라우저에만 저장됩니다.',integrationPending:'미연결',apiUnavailable:'응답 없음',realScan:'Wi-Fi 검색',realConnect:'연결',scanProgress:'검색 중…',scanDone:'검색 완료',connectAccepted:'접속 요청을 받았습니다. AP가 종료되면 기기의 새 IP로 접속하세요.',browserNames:'이 브라우저에 이름을 저장했습니다.',netState0:'정지',netState1:'종료 중',netState2:'설정 AP',netState3:'접속 중',netState4:'IP 할당 중',netState5:'연결됨',netState6:'오류'});
  Object.assign(en,{deviceIntegration:'Device integration draft',liveNote:'Wi-Fi API integration draft. Meter, relay, MQTT and OTA are not connected. Wi-Fi settings are in RAM; names are stored only in this browser.',integrationPending:'Not connected',apiUnavailable:'No response',realScan:'Scan Wi-Fi',realConnect:'Connect',scanProgress:'Scanning…',scanDone:'Scan completed',connectAccepted:'Connection requested. After the AP closes, open the device at its new IP.',browserNames:'Names saved in this browser.',netState0:'Stopped',netState1:'Stopping',netState2:'Setup AP',netState3:'Connecting',netState4:'Obtaining IP',netState5:'Connected',netState6:'Fault'});
  ko.liveNote='실험용 기기 펌웨어입니다. Wi-Fi·이름·MQTT 설정을 기기 플래시에 저장합니다. 웹 OTA는 /ota에서 시험할 수 있으며, 릴레이·계측은 아직 비활성화되어 있습니다.';
  en.liveNote='Experimental device firmware. Wi-Fi, names and MQTT settings are saved in device flash. Web OTA is available at /ota; relay and meter control remain disabled.';
  Object.assign(ko,{scanFailed:'Wi-Fi 검색 시작 실패 · 설정 화면만 남기고 다른 탭을 닫은 뒤 다시 시도하세요.',scanTimeout:'Wi-Fi 검색 대기 시간 초과 · 잠시 후 다시 시도하세요.'});
  Object.assign(en,{scanFailed:'Wi-Fi scan could not start. Close other device tabs and retry.',scanTimeout:'Wi-Fi scan timed out. Please retry shortly.'});
  ko.scheduler='스케줄러';en.scheduler='Scheduler';
  const api={messages:{ko,en},text:(language,key,args={})=>{
    const message=(language==='en'?en:ko)[key];
    if(message===undefined)throw Error('Missing translation: '+key);
    return message.replace(/\{(\w+)\}/g,(_,name)=>String(args[name]??''));
  }};
  if(typeof module==='object'&&module.exports)module.exports=api;else root.MttlI18n=api;
})(globalThis);
