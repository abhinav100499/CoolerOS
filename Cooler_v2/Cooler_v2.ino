#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <time.h>

#define RELAY_PIN 5
#define ACTIVE_LOW true

WebServer server(80);
Preferences prefs;

String ssid="";
String password="";

bool relayState=false;

unsigned long coolerStartTime=0;

String scheduleJson="[]";

// Timer variables
struct Timer {
  bool active;
  unsigned long startTime;
  unsigned long duration; // in milliseconds
  int remainingMinutes;
  int remainingSeconds;
};
Timer activeTimer = {false, 0, 0, 0, 0};

// Manual override state
bool manualOverride = false;
unsigned long manualOnTime = 0;

// Schedule variables
#define MAX_SCHEDULES 10
struct Schedule {
  String id;
  int startHour;
  int startMinute;
  int endHour;
  int endMinute;
  bool days[7]; // MON-SUN: 0-6
  bool enabled;
};
Schedule schedules[MAX_SCHEDULES];
int scheduleCount = 0;

void setRelay(bool state)
{
  if (state) {
    // Manual ON - set manual override with timestamp
    manualOverride = true;
    manualOnTime = millis();
  } else {
    // Manual OFF - always wins, cancels everything
    manualOverride = false;
    if (activeTimer.active) {
      cancelTimer();
    }
  }
  
  relayState=state; 

  digitalWrite(RELAY_PIN,(ACTIVE_LOW?!state:state));

  if(state)
    coolerStartTime=millis();
}

// Timer functions
void setTimer(int hours, int minutes) {
  int totalMinutes = hours * 60 + minutes;
  if (totalMinutes < 1) return; // Minimum 1 minute, no upper limit
  
  activeTimer.active = true;
  activeTimer.startTime = millis();
  activeTimer.duration = totalMinutes * 60000UL; // Convert to milliseconds
  activeTimer.remainingMinutes = totalMinutes;
  activeTimer.remainingSeconds = 0;
  
  // Only turn on cooler if it's not already on
  if (!relayState) {
    setRelay(true);
  }
}

void cancelTimer() {
  activeTimer.active = false;
  activeTimer.startTime = 0;
  activeTimer.duration = 0;
  activeTimer.remainingMinutes = 0;
  activeTimer.remainingSeconds = 0;
}

void checkTimer() {
  if (!activeTimer.active) return;
  
  unsigned long elapsed = millis() - activeTimer.startTime;
  
  // Check if timer has expired
  if (elapsed >= activeTimer.duration) {
    // Timer expired - check if manual ON happened before timer expiry
    if (manualOverride && manualOnTime < activeTimer.startTime) {
      // Manual ON was before timer started - timer wins
      setRelay(false);
    } else if (!manualOverride) {
      // No manual override - timer wins
      setRelay(false);
    }
    // Always cancel timer
    cancelTimer();
  } else {
    // Update remaining time
    unsigned long remaining = activeTimer.duration - elapsed;
    activeTimer.remainingMinutes = remaining / 60000;
    activeTimer.remainingSeconds = (remaining % 60000) / 1000;
  }
}

String getTimerStatus() {
  if (!activeTimer.active) {
    return "INACTIVE";
  }
  // Only show remaining if cooler is actually on and timer is active
  if (!relayState) {
    return "CANCELLED";
  }
  return String(activeTimer.remainingMinutes) + "m " + String(activeTimer.remainingSeconds) + "s";
}

String formatDuration(unsigned long seconds)
{
  int h=seconds/3600;
  int m=(seconds%3600)/60;
  int s=seconds%60;

  return String(h)+"h "+String(m)+"m "+String(s)+"s";
}

void saveWiFi(String s,String p)
{
  prefs.begin("wifi",false);
  prefs.putString("ssid",s);
  prefs.putString("pass",p);
  prefs.end();
}

void loadWiFi()
{
  prefs.begin("wifi",true);
  ssid=prefs.getString("ssid","");
  password=prefs.getString("pass","");
  prefs.end();
}

// Schedule management functions
String generateScheduleId() {
  return "sch_" + String(millis());
}

