/* Shared preview/device editor. Fixed 50-byte codec, eight bounded rules. */
(function(root){
 'use strict';
 const empty=()=>({minute:0,mode:0,day:0,channel:0,on:0});
 const defaults=()=>({timezone:540,rules:Array.from({length:8},empty)});
 function encode(c){const b=new Uint8Array(50),v=new DataView(b.buffer);if(!Number.isInteger(c.timezone)||c.timezone< -720||c.timezone>840||c.timezone%15||c.rules.length!==8)throw Error('Schedule');v.setInt16(0,c.timezone,true);
  c.rules.forEach((r,i)=>{const mode=r.mode&127;if(![r.minute,r.mode,r.day,r.channel,r.on].every(Number.isInteger)||r.mode<0||r.mode>255||mode>4||r.minute<0||r.minute>=1440||r.channel<0||r.channel>4||r.on<0||r.on>1||r.day<0||r.day>127||(!mode&&(r.mode||r.minute||r.day||r.channel||r.on))||(mode===2&&!r.day)||(mode===3&&(r.day<1||r.day>31))||([1,4].includes(mode)&&r.day))throw Error('Schedule');const p=2+i*6;v.setUint16(p,r.minute,true);b.set([r.mode,r.day,r.channel,r.on],p+2);});return Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');
 }
 function decode(hex){if(typeof hex!=='string'||!/^[0-9a-f]{100}$/i.test(hex))throw Error('Schedule');const b=Uint8Array.from(hex.match(/../g),x=>parseInt(x,16)),v=new DataView(b.buffer);const c={timezone:v.getInt16(0,true),rules:Array.from({length:8},(_,i)=>{const p=2+i*6;return {minute:v.getUint16(p,true),mode:b[p+2],day:b[p+3],channel:b[p+4],on:b[p+5]};})};encode(c);return c;}
 function mount({language,save,available}){
  const box=document.getElementById('schedule-editor'),form=document.getElementById('schedule-form'),status=document.getElementById('schedule-status');let draft=defaults(),loaded=false,busy=false,dirty=false;
  const text=(ko,en)=>language()==='en'?en:ko;
  function node(tag,value){const n=document.createElement(tag);if(value)n.textContent=value;if(['input','select','button'].includes(tag))n.disabled=busy||!loaded;return n;}
  function select(items,value,change){const n=node('select');for(const [key,label]of items){const o=node('option',label);o.value=key;n.append(o);}n.value=String(value);n.onchange=()=>{dirty=true;change(Number(n.value));};return n;}
  function field(parent,title,input){const l=node('label',title);l.append(input);parent.append(l);}
  function render(){box.replaceChildren();const hint=node('p',text('최대 8개 · NTP 필요 · 지나간 예약·없는 날짜는 건너뜁니다.','8 rules. Requires NTP. Missed events and missing dates are skipped.'));hint.className='muted';box.append(hint);
   const tz=select(Array.from({length:105},(_,i)=>{const m=-720+i*15,a=Math.abs(m);return [m,'UTC'+(m<0?'-':'+')+String(Math.floor(a/60)).padStart(2,'0')+':'+String(a%60).padStart(2,'0')];}),draft.timezone,x=>draft.timezone=x);field(box,text('시간대 · 서머타임 수동','Time zone · manual DST'),tz);
   draft.rules.forEach((r,i)=>{if(!r.mode)return;const card=node('div');card.className='name-editor';const head=node('div');head.className='row';head.append(node('b',text('예약 ','Rule ')+(i+1)));const enabled=node('input');enabled.type='checkbox';enabled.checked=!!(r.mode&128);enabled.onchange=()=>{dirty=true;r.mode=(r.mode&127)|(enabled.checked?128:0);};const lab=node('label',text('활성화','Enabled'));lab.prepend(enabled);head.append(lab);const del=node('button',text('삭제','Delete'));del.type='button';del.onclick=()=>{draft.rules.splice(i,1);draft.rules.push(empty());dirty=true;render();};head.append(del);card.append(head);
    field(card,text('반복','Repeat'),select([[1,text('매일','Daily')],[2,text('매주','Weekly')],[3,text('매월 지정일','Monthly date')],[4,text('매월 마지막 날','Last day of month')]],r.mode&127,x=>{r.mode=x|(r.mode&128);r.day=x===2?127:x===3?1:0;render();}));
    if((r.mode&127)===2){const days=node('div');days.className='row';[text('월','Mon'),text('화','Tue'),text('수','Wed'),text('목','Thu'),text('금','Fri'),text('토','Sat'),text('일','Sun')].forEach((name,k)=>{const l=node('label',name),c=node('input');c.type='checkbox';c.checked=!!(r.day&(1<<k));c.onchange=()=>{dirty=true;r.day=c.checked?r.day|(1<<k):r.day&~(1<<k);};l.prepend(c);days.append(l);});card.append(days);}
    if((r.mode&127)===3)field(card,text('날짜','Day'),select(Array.from({length:31},(_,k)=>[k+1,String(k+1)]),r.day,x=>r.day=x));
    /* Native time inputs follow the browser/OS locale, not dashboard language. */
    const time=node('div');time.className='schedule-time';let hour,minute,period;const update=()=>{dirty=true;r.minute=hour.validity.valid&&minute.validity.valid?(Number(hour.value)%12+Number(period.value)*12)*60+Number(minute.value):-1;};
    function part(name,min,max,value){const n=node('input');n.type='number';n.min=min;n.max=max;n.step=1;n.required=true;n.value=String(value).padStart(2,'0');n.id='schedule-'+name+'-'+i;n.ariaLabel=name;n.oninput=update;return n;}
    hour=part('hour',1,12,Math.floor(r.minute/60)%12||12);minute=part('minute',0,59,r.minute%60);
    period=select([[0,text('오전','AM')],[1,text('오후','PM')]],Math.floor(r.minute/720),update);period.id='schedule-period-'+i;
    time.append(period,hour,node('span',':'),minute);field(card,text('시각','Time'),time);
    field(card,text('대상','Target'),select([0,1,2,3,4].map(k=>[k,k===4?text('전체 채널','All channels'):text('채널 ','Channel ')+(k+1)]),r.channel,x=>r.channel=x));field(card,text('동작','Action'),select([[1,'ON'],[0,'OFF']],r.on,x=>r.on=x));box.append(card);
   });
   const actions=node('div');actions.className='row';const add=node('button',text('예약 추가','Add rule'));add.type='button';add.disabled=busy||!loaded||draft.rules.every(r=>r.mode);add.onclick=()=>{const i=draft.rules.findIndex(r=>!r.mode);if(i<0)return;draft.rules[i]={minute:600,mode:129,day:0,channel:4,on:0};dirty=true;render();};const apply=node('button',text('기기에 저장','Save to device'));apply.type='submit';apply.className='primary';apply.disabled=busy||!loaded||!available();actions.append(add,apply);box.append(actions);
  }
  form.onsubmit=async e=>{e.preventDefault();if(busy||!loaded||!available())return;try{const hex=encode(draft);busy=true;render();await save(hex);dirty=false;status.textContent=text('저장했습니다.','Saved.');}catch(_){status.textContent=text('저장 실패 · 설정·연결 확인','Save failed. Check settings/connection.');}finally{busy=false;render();}};
  render();return {load(hex){if(dirty||busy)return;draft=decode(hex);loaded=true;render();},translate:render,ready(){const button=box.querySelector('button[type=submit]');if(button)button.disabled=busy||!loaded||!available();}};
 }
 root.MttlSchedule={encode,decode,defaults,mount};if(typeof module==='object'&&module.exports)module.exports=root.MttlSchedule;
})(globalThis);
