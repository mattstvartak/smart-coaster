#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>Smart Coaster</title>
<style>
:root{--bg:#0d0d1a;--card:#161628;--card2:#1c1c35;--accent:#e94560;--accent2:#c73852;--green:#2ecc71;--green2:#27ae60;--red:#e74c3c;--text:#f0f0f0;--dim:#777;--border:#252545;--input:#1a1a35;--radius:12px;--shadow:0 4px 15px rgba(0,0,0,.4)}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--text);max-width:500px;margin:0 auto;padding:16px 16px 80px;min-height:100vh}
h1{text-align:center;font-size:22px;margin:8px 0 20px;letter-spacing:1px}
h1 span{color:var(--accent)}
h2{font-size:18px;margin-bottom:16px;color:var(--text)}
h3{color:var(--accent);font-size:13px;text-transform:uppercase;letter-spacing:1px;margin:16px 0 8px}

/* Tabs */
.tabs{display:flex;background:var(--card);border-radius:var(--radius);padding:4px;margin-bottom:20px;box-shadow:var(--shadow)}
.tab{flex:1;padding:10px;text-align:center;background:transparent;border:none;color:var(--dim);border-radius:9px;cursor:pointer;font-size:14px;font-weight:600;transition:all .2s}
.tab.active{background:var(--accent);color:#fff;box-shadow:0 2px 8px rgba(233,69,96,.4)}

/* Cards */
.card{background:var(--card);border-radius:var(--radius);padding:14px;margin-bottom:10px;border:1px solid var(--border);cursor:pointer;transition:transform .15s,border-color .2s;box-shadow:var(--shadow)}
.card:active{transform:scale(.98);border-color:var(--accent)}
.card-static{cursor:default}.card-static:active{transform:none}
.card-title{font-size:16px;font-weight:700;margin-bottom:4px}
.card-sub{color:var(--dim);font-size:13px;line-height:1.4}
.card-badge{display:inline-block;background:var(--accent);color:#fff;font-size:11px;padding:2px 8px;border-radius:20px;margin-top:6px;font-weight:600}

/* Buttons */
.btn{padding:12px 24px;border:none;border-radius:var(--radius);cursor:pointer;font-size:14px;font-weight:700;transition:all .2s;display:inline-flex;align-items:center;justify-content:center;gap:6px}
.btn:active{transform:scale(.96)}
.btn-primary{background:linear-gradient(135deg,var(--accent),var(--accent2));color:#fff;width:100%;box-shadow:0 4px 15px rgba(233,69,96,.3)}
.btn-save{background:linear-gradient(135deg,var(--green),var(--green2));color:#fff;box-shadow:0 4px 15px rgba(46,204,113,.3)}
.btn-cancel{background:var(--card2);color:var(--dim);border:1px solid var(--border)}
.btn-del{background:var(--red);color:#fff}
.btn-sm{padding:8px 14px;font-size:12px;border-radius:8px}
.btn-outline{background:transparent;border:2px solid var(--border);color:var(--dim)}
.btn-outline:active{border-color:var(--accent);color:var(--accent)}
.btn-icon{width:36px;height:36px;padding:0;border-radius:8px;font-size:18px;background:rgba(231,76,60,.15);color:var(--red);border:none;cursor:pointer;flex-shrink:0}
.btn-icon:active{background:rgba(231,76,60,.3)}

/* Forms */
input,select{width:100%;padding:12px;margin:4px 0;border-radius:8px;border:1px solid var(--border);background:var(--input);color:var(--text);font-size:15px;transition:border-color .2s}
input:focus{border-color:var(--accent);outline:none;box-shadow:0 0 0 3px rgba(233,69,96,.15)}
input::placeholder{color:#555}
label{font-size:12px;color:var(--dim);text-transform:uppercase;letter-spacing:.5px;display:block;margin-top:12px;margin-bottom:2px}
select.sel{width:auto;padding:8px 32px 8px 12px;appearance:none;background:var(--input) url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='8'%3E%3Cpath d='M1 1l5 5 5-5' stroke='%23777' stroke-width='2' fill='none'/%3E%3C/svg%3E") no-repeat right 10px center;cursor:pointer}

/* Ingredient rows */
.ing-row{background:var(--card2);border-radius:8px;padding:10px;margin:6px 0;border:1px solid var(--border)}
.ing-row .row:first-child{margin-bottom:6px}
.row{display:flex;gap:8px;align-items:center}
.flex-1{flex:1}
.ml-label,.oz-label{position:relative}
.ml-label::after,.oz-label::after{position:absolute;right:10px;top:50%;transform:translateY(-50%);color:var(--dim);font-size:12px;pointer-events:none}
.ml-label::after{content:'ml'}
.oz-label::after{content:'oz'}
.ml-label input,.oz-label input{padding-right:30px}

/* Settings */
.setting-section{margin-bottom:20px}
.setting-section-title{font-size:11px;text-transform:uppercase;letter-spacing:1.5px;color:var(--dim);margin-bottom:8px;padding-left:4px}
.setting-row{display:flex;justify-content:space-between;align-items:center;padding:14px 16px;background:var(--card);border:1px solid var(--border)}
.setting-row:first-child{border-radius:var(--radius) var(--radius) 0 0}
.setting-row:last-child{border-radius:0 0 var(--radius) var(--radius)}
.setting-row:only-child{border-radius:var(--radius)}
.setting-row+.setting-row{border-top:none}
.setting-label{font-size:14px;font-weight:500}
.setting-value{color:var(--dim);font-size:13px;font-family:monospace}

/* Toggle */
.toggle{position:relative;width:50px;height:28px;background:#333;border-radius:14px;cursor:pointer;transition:background .3s;flex-shrink:0}
.toggle.on{background:var(--green)}
.toggle::after{content:'';position:absolute;width:24px;height:24px;background:#fff;border-radius:50%;top:2px;left:2px;transition:transform .3s;box-shadow:0 2px 4px rgba(0,0,0,.3)}
.toggle.on::after{transform:translateX(22px)}

/* Status / misc */
.status{text-align:center;color:var(--dim);font-size:13px;margin-top:16px}
.hidden{display:none}
.divider{height:1px;background:var(--border);margin:20px 0}
.danger-zone{margin-top:24px;padding-top:20px;border-top:1px solid rgba(231,76,60,.2)}
.danger-zone .setting-section-title{color:var(--red)}

/* Toast */
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%) translateY(80px);background:var(--card);color:var(--text);padding:12px 24px;border-radius:var(--radius);box-shadow:0 8px 30px rgba(0,0,0,.5);font-size:14px;font-weight:600;transition:transform .3s;z-index:100;border:1px solid var(--border)}
.toast.show{transform:translateX(-50%) translateY(0)}
.toast.success{border-color:var(--green)}
.toast.error{border-color:var(--red)}

/* Animations */
@keyframes fadeIn{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}
.fade-in{animation:fadeIn .3s ease}
</style></head><body>

<h1><span>Smart</span>Coaster</h1>

<div class="tabs" id="main-tabs">
<button class="tab active" onclick="showTab('recipes',this)">Recipes</button>
<button class="tab" onclick="showTab('settings',this)">Settings</button>
</div>

<!-- RECIPES TAB -->
<div id="recipes-tab" class="fade-in">
<button class="btn btn-primary" onclick="editRecipe(-1)">+ New Recipe</button>
<div style="height:12px"></div>
<div id="recipe-list"></div>
<p class="status" id="recipe-count"></p>
</div>

<!-- SETTINGS TAB -->
<div id="settings-tab" class="hidden">
<div class="setting-section fade-in">
<div class="setting-section-title">Device</div>
<div class="setting-row">
<span class="setting-label">Sound</span>
<div id="snd-toggle" class="toggle on" onclick="toggleSound()"></div>
</div>
<div class="setting-row">
<span class="setting-label">Screen Brightness</span>
<select class="sel" id="scr-sel" onchange="updateSetting('screenLevel',this.value)">
<option value="0">Dim</option><option value="1">Normal</option><option value="2">Bright</option>
</select>
</div>
<div class="setting-row">
<span class="setting-label">LED Ring</span>
<select class="sel" id="led-sel" onchange="updateSetting('ledLevel',this.value)">
<option value="0">Dim</option><option value="1">Normal</option><option value="2">Bright</option>
</select>
</div>
</div>

<div class="setting-section fade-in">
<div class="setting-section-title">Battery</div>
<div class="setting-row">
<span class="setting-label">Status</span>
<span class="setting-value" id="batt-status">--</span>
</div>
<div class="setting-row">
<span class="setting-label">Level</span>
<div style="display:flex;align-items:center;gap:8px">
<div style="width:80px;height:14px;border-radius:7px;border:1px solid var(--dim);overflow:hidden">
<div id="batt-bar" style="height:100%;width:0%;border-radius:7px;transition:width .5s"></div>
</div>
<span class="setting-value" id="batt-pct">--%</span>
</div>
</div>
<div class="setting-row">
<span class="setting-label">Voltage</span>
<span class="setting-value" id="batt-volt">--V</span>
</div>
</div>

<div class="setting-section fade-in">
<div class="setting-section-title">Network</div>
<div class="setting-row">
<span class="setting-label">WiFi Name</span>
<span class="setting-value" id="wifi-ssid">--</span>
</div>
<div class="setting-row">
<span class="setting-label">Password</span>
<span class="setting-value" id="wifi-pass">--</span>
</div>
<div class="setting-row">
<span class="setting-label">URL</span>
<span class="setting-value" id="wifi-host">--</span>
</div>
<div class="setting-row">
<span class="setting-label">IP Address</span>
<span class="setting-value" id="wifi-ip">--</span>
</div>
<div class="setting-row" style="flex-direction:column;align-items:stretch;gap:8px">
<span class="setting-label">Change WiFi Password</span>
<div class="row">
<input id="new-pass" type="text" placeholder="New password (8+ chars)" style="margin:0" autocomplete="off">
<button class="btn btn-sm btn-save" onclick="changePassword()">Save</button>
</div>
</div>
</div>

<div class="setting-section fade-in">
<div class="setting-section-title">Recipes</div>
<button class="btn btn-outline" style="width:100%" onclick="resetDefaults()">Restore Default Recipes</button>
</div>

<div class="danger-zone fade-in">
<div class="setting-section-title">Danger Zone</div>
<button class="btn btn-outline" style="width:100%;border-color:rgba(231,76,60,.4);color:var(--red)" onclick="deviceReset()">Factory Reset Device</button>
<p style="color:var(--dim);font-size:11px;margin-top:6px;text-align:center">Erases all recipes, settings, and restarts the device</p>
</div>
</div>

<!-- EDIT RECIPE TAB -->
<div id="edit-tab" class="hidden">
<div class="fade-in">
<h2 id="edit-title"></h2>
<label>Drink Name</label>
<input id="e-name" placeholder="e.g. Old Fashioned" autocomplete="off">

<h3>Measured Ingredients</h3>
<div id="e-weighed"></div>
<button class="btn btn-sm btn-outline" style="margin:8px 0" onclick="addWeighedRow()">+ Add Ingredient</button>

<h3>Unweighed Steps</h3>
<div id="e-unweighed"></div>
<button class="btn btn-sm btn-outline" style="margin:8px 0" onclick="addUnweighedRow()">+ Add Step</button>

<label>Finishing Instruction</label>
<input id="e-finish" placeholder="e.g. Shake & strain" autocomplete="off">

<div class="row" style="margin-top:20px">
<button class="btn btn-save flex-1" onclick="saveRecipe()">Save Recipe</button>
<button class="btn btn-cancel flex-1" onclick="cancelEdit()">Cancel</button>
</div>
<div id="del-wrap" style="margin-top:12px">
<button class="btn btn-del" style="width:100%" onclick="deleteRecipe()">Delete Recipe</button>
</div>
</div>
</div>

<!-- TOAST -->
<div class="toast" id="toast"></div>

<script>
let recipes=[],editId=-1;

function showTab(t,btn){
document.querySelectorAll('.tab').forEach(b=>b.classList.remove('active'));
if(btn)btn.classList.add('active');
['recipes-tab','settings-tab','edit-tab'].forEach(id=>{
const el=document.getElementById(id);
el.classList.toggle('hidden',id!==t+'-tab');
});
document.getElementById('main-tabs').classList.toggle('hidden',t==='edit');
}

function toast(msg,type){
const el=document.getElementById('toast');
el.textContent=msg;el.className='toast '+type+' show';
setTimeout(()=>el.classList.remove('show'),2200);
}

async function loadRecipes(){
try{const r=await fetch('/api/recipes');recipes=await r.json();renderList();}
catch(e){document.getElementById('recipe-list').innerHTML='<p class="status">Unable to connect</p>';}
}

async function loadSettings(){
try{
const r=await fetch('/api/settings');const s=await r.json();
document.getElementById('snd-toggle').classList.toggle('on',!s.muted);
document.getElementById('scr-sel').value=s.screenLevel;
document.getElementById('led-sel').value=s.ledLevel;
document.getElementById('wifi-ssid').textContent=s.ssid||'--';
document.getElementById('wifi-pass').textContent=s.pass||'--';
document.getElementById('wifi-host').textContent=s.hostname||'--';
document.getElementById('wifi-ip').textContent=s.ip||'--';
const pct=s.battPercent||0;const chg=s.battCharging||false;
document.getElementById('batt-status').textContent=chg?'Charging':'On Battery';
document.getElementById('batt-pct').textContent=pct+'%';
document.getElementById('batt-volt').textContent=(s.battVoltage||0).toFixed(2)+'V';
const bar=document.getElementById('batt-bar');
bar.style.width=pct+'%';
bar.style.background=chg?'var(--green)':pct>20?'var(--green)':pct>10?'#f39c12':'var(--red)';
}catch(e){}
}
setInterval(loadSettings,10000);

function renderList(){
const el=document.getElementById('recipe-list');
if(!recipes.length){
el.innerHTML='<p class="status">No recipes yet. Add one above!</p>';
document.getElementById('recipe-count').textContent='';
return;
}
el.innerHTML=recipes.map((d,i)=>{
const ings=d.weighed.map(w=>esc(w.name)).join(' + ');
const total=d.weighed.reduce((s,w)=>s+w.oz,0);
const totalStr=total%1===0?total.toFixed(0):total.toFixed(1);
return`<div class="card fade-in" onclick="editRecipe(${i})" style="animation-delay:${i*30}ms">
<div class="card-title">${esc(d.name)}</div>
<div class="card-sub">${ings}</div>
<span class="card-badge">${totalStr} oz total</span>
${d.finish?`<div class="card-sub" style="margin-top:4px">${esc(d.finish)}</div>`:''}
</div>`;
}).join('');
document.getElementById('recipe-count').textContent=recipes.length+' recipe'+(recipes.length!==1?'s':'');
}

function esc(s){if(!s)return'';return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}

function editRecipe(id){
editId=id;
const isNew=id===-1;
document.getElementById('edit-title').textContent=isNew?'New Recipe':'Edit Recipe';
document.getElementById('del-wrap').classList.toggle('hidden',isNew);
const d=isNew?{name:'',weighed:[],unweighed:[],finish:''}:JSON.parse(JSON.stringify(recipes[id]));
document.getElementById('e-name').value=d.name;
document.getElementById('e-finish').value=d.finish;
document.getElementById('e-weighed').innerHTML='';
if(d.weighed.length)d.weighed.forEach(w=>addWeighedRow(w.name,w.ml,w.oz));
else addWeighedRow();
document.getElementById('e-unweighed').innerHTML='';
d.unweighed.forEach(u=>addUnweighedRow(u));
showTab('edit');
window.scrollTo(0,0);
}

function addWeighedRow(name,ml,oz){
const div=document.createElement('div');div.className='ing-row fade-in';
div.innerHTML=`<div class="row"><input class="flex-1 w-name" placeholder="Ingredient name" value="${esc(name||'')}" autocomplete="off">
<button class="btn-icon" onclick="this.closest('.ing-row').remove()" aria-label="Remove">&times;</button></div>
<div class="row"><div class="ml-label flex-1"><input class="w-ml" type="number" inputmode="decimal" step="5" placeholder="0" value="${ml||''}"></div>
<div class="oz-label flex-1"><input class="w-oz" type="number" inputmode="decimal" step="0.25" placeholder="0" value="${oz||''}"></div></div>`;
document.getElementById('e-weighed').appendChild(div);
}

function addUnweighedRow(text){
const div=document.createElement('div');div.className='ing-row fade-in';
div.innerHTML=`<div class="row"><input class="flex-1 u-text" placeholder="e.g. 2 dashes Angostura" value="${esc(text||'')}" autocomplete="off">
<button class="btn-icon" onclick="this.closest('.ing-row').remove()" aria-label="Remove">&times;</button></div>`;
document.getElementById('e-unweighed').appendChild(div);
}

async function saveRecipe(){
const d={
name:document.getElementById('e-name').value.trim(),
weighed:[...document.querySelectorAll('#e-weighed .ing-row')].map(r=>({
name:r.querySelector('.w-name').value.trim(),
ml:parseFloat(r.querySelector('.w-ml').value)||0,
oz:parseFloat(r.querySelector('.w-oz').value)||0
})).filter(w=>w.name),
unweighed:[...document.querySelectorAll('#e-unweighed .u-text')].map(i=>i.value.trim()).filter(Boolean),
finish:document.getElementById('e-finish').value.trim()
};
if(!d.name){toast('Enter a drink name','error');return;}
if(!d.weighed.length){toast('Add at least one ingredient','error');return;}
try{
if(editId===-1){
await fetch('/api/recipes',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});
}else{
await fetch('/api/recipes?id='+editId,{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(d)});
}
await loadRecipes();showTab('recipes');
toast(editId===-1?'Recipe added':'Recipe updated','success');
}catch(e){toast('Save failed','error');}
}

async function deleteRecipe(){
if(!confirm('Delete "'+recipes[editId].name+'"?'))return;
try{
await fetch('/api/recipes',{method:'DELETE',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:editId})});
await loadRecipes();showTab('recipes');
toast('Recipe deleted','success');
}catch(e){toast('Delete failed','error');}
}

function cancelEdit(){showTab('recipes');}

async function toggleSound(){
const el=document.getElementById('snd-toggle');el.classList.toggle('on');
try{await fetch('/api/settings',{method:'PUT',headers:{'Content-Type':'application/json'},
body:JSON.stringify({muted:!el.classList.contains('on')})});
toast(el.classList.contains('on')?'Sound on':'Sound muted','success');
}catch(e){}
}

async function updateSetting(key,val){
const body={};body[key]=parseInt(val);
try{await fetch('/api/settings',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
toast('Setting updated','success');
}catch(e){}
}

async function resetDefaults(){
if(!confirm('Restore all recipes to defaults? Your custom recipes will be removed.'))return;
try{await fetch('/api/recipes/reset',{method:'POST'});await loadRecipes();
toast('Default recipes restored','success');
}catch(e){toast('Reset failed','error');}
}

async function deviceReset(){
if(!confirm('Factory reset? This erases ALL data and restarts the device.'))return;
if(!confirm('Are you sure? This cannot be undone.'))return;
try{await fetch('/api/device/reset',{method:'POST'});
toast('Device resetting...','success');
setTimeout(()=>{document.body.innerHTML='<div style="text-align:center;margin-top:40vh;color:#777"><p>Device is restarting...</p><p style="margin-top:8px;font-size:13px">Reconnect to SmartCoaster WiFi</p></div>';},1500);
}catch(e){toast('Reset failed','error');}
}

async function changePassword(){
const p=document.getElementById('new-pass').value.trim();
if(p.length<8){toast('Password must be 8+ characters','error');return;}
if(!confirm('Change WiFi password to "'+p+'"? Device will restart and you will need to reconnect.'))return;
try{
const r=await fetch('/api/settings',{method:'PUT',headers:{'Content-Type':'application/json'},body:JSON.stringify({pass:p})});
const d=await r.json();
if(d.error){toast(d.error,'error');return;}
toast('Password changed. Reconnecting...','success');
setTimeout(()=>{document.body.innerHTML='<div style="text-align:center;margin-top:40vh;color:#777"><p>Device is restarting...</p><p style="margin-top:8px;font-size:13px">Reconnect to SmartCoaster WiFi with new password</p></div>';},1500);
}catch(e){toast('Failed to change password','error');}
}

loadRecipes();loadSettings();
</script></body></html>)rawliteral";