void loadSchedules() {
  prefs.begin("sched", true);
  String json = prefs.getString("data", "[]");
  prefs.end();
  
  DynamicJsonDocument doc(4096);
  deserializeJson(doc, json);
  
  scheduleCount = 0;
  for (JsonObject sched : doc.as<JsonArray>()) {
    if (scheduleCount < MAX_SCHEDULES) {
      schedules[scheduleCount].id = sched["id"].as<String>();
      schedules[scheduleCount].startHour = sched["startHour"];
      schedules[scheduleCount].startMinute = sched["startMinute"];
      schedules[scheduleCount].endHour = sched["endHour"];
      schedules[scheduleCount].endMinute = sched["endMinute"];
      schedules[scheduleCount].enabled = sched["enabled"];
      
      JsonArray daysArray = sched["days"];
      for (int i = 0; i < 7; i++) {
        schedules[scheduleCount].days[i] = daysArray[i];
      }
      scheduleCount++;
    }
  }
}

void saveSchedules() {
  DynamicJsonDocument doc(4096);
  JsonArray schedArray = doc.to<JsonArray>();
  
  for (int i = 0; i < scheduleCount; i++) {
    JsonObject sched = schedArray.createNestedObject();
    sched["id"] = schedules[i].id;
    sched["startHour"] = schedules[i].startHour;
    sched["startMinute"] = schedules[i].startMinute;
    sched["endHour"] = schedules[i].endHour;
    sched["endMinute"] = schedules[i].endMinute;
    sched["enabled"] = schedules[i].enabled;
    
    JsonArray daysArray = sched.createNestedArray("days");
    for (int j = 0; j < 7; j++) {
      daysArray.add(schedules[i].days[j]);
    }
  }
  
  String json;
  serializeJson(doc, json);
  
  prefs.begin("sched", false);
  prefs.putString("data", json);
  prefs.end();
}

bool addSchedule(int startHour, int startMinute, int endHour, int endMinute, bool days[7]) {
  if (scheduleCount >= MAX_SCHEDULES) return false;
  
  // Validate time range
  if (startHour < 0 || startHour > 23 || endHour < 0 || endHour > 23) return false;
  if (startMinute < 0 || startMinute > 59 || endMinute < 0 || endMinute > 59) return false;
  
  // Check if any day is selected
  bool hasDay = false;
  for (int i = 0; i < 7; i++) {
    if (days[i]) {
      hasDay = true;
      break;
    }
  }
  if (!hasDay) return false;
  
  schedules[scheduleCount].id = generateScheduleId();
  schedules[scheduleCount].startHour = startHour;
  schedules[scheduleCount].startMinute = startMinute;
  schedules[scheduleCount].endHour = endHour;
  schedules[scheduleCount].endMinute = endMinute;
  schedules[scheduleCount].enabled = true;
  
  for (int i = 0; i < 7; i++) {
    schedules[scheduleCount].days[i] = days[i];
  }
  
  scheduleCount++;
  saveSchedules();
  return true;
}

bool removeSchedule(String id) {
  for (int i = 0; i < scheduleCount; i++) {
    if (schedules[i].id == id) {
      // Shift remaining schedules
      for (int j = i; j < scheduleCount - 1; j++) {
        schedules[j] = schedules[j + 1];
      }
      scheduleCount--;
      saveSchedules();
      return true;
    }
  }
  return false;
}

bool toggleSchedule(String id, bool enabled) {
  for (int i = 0; i < scheduleCount; i++) {
    if (schedules[i].id == id) {
      schedules[i].enabled = enabled;
      saveSchedules();
      return true;
    }
  }
  return false;
}

