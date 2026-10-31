#pragma once
// Debug web page markup.
//
// Kept out of the .ino on purpose: the Arduino builder preprocesses .ino files
// for prototypes and mis-parses a long multi-line raw string (it counts braces
// inside the CSS/JS and loses track of the literal), which made the compiler
// treat the embedded JavaScript as C++.  Headers are left alone.
//
// Layout: two top-level entries -- 在线使用 (chat) and 设置 (configuration).
// Settings is a stack of collapsible sections so nothing is buried in one wall
// of controls, and each field applies on its own.
static const char DEBUG_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html lang="zh"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VoiceAssistant</title><style>
body{font-family:system-ui,sans-serif;margin:0;padding:16px;background:#0e141a;color:#dfe7ee}
h2{font-size:16px;margin:0 0 10px;color:#7fd4ff}
h3{font-size:14px;margin:0 0 8px;color:#8fb6cc}
.card{background:#131c24;border:1px solid #22323f;border-radius:8px;padding:12px;margin-bottom:14px}
label{display:block;font-size:12px;color:#8fa3b3;margin:6px 0 2px}
input,button,select{font-size:15px;padding:8px;border-radius:6px;border:1px solid #2b3d4d;
background:#0b1218;color:#dfe7ee;width:100%;box-sizing:border-box}
button{background:#1c4f70;border-color:#2f7ba6;margin-top:8px;cursor:pointer}
button:active{background:#2f7ba6}
pre{white-space:pre-wrap;word-break:break-word;font-size:13px;margin:6px 0 0;color:#b8f0c8}
.slot{display:flex;gap:8px;align-items:center;font-size:13px;padding:2px 0;color:#9fb3c4}
.slot b{color:#fff;min-width:74px}
.row{display:flex;gap:8px}
.row>*{flex:1}
nav.top{display:flex;gap:10px;margin-bottom:14px}
nav.top button{margin:0;font-size:16px;padding:12px 8px;background:#16222c;border-color:#26384a}
nav.top button.on{background:#1c4f70;border-color:#2f7ba6;color:#fff}
details.sec{background:#131c24;border:1px solid #22323f;border-radius:8px;margin-bottom:10px}
details.sec>summary{cursor:pointer;padding:12px;font-size:15px;color:#7fd4ff;list-style:none}
details.sec>summary::-webkit-details-marker{display:none}
details.sec>summary:before{content:"\25B8  ";color:#4d7f9e}
details.sec[open]>summary:before{content:"\25BE  "}
details.sec>.body{padding:0 12px 12px}
.panel{display:none}
.panel.on{display:block}
</style></head><body>
<h2>VoiceAssistant</h2>
<nav class="top">
  <button data-t="live" class="on" onclick="top_('live')">在线使用</button>
  <button data-t="cfg" onclick="top_('cfg')">设置</button>
</nav>

<div class="panel on" id="p-live">
  <div class="card"><div id="status">加载中…</div></div>
  <div class="card">
    <label>询问</label>
    <input id="q" type="text" placeholder="问点什么…">
    <button onclick="ask()">发送</button>
    <pre id="ans"></pre>
  </div>
  <div class="card">
    <h3>会话</h3>
    <select id="sess"></select>
    <button onclick="switchSess()">切换会话</button>
    <div id="sessions"></div>
  </div>
  <div class="card">
    <h3>上下文</h3>
    <button onclick="ctx()">刷新上下文</button>
    <button onclick="clearCtx()" style="background:#5c2430;border-color:#8d3a4a">清除当前会话上下文</button>
    <pre id="context"></pre>
  </div>
</div>

<div class="panel" id="p-cfg">
  <details class="sec" open><summary>Wi-Fi 网络</summary><div class="body">
    <label>当前</label><div class="slot" id="wifinow">-</div>
    <label>SSID</label><input id="w_ssid" type="text" placeholder="例如 2-2-303">
    <label>密码（留空则用已绑定的预设槽）</label><input id="w_pw" type="text">
    <button onclick="joinWifi()">连接</button>
    <div id="wres"></div>
    <label>预设密码槽 (1-8)</label>
    <div class="row"><select id="slot"></select>
      <input id="pass" type="text" placeholder="新密码，留空=清除"></div>
    <button onclick="saveSlot()">保存槽位</button>
    <label>SSID 绑定槽位</label>
    <div class="row"><input id="ssid" type="text" placeholder="SSID">
      <select id="mslot"></select></div>
    <button onclick="saveMap()">保存绑定</button>
    <label>删除 SSID（同时解除密码绑定）</label>
    <div class="row"><input id="d_ssid" type="text" placeholder="要删除的 SSID">
      <button onclick="delWifi()" style="background:#5c2430;border-color:#8d3a4a">删除</button></div>
    <div id="d_wres"></div>
    <div id="slots"></div>
  </div></details>

  <details class="sec"><summary>AI Provider 与模型</summary><div class="body">
    <label>当前</label><div class="slot" id="ainow">-</div>
    <select id="prov"></select>
    <button onclick="setProv()">切换 provider</button>
    <select id="mdl"></select>
    <button onclick="setMdl()">切换模型</button>
    <div id="models"></div>
    <details id="delBox" style="margin-top:10px">
      <summary>－ 删除 provider / 模型</summary>
      <label>要删除的 provider</label>
      <div class="row"><select id="d_prov"></select>
        <button onclick="delProv()" style="background:#5c2430;border-color:#8d3a4a">删除</button></div>
      <label>要删除的模型 id</label>
      <div class="row"><select id="d_mdl"></select>
        <button onclick="delMdl()" style="background:#5c2430;border-color:#8d3a4a">删除</button></div>
      <div id="delres"></div>
    </details>
    <details id="addProvBox" style="margin-top:10px">
      <summary>＋ 添加 / 覆盖 provider</summary>
      <label>名称</label><input id="np_name" type="text" placeholder="例如 deepseek">
      <label>Base URL</label><input id="np_url" type="text" placeholder="https://api.deepseek.com/v1">
      <label>API Key（留空表示不修改已有 key）</label><input id="np_key" type="text">
      <label>默认模型 id（可留空）</label><input id="np_model" type="text">
      <label>默认模型是否支持识图（必选）</label><select id="np_vision"><option value="">请选择</option><option value="true">支持识图</option><option value="false">不支持，仅文本</option></select>
      <button onclick="addProv()">保存 provider</button>
      <div id="npres"></div>
    </details>
    <details id="addModelBox" style="margin-top:10px">
      <summary>＋ 为当前 provider 添加模型 id</summary>
      <label>模型 id</label><input id="nm_id" type="text" placeholder="deepseek-reasoner">
      <label>是否支持图像识别（必选）</label><select id="nm_vision"><option value="">请选择</option><option value="true">支持识图</option><option value="false">不支持，仅文本</option></select>
      <button onclick="addModel()">添加或更新模型</button>
      <div id="nmres"></div>
    </details>
  </div></details>

  <details class="sec"><summary>显示</summary><div class="body">
    <label>屏幕亮度 (1-100)</label>
    <div class="row"><input id="s_bright" type="number" min="1" max="100">
      <button onclick="apply({brightness:$('s_bright').value})">应用</button></div>
    <label>自动休眠（0 = 永不息屏）</label>
    <div class="row"><input id="s_sleep" type="number" min="0">
      <button onclick="apply({sleep:$('s_sleep').value})">应用</button></div>
    <label>全屏时钟</label>
    <div class="row"><select id="s_clock"><option value="0">关闭</option><option value="1">开启</option></select>
      <button onclick="apply({clock:$('s_clock').value})">应用</button></div>
    <label>界面语言</label>
    <div class="row"><select id="s_lang"><option value="zh">中文</option><option value="en">English</option></select>
      <button onclick="apply({language:$('s_lang').value})">应用</button></div>
  </div></details>

  <details class="sec"><summary>推理与搜索</summary><div class="body">
    <label>推理模式</label>
    <div class="row"><select id="s_think"><option value="0">关闭</option><option value="1">开启</option></select>
      <button onclick="apply({thinking:$('s_think').value})">应用</button></div>
    <label>思考过程显示</label>
    <div class="row"><select id="s_think_show"><option value="0">隐藏</option><option value="1">显示</option></select>
      <button onclick="apply({thinking_show:$('s_think_show').value})">应用</button></div>
    <label>联网搜索模式</label>
    <div class="row"><select id="s_search">
      <option value="0">关闭</option><option value="1">开启</option>
      <option value="2">自动</option><option value="3">Agent</option></select>
      <button onclick="apply({search:$('s_search').value})">应用</button></div>
  </div></details>
</div>

<script>
const $=id=>document.getElementById(id);
function top_(t){document.querySelectorAll('nav.top button').forEach(b=>b.classList.toggle('on',b.dataset.t===t));
 document.querySelectorAll('.panel').forEach(p=>p.classList.toggle('on',p.id==='p-'+t));}
async function post(u,d){let r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
 body:new URLSearchParams(d||{})});return await r.text();}
async function get(u){let r=await fetch(u);return await r.text();}
async function status(){try{let t=await get('/api/status');$('status').textContent=t;
 let o={};t.split('\n').forEach(l=>{let i=l.indexOf(' ');if(i>0)o[l.slice(0,i)]=l.slice(i+1);});
 $('wifinow').textContent=o['已连']||'-';}catch(e){$('status').textContent='离线';}}
async function slots(){$('slots').innerHTML=await get('/api/slots');}
async function sessions(){$('sessions').innerHTML=await get('/api/sessions');}
async function ctx(){let b=$('context');b.textContent='读取中…';
 try{b.textContent=await get('/api/context?limit=20');}catch(e){b.textContent='读取失败';}}
async function models(){let h=await get('/api/models');$('models').innerHTML=h;
 let d=document.createElement('div');d.innerHTML=h;
 let texts=[...d.querySelectorAll('.slot')].map(e=>e.textContent);
 let cur=texts.find(t=>t.startsWith('当前'))||'';
 let curText=cur.replace('当前','').trim();
 $('ainow').textContent=curText;
 // The board reports "deepseek / deepseek-flash" as active.  Seed the pickers
 // from it: without this the selects just showed their first option, so the page
 // kept claiming the old provider after a switch that had actually succeeded.
 let curParts=curText.split('/');
 let curProv=(curParts[0]||'').trim();
 let curModel=(curParts[1]||'').trim();
 let pick=(pfx)=>{let s=texts.find(t=>t.startsWith(pfx))||'';
  return (s.replace(pfx,'').match(/\[([^\]]+)\]|([^\s\[\]]+)/g)||[]).map(x=>x.replace(/[\[\]]/g,''));};
 let provs=pick('Provider'), mdls=pick('Models');
 // The active entry is not always in the profile's own list (a model can be set
 // ad-hoc), so add it when missing -- otherwise the select has nothing matching
 // the current value and falls back to the first option.
 if(curProv&&!provs.includes(curProv))provs.unshift(curProv);
 if(curModel&&!mdls.includes(curModel))mdls.unshift(curModel);
 $('prov').innerHTML=provs.map(n=>`<option>${n}</option>`).join('');
 $('mdl').innerHTML=mdls.map(n=>`<option>${n}</option>`).join('');
 if(curProv)$('prov').value=curProv;
 if(curModel)$('mdl').value=curModel;
 // Same lists feed the delete pickers.
 $('d_prov').innerHTML=provs.map(n=>`<option>${n}</option>`).join('');
 $('d_mdl').innerHTML=mdls.map(n=>`<option>${n}</option>`).join('');}
async function settings(){let t=await get('/api/settings');let o={};
 t.split('\n').forEach(l=>{let i=l.indexOf('=');if(i>0)o[l.slice(0,i)]=l.slice(i+1);});
 if(o.brightness!==undefined)$('s_bright').value=o.brightness;
 if(o.sleep!==undefined)$('s_sleep').value=o.sleep;
 if(o.clock!==undefined)$('s_clock').value=o.clock;
 if(o.language!==undefined)$('s_lang').value=o.language;
 if(o.thinking!==undefined)$('s_think').value=o.thinking;
 if(o.thinking_show!==undefined)$('s_think_show').value=o.thinking_show;
 if(o.search!==undefined)$('s_search').value=o.search;
 if(o.ssid!==undefined&&o.ssid!=='(none)')$('w_ssid').placeholder=o.ssid;}
async function saveSlot(){let t=await post('/api/slot',{slot:$('slot').value,pass:$('pass').value});
 $('pass').value='';alert(t);slots();}
async function saveMap(){alert(await post('/api/map',{ssid:$('ssid').value,slot:$('mslot').value}));slots();}
async function joinWifi(){let b=$('wres');b.textContent='连接中…（可能需要十几秒）';
 b.textContent=await post('/api/wifi',{ssid:$('w_ssid').value,password:$('w_pw').value});
 $('w_pw').value='';status();settings();}
async function setProv(){alert(await post('/api/provider',{name:$('prov').value}));status();models();settings();}
async function setMdl(){alert(await post('/api/model',{id:$('mdl').value}));status();models();settings();}
async function apply(o){alert(await post('/api/settings',o));status();settings();}
async function addProv(){let b=$('npres');if(!$('np_vision').value){b.textContent='请选择默认模型是否支持识图';return;}b.textContent='保存中…';
 let t=await post('/api/provider/add',{name:$('np_name').value,url:$('np_url').value,
  key:$('np_key').value,model:$('np_model').value,vision:$('np_vision').value});
 b.textContent=t;$('np_key').value='';$('np_model').value='';
 if(t.indexOf('已保存')===0){$('addProvBox').open=false;status();models();settings();}}
async function addModel(){let b=$('nmres');if(!$('nm_vision').value){b.textContent='请先选择模型是否支持识图';return;}b.textContent='添加中…';
 let t=await post('/api/model/add',{id:$('nm_id').value,vision:$('nm_vision').value});
 b.textContent=t;$('nm_id').value='';
 if(t.indexOf('已为')===0){$('addModelBox').open=false;status();models();settings();}}
async function delProv(){let n=$('d_prov').value;
 if(!n||!confirm('确定删除 provider「'+n+'」？'))return;
 let t=await post('/api/provider/del',{name:n});$('delres').textContent=t;
 if(t.indexOf('已删除')===0){status();models();settings();}}
async function delMdl(){let n=$('d_mdl').value;
 if(!n||!confirm('确定删除模型「'+n+'」？'))return;
 let t=await post('/api/model/del',{id:n});$('delres').textContent=t;
 if(t.indexOf('已删除')===0){status();models();settings();}}
async function delWifi(){let n=$('d_ssid').value;
 if(!n||!confirm('确定删除 SSID「'+n+'」及其密码绑定？'))return;
 $('d_wres').textContent=await post('/api/wifi/del',{ssid:n});
 $('d_ssid').value='';slots();status();settings();}
async function switchSess(){alert(await post('/api/session',{session:$('sess').value}));
 status();sessions();ctx();settings();}
async function clearCtx(){if(!confirm('确定清空当前会话的全部上下文？此操作不可撤销。'))return;
 alert(await post('/api/clearchat',{}));status();sessions();ctx();}
async function ask(){let b=$('ans');b.textContent='思考中…';
 try{b.textContent=await post('/api/ask',{q:$('q').value});}catch(e){b.textContent='请求失败';}
 ctx();}
function fillSlots(){let a='',b='',c='';
 for(let i=1;i<=8;i++){a+=`<option>${i}</option>`;b+=`<option>${i}</option>`;}
 for(let i=1;i<=5;i++){c+=`<option>${i}</option>`;}
 $('slot').innerHTML=a;$('mslot').innerHTML=b;$('sess').innerHTML=c;}
fillSlots();status();slots();sessions();models();settings();ctx();
setInterval(status,10000);
</script></body></html>)HTML";