void checkSchedule()
{
  // Priority: Manual OFF > Timer > Schedule
  // If manual override is active, don't check schedules
  if (manualOverride) {
    return; // Skip schedule check during manual override
  }
  
  // If timer is active, don't check schedules
  if (activeTimer.active) {
    return;
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;
  
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;
  int currentDay = timeinfo.tm_wday; // 0=Sunday, 1=Monday, ..., 6=Saturday
  
  bool shouldTurnOn = false;
  
  for (int i = 0; i < scheduleCount; i++) {
    if (!schedules[i].enabled) continue;
    
    // Check if current day is selected (convert Sunday=0 to Monday=0 format)
    int dayIndex = (currentDay == 0) ? 6 : currentDay - 1; // Sunday->6, Monday->0, etc.
    if (!schedules[i].days[dayIndex]) continue;
    
    // Check time range
    int startTotalMinutes = schedules[i].startHour * 60 + schedules[i].startMinute;
    int endTotalMinutes = schedules[i].endHour * 60 + schedules[i].endMinute;
    int currentTotalMinutes = currentHour * 60 + currentMinute;
    
    // Handle overnight schedules (e.g., 22:00 to 06:00)
    if (endTotalMinutes < startTotalMinutes) {
      // Overnight schedule
      if (currentTotalMinutes >= startTotalMinutes || currentTotalMinutes <= endTotalMinutes) {
        shouldTurnOn = true;
        break;
      }
    } else {
      // Normal schedule
      if (currentTotalMinutes >= startTotalMinutes && currentTotalMinutes <= endTotalMinutes) {
        shouldTurnOn = true;
        break;
      }
    }
  }
  
  setRelay(shouldTurnOn);
}

void handleStatus()
{
  struct tm timeinfo;
  getLocalTime(&timeinfo);

  unsigned long uptime=millis()/1000;

  unsigned long runtime=0; 

  if(relayState)
  runtime=(millis()-coolerStartTime)/1000;

  DynamicJsonDocument doc(1024);

  doc["device"]="cooler-controller";
  doc["relay"]=relayState?"ON":"OFF";
  doc["ip"]=WiFi.localIP().toString();
  doc["uptime"]=formatDuration(uptime);
  doc["cooler_runtime"]=formatDuration(runtime);
  doc["time"]=String(timeinfo.tm_hour)+":"+String(timeinfo.tm_min);
  
  // Timer status
  doc["timer_active"]=activeTimer.active;
  doc["timer_remaining"]=getTimerStatus();
  if (activeTimer.active) {
    doc["timer_minutes"]=activeTimer.remainingMinutes;
    doc["timer_seconds"]=activeTimer.remainingSeconds;
  } else {
    doc["timer_minutes"]=0;
    doc["timer_seconds"]=0;
  }
  
  // New schedule data
  JsonArray schedArray = doc.createNestedArray("schedules");
  for (int i = 0; i < scheduleCount; i++) {
    JsonObject sched = schedArray.createNestedObject();
    sched["id"] = schedules[i].id;
    sched["startHour"] = schedules[i].startHour;
    sched["startMinute"] = schedules[i].startMinute;
    sched["endHour"] = schedules[i].endHour;
    sched["endMinute"] = schedules[i].endMinute;
    sched["enabled"] = schedules[i].enabled;
    
    JsonArray daysArray = sched.createNestedArray("days");
    for (int j = 0; j < 7; j++) {
      daysArray.add(schedules[i].days[j]);
    }
  }
  doc["schedule_count"] = scheduleCount;
  doc["max_schedules"] = MAX_SCHEDULES;

  String json;
  serializeJson(doc,json);

  server.send(200,"application/json",json);
}

void handleReset()
{
  prefs.begin("wifi",false);
  prefs.clear();
  prefs.end();

  prefs.begin("sched",false);
  prefs.clear();
  prefs.end();

  server.send(200,"text/plain","Resetting...");
  delay(1000);

  ESP.restart();
}

void handleAddSchedule() {
  if (!server.hasArg("startHour") || !server.hasArg("startMinute") || 
      !server.hasArg("endHour") || !server.hasArg("endMinute")) {
    server.send(400, "text/plain", "Missing time parameters");
    return;
  }
  
  int startHour = server.arg("startHour").toInt();
  int startMinute = server.arg("startMinute").toInt();
  int endHour = server.arg("endHour").toInt();
  int endMinute = server.arg("endMinute").toInt();
  
  bool days[7] = {false};
  String dayNames[7] = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};
  
  for (int i = 0; i < 7; i++) {
    if (server.hasArg(dayNames[i])) {
      days[i] = server.arg(dayNames[i]) == "true";
    }
  }
  
  if (addSchedule(startHour, startMinute, endHour, endMinute, days)) {
    server.send(200, "text/plain", "Schedule added successfully");
  } else {
    server.send(400, "text/plain", "Failed to add schedule (invalid data or full)");
  }
}

void handleRemoveSchedule() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "Missing schedule ID");
    return;
  }
  
  String id = server.arg("id");
  if (removeSchedule(id)) {
    server.send(200, "text/plain", "Schedule removed successfully");
  } else {
    server.send(400, "text/plain", "Schedule not found");
  }
}

void handleToggleSchedule() {
  if (!server.hasArg("id") || !server.hasArg("enabled")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  String id = server.arg("id");
  bool enabled = server.arg("enabled") == "true";
  
  if (toggleSchedule(id, enabled)) {
    String status = enabled ? "enabled" : "disabled";
    server.send(200, "text/plain", "Schedule " + status + " successfully");
  } else {
    server.send(400, "text/plain", "Schedule not found");
  }
}

void handleTimerStatus() {
  DynamicJsonDocument doc(256);
  doc["active"] = activeTimer.active;
  doc["remaining"] = getTimerStatus();
  doc["minutes"] = activeTimer.remainingMinutes;
  doc["seconds"] = activeTimer.remainingSeconds;
  
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSetTimer() {
  // Handle cases where parameters might be empty or missing
  String hoursStr = server.arg("hours");
  String minutesStr = server.arg("minutes");
  
  int hours = 0;
  int minutes = 0;
  
  // Parse hours - default to 0 if empty or invalid
  if (hoursStr != "") {
    hours = hoursStr.toInt();
  }
  
  // Parse minutes - default to 0 if empty or invalid  
  if (minutesStr != "") {
    minutes = minutesStr.toInt();
  }
  
  if (hours >= 0 && minutes >= 0 && minutes <= 60) {
    int totalMinutes = hours * 60 + minutes;
    if (totalMinutes >= 1) {
      setTimer(hours, minutes);
      String timeStr = "";
      if (hours > 0) timeStr += String(hours) + "h ";
      if (minutes > 0) timeStr += String(minutes) + "m";
      server.send(200, "text/plain", "Timer set for " + timeStr);
    } else {
      server.send(400, "text/plain", "Total time must be at least 1 minute");
    }
  } else {
    server.send(400, "text/plain", "Invalid range (0+ hours, 0-60 minutes)");
  }
}

void handleCancelTimer() {
  cancelTimer();
  server.send(200, "text/plain", "Timer cancelled");
}

void handleDashboard()
{
  String page=R"rawliteral(

<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">

<style>
body{font-family:Arial;background:#111;color:white}
.card{background:#1e1e1e;margin:10px;padding:15px;border-radius:10px}
button{padding:10px;margin:5px;border:none;border-radius:6px}
input{padding:5px;margin:5px}
</style>

</head>

<body>

<div class="card">
<h2>Cooler Controller</h2>
<p>Status: <span id="relay"></span></p>
<button onclick="fetch('/on')">ON</button>
<button onclick="fetch('/off')">OFF</button>
</div>

<div class="card">
<h3>Timer Control</h3>
<p>Timer Status: <span id="timer_status"></span></p>
<p>Time Remaining: <span id="timer_remaining"></span></p>
<input id="timer_hours" type="number" min="0" max="999" placeholder="Hours" size="4">
<input id="timer_minutes" type="number" min="0" max="60" placeholder="Minutes" size="3">
<button onclick="setTimer()">Set Timer</button>
<button onclick="cancelTimer()">Cancel Timer</button>
</div>

<div class="card">
<h3>Schedules</h3>
<div id="schedules_container"></div>
<button onclick="addScheduleForm()" id="add_schedule_btn">+ Add Schedule</button>
</div>

<div class="card">
<h3>Metrics</h3>
<p>Uptime: <span id="uptime"></span></p>
<p>Cooler runtime: <span id="runtime"></span></p>
<p>Time: <span id="time"></span></p>
</div>

<script>

let refreshInterval;
let isFormActive = false;

function startRefresh() {
  if (!isFormActive) {
    refreshInterval = setInterval(refresh, 3000);
  }
}

function stopRefresh() {
  if (refreshInterval) {
    clearInterval(refreshInterval);
  }
}

function refresh(){

fetch('/status')
.then(r=>r.json())
.then(d=>{

document.getElementById("relay").innerText=d.relay
document.getElementById("uptime").innerText=d.uptime
document.getElementById("runtime").innerText=d.cooler_runtime
document.getElementById("time").innerText=d.time
document.getElementById("timer_status").innerText=d.timer_active ? "ACTIVE" : "INACTIVE"
document.getElementById("timer_remaining").innerText=d.timer_remaining

// Update schedules display
updateSchedulesDisplay(d.schedules, d.schedule_count, d.max_schedules)

})

}

function updateSchedulesDisplay(schedules, count, maxSchedules) {
const container = document.getElementById('schedules_container')
const addBtn = document.getElementById('add_schedule_btn')

container.innerHTML = ''

// Get current day (0=Sunday, 1=Monday, ..., 6=Saturday)
const today = new Date().getDay()
const dayIndex = today === 0 ? 6 : today - 1 // Convert to Monday=0 format

schedules.forEach((sched, index) => {
  const div = document.createElement('div')
  div.className = 'schedule-item'
  
  // Check if schedule is for today and enabled
  const isForToday = sched.days[dayIndex]
  const isEnabled = sched.enabled
  
  // Set different styling based on enabled state and today
  let borderColor, bgColor, opacity, buttonBg
  
  if (!isEnabled) {
    // Disabled schedules - grayed out
    borderColor = '#666'
    bgColor = '#1a1a1a'
    opacity = '0.5'
    buttonBg = '#333'
  } else if (isForToday) {
    // Enabled & for today - green
    borderColor = '#4CAF50'
    bgColor = '#2a2a2a'
    opacity = '1'
    buttonBg = '#444'
  } else {
    // Enabled but not today - blue
    borderColor = '#2196F3'
    bgColor = '#2a2a2a'
    opacity = '0.8'
    buttonBg = '#444'
  }
  
  div.style.cssText = `background:${bgColor};margin:5px;padding:10px;border-radius:5px;border-left:4px solid ${borderColor};opacity:${opacity}`
  
  const days = ['MON','TUE','WED','THU','FRI','SAT','SUN']
  const selectedDays = days.filter((day, i) => sched.days[i]).join(', ')
  
  div.innerHTML = `
    <div style="display:flex;justify-content:space-between;align-items:center">
      <div>
        <strong style="color:${isEnabled ? '#fff' : '#888'}">${String(sched.startHour).padStart(2,'0')}:${String(sched.startMinute).padStart(2,'0')} - 
        ${String(sched.endHour).padStart(2,'0')}:${String(sched.endMinute).padStart(2,'0')}</strong>
        ${!isForToday && isEnabled ? '<span style="color:#888;font-size:12px;margin-left:10px">(Not today)</span>' : ''}
        ${!isEnabled ? '<span style="color:#ff666;font-size:12px;margin-left:10px">(DISABLED)</span>' : ''}
        <br>
        <small style="color:${isEnabled ? (isForToday ? '#ccc' : '#aaa') : '#666'}">${selectedDays}</small>
      </div>
      <div>
        <button onclick="editScheduleForm('${sched.id}')" style="padding:5px;margin:2px;background:#2196F3">Edit</button>
        <button onclick="toggleSchedule('${sched.id}', ${!sched.enabled})" style="padding:5px;margin:2px;background:${isEnabled ? '#ff9800' : '#4CAF50'}">
          ${sched.enabled ? 'Disable' : 'Enable'}
        </button>
        <button onclick="removeSchedule('${sched.id}')" style="padding:5px;margin:2px;background:#ff4444">Remove</button>
      </div>
    </div>
  `
  container.appendChild(div)
})

addBtn.style.display = count >= maxSchedules ? 'none' : 'inline-block'
}

function addScheduleForm() {
isFormActive = true;
stopRefresh();

const container = document.getElementById('schedules_container')
const form = document.createElement('div')
form.className = 'schedule-form'
form.style.cssText = 'background:#2a2a2a;margin:5px;padding:10px;border-radius:5px'

// Get current day for auto-selection
const today = new Date().getDay()
const todayIndex = today === 0 ? 6 : today - 1 // Convert to Monday=0 format
const todayDay = ['mon','tue','wed','thu','fri','sat','sun'][todayIndex]

form.innerHTML = `
  <h4>New Schedule</h4>
  <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
    <div>
      <label>Start Time:</label><br>
      <input type="time" id="new_start_time" style="padding:5px">
    </div>
    <div>
      <label>End Time:</label><br>
      <input type="time" id="new_end_time" style="padding:5px">
    </div>
  </div>
  <div style="margin-top:10px">
    <label>Days:</label><br>
    <div style="display:grid;grid-template-columns:repeat(7,1fr);gap:5px">
      ${['MON','TUE','WED','THU','FRI','SAT','SUN'].map((day, i) => {
        const dayValue = day.toLowerCase()
        const isToday = dayValue === todayDay
        return `<label style="font-size:12px"><input type="checkbox" value="${dayValue}" name="day" ${isToday ? 'checked' : ''}> ${day}</label>`
      }).join('')}
    </div>
  </div>
  <div style="margin-top:10px">
    <button onclick="saveNewSchedule()" style="padding:5px 10px">Save</button>
    <button onclick="cancelScheduleForm()" style="padding:5px 10px;background:#666">Cancel</button>
  </div>
`

container.appendChild(form)
document.getElementById('add_schedule_btn').style.display = 'none'
}

function cancelScheduleForm() {
isFormActive = false;
const forms = document.querySelectorAll('.schedule-form')
forms.forEach(form => form.remove())
document.getElementById('add_schedule_btn').style.display = 'inline-block'
startRefresh();
}

function saveNewSchedule() {
const startTime = document.getElementById('new_start_time').value
const endTime = document.getElementById('new_end_time').value

if (!startTime || !endTime) {
  alert('Please select both start and end times')
  return
}

const [startHour, startMinute] = startTime.split(':').map(Number)
const [endHour, endMinute] = endTime.split(':').map(Number)

const dayCheckboxes = document.querySelectorAll('input[name="day"]:checked')
const params = new URLSearchParams()
params.append('startHour', startHour)
params.append('startMinute', startMinute)
params.append('endHour', endHour)
params.append('endMinute', endMinute)

if (dayCheckboxes.length === 0) {
  // If no days selected, automatically use today
  const today = new Date().getDay()
  const todayIndex = today === 0 ? 6 : today - 1 // Convert to Monday=0 format
  const todayDay = ['mon','tue','wed','thu','fri','sat','sun'][todayIndex]
  params.append(todayDay, 'true')
} else {
  // Use selected days
  dayCheckboxes.forEach(checkbox => {
    params.append(checkbox.value, 'true')
  })
}

fetch('/add_schedule?' + params.toString())
.then(r => r.text())
.then(msg => {
  console.log(msg)
  isFormActive = false;
  cancelScheduleForm()
  setTimeout(refresh, 500)
})
.catch(err => {
  console.error('Error:', err)
  alert('Failed to add schedule')
})
}

function editScheduleForm(scheduleId) {
isFormActive = true;
stopRefresh();

// Find the schedule data from the current display
fetch('/status')
.then(r => r.json())
.then(data => {
  const schedule = data.schedules.find(s => s.id === scheduleId)
  if (!schedule) {
    alert('Schedule not found')
    return
  }

  const container = document.getElementById('schedules_container')
  const form = document.createElement('div')
  form.className = 'schedule-form'
  form.style.cssText = 'background:#2a2a2a;margin:5px;padding:10px;border-radius:5px;border-left:4px solid #2196F3'

  form.innerHTML = `
    <h4>Edit Schedule</h4>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:10px">
      <div>
        <label>Start Time:</label><br>
        <input type="time" id="edit_start_time" value="${String(schedule.startHour).padStart(2,'0')}:${String(schedule.startMinute).padStart(2,'0')}" style="padding:5px">
      </div>
      <div>
        <label>End Time:</label><br>
        <input type="time" id="edit_end_time" value="${String(schedule.endHour).padStart(2,'0')}:${String(schedule.endMinute).padStart(2,'0')}" style="padding:5px">
      </div>
    </div>
    <div style="margin-top:10px">
      <label>Days:</label><br>
      <div style="display:grid;grid-template-columns:repeat(7,1fr);gap:5px">
        ${['MON','TUE','WED','THU','FRI','SAT','SUN'].map((day, i) => {
          const dayValue = day.toLowerCase()
          const isChecked = schedule.days[i]
          return `<label style="font-size:12px"><input type="checkbox" value="${dayValue}" name="day" ${isChecked ? 'checked' : ''}> ${day}</label>`
        }).join('')}
      </div>
    </div>
    <div style="margin-top:10px">
      <button onclick="updateSchedule('${scheduleId}')" style="padding:5px 10px;background:#2196F3">Update</button>
      <button onclick="cancelScheduleForm()" style="padding:5px 10px;background:#666">Cancel</button>
    </div>
  `

  container.appendChild(form)
  document.getElementById('add_schedule_btn').style.display = 'none'
})
}

function updateSchedule(scheduleId) {
  const startTime = document.getElementById('edit_start_time').value
  const endTime = document.getElementById('edit_end_time').value

  if (!startTime || !endTime) {
    alert('Please select both start and end times')
    return
  }

  const [startHour, startMinute] = startTime.split(':').map(Number)
  const [endHour, endMinute] = endTime.split(':').map(Number)

  const dayCheckboxes = document.querySelectorAll('input[name="day"]:checked')
  const params = new URLSearchParams()
  params.append('startHour', startHour)
  params.append('startMinute', startMinute)
  params.append('endHour', endHour)
  params.append('endMinute', endMinute)

  if (dayCheckboxes.length === 0) {
    // If no days selected, automatically use today
    const today = new Date().getDay()
    const todayIndex = today === 0 ? 6 : today - 1 // Convert to Monday=0 format
    const todayDay = ['mon','tue','wed','thu','fri','sat','sun'][todayIndex]
    params.append(todayDay, 'true')
  } else {
    // Use selected days
    dayCheckboxes.forEach(checkbox => {
      params.append(checkbox.value, 'true')
    })
  }

  // First remove the old schedule, then add the new one
  fetch('/remove_schedule?id=' + scheduleId)
  .then(r => r.text())
  .then(() => {
    return fetch('/add_schedule?' + params.toString())
  })
  .then(r => r.text())
  .then(msg => {
    console.log('Schedule updated:', msg)
    isFormActive = false;
    cancelScheduleForm()
    setTimeout(refresh, 500)
  })
  .catch(err => {
    console.error('Error:', err)
    alert('Failed to update schedule')
  })
}

function removeSchedule(id) {
if (confirm('Remove this schedule?')) {
  fetch('/remove_schedule?id=' + id)
  .then(r => r.text())
  .then(msg => {
    console.log(msg)
    setTimeout(refresh, 500)
  })
}
}

function toggleSchedule(id, enabled) {
fetch('/toggle_schedule?id=' + id + '&enabled=' + enabled)
.then(r => r.text())
.then(msg => {
  console.log(msg)
  setTimeout(refresh, 500)
})
.catch(err => {
  console.error('Error:', err)
  alert('Failed to toggle schedule')
})
}

function setTimer() {
let hours = parseInt(document.getElementById("timer_hours").value) || 0;
let minutes = parseInt(document.getElementById("timer_minutes").value) || 0;

if (hours >= 0 && minutes >= 0 && minutes <= 60) {
  let totalMinutes = hours * 60 + minutes;
  if (totalMinutes >= 1) {
    // Always send both parameters, even if zero
    fetch(`/set_timer?hours=${hours}&minutes=${minutes}`)
    .then(r => r.text())
    .then(msg => {
      console.log(msg);
      setTimeout(refresh, 500);
    })
  } else {
    alert('Total time must be at least 1 minute');
  }
} else {
  alert('Invalid input: Hours (0+), Minutes (0-60)');
}
}

function cancelTimer() {
fetch('/cancel_timer')
.then(r => r.text())
.then(msg => {
console.log(msg)
setTimeout(refresh, 500)
})
}

// Start the refresh cycle
startRefresh();
refresh()

</script>

</body>
</html>

)rawliteral";

  server.send(200,"text/html",page);
}

void setupAP()
{
  WiFi.softAP("Cooler-Setup");
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP Mode IP address: ");
  Serial.println(IP);

  server.on("/",[IP](){

  server.send(200,"text/html",
  "<h2>🌡️ Cooler Controller Setup</h2>"
  "<h3>Option 1: Connect to WiFi</h3>"
  "<form action='/save'>"
  "SSID:<input name='s' placeholder='Enter WiFi name'><br>"
  "Password:<input name='p' placeholder='Enter WiFi password'><br>"
  "<button>Connect to WiFi</button></form>"
  "<hr>"
  "<h3>Option 2: Use in AP Mode</h3>"
  "<p>Current IP: " + IP.toString() + "</p>"
  "<p><a href='/dashboard'>Access Cooler Control</a> (No WiFi needed)</p>"
  "<p><small>Note: In AP mode, time/schedules may not work accurately without internet.</small></p>");

  });

  server.on("/dashboard", handleDashboard);

  server.on("/save",[](){

    String s=server.arg("s");
    String p=server.arg("p");

    saveWiFi(s,p);

    server.send(200,"text/html","Saved. Restart device.");

  });

  server.begin();
}

void setup()
{
  Serial.begin(115200);

  pinMode(RELAY_PIN,OUTPUT);
  setRelay(false);

  loadWiFi();
  
  if(ssid=="")
  {
    setupAP();
    return;
  }

  WiFi.begin(ssid.c_str(),password.c_str());

  // Try to connect for 15 seconds, then fallback to AP mode
  Serial.println("Connecting to WiFi...");
  int timeout = 0;
  while(WiFi.status()!=WL_CONNECTED && timeout < 30) {  // 30 * 500ms = 15 seconds
    delay(500);
    timeout++;
    Serial.print(".");
  }
  
  if(WiFi.status()!=WL_CONNECTED) {
    Serial.println("\nWiFi connection failed. Starting AP mode...");
    setupAP();
    return;
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Load schedules only after successful WiFi connection
  loadSchedules();

  if(MDNS.begin("cooler"))
  Serial.println("http://cooler.local");

  configTime(19800,0,"pool.ntp.org");

  server.on("/",handleDashboard);

  server.on("/on",[](){
  setRelay(true);
  server.send(200,"text/plain","ON");
  });

  server.on("/off",[](){
  setRelay(false);
  server.send(200,"text/plain","OFF");
  });

  server.on("/add_schedule", handleAddSchedule);
  server.on("/remove_schedule", handleRemoveSchedule);
  server.on("/toggle_schedule", handleToggleSchedule);
  server.on("/timer",handleTimerStatus);
  server.on("/set_timer",handleSetTimer);
  server.on("/cancel_timer",handleCancelTimer);
  server.on("/status",handleStatus);

  server.on("/reset",handleReset);

  server.begin();
}

void loop()
{
  server.handleClient();

  static unsigned long last=0;

  if(millis()-last>30000)
  {
    checkSchedule();
    last=millis();
  }
  
  // Check timer more frequently for accurate countdown
  static unsigned long lastTimerCheck=0;
  if(millis()-lastTimerCheck>1000) {
    checkTimer();
    lastTimerCheck=millis();
  }
}
