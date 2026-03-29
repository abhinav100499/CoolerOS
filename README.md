# CoolerOS v2.1
**ESP32 Cooler Controller** - A smart cooler controller with intelligent priority system, scheduling, timer, and web interface.

## 🆕 v2.1 Latest Updates

### **Enhanced Priority Logic**
- **First-Come, First-Served**: Timer vs Manual ON respects whichever was activated first
- **Manual Override**: Manual ON stays active until manual OFF (no time limits)
- **Smart Timer Integration**: Timer respects manual control timing
- **Schedule Protection**: Schedules completely ignored during manual override

### **Bug Fixes**
- ✅ **Fixed 30-second cycle issue**: Manual control no longer overridden by schedule checks
- ✅ **Fixed timer conflicts**: Timer expiry now respects manual override timing
- ✅ **Fixed schedule interference**: Manual control has absolute priority

## Features

- **🎯 Manual Control**: ON/OFF buttons with intelligent override system
- **⏰ Timer System**: Unlimited hours with precise timing (0-60 minutes)
- **📅 Advanced Scheduling**: 
  - Max 10 schedules
  - Multiselect days (MON-SUN)
  - Start/end time with overnight support
  - Enable/disable functionality
  - Edit/delete individual schedules
  - Visual indication for schedules not active today
- **🏆 Intelligent Priority System**: Manual OFF > (Timer vs Manual ON) > Schedule
- **🎨 Color-coded UI**: Green (today), Blue (other days), Gray (disabled)
- **📶 Smart WiFi**: 15-second timeout with AP fallback for portable deployment
- **⏱️ Accurate Runtime**: Shows exact run time without resets
- **🔒 Manual Override Protection**: Manual control respected until manual OFF

## Hardware

- ESP32
- Relay Module (5V)
- Button (optional for manual control)

## UI
![coolerOS UI](./image/coolerOS.jpeg)

## Setup

1. Install ESP32 board in Arduino IDE
2. Connect relay to pin 5
3. Upload code
4. Connect to WiFi via AP mode or auto-connect
5. Access web interface at ESP32 IP or http://cooler.local

## Web Interface

- **Dashboard**: Real-time status and controls
- **Timer**: Set duration with hours/minutes (unlimited hours)
- **Schedules**: Create, edit, delete schedules
- **Auto-refresh**: Updates every 3 seconds with pause during form input

## API Endpoints

- `/on` - Turn cooler ON
- `/off` - Turn cooler OFF
- `/set_timer` - Set timer (hours, minutes)
- `/cancel_timer` - Cancel active timer
- `/add_schedule` - Add new schedule
- `/remove_schedule` - Remove schedule
- `/toggle_schedule` - Enable/disable schedule
- `/status` - Get system status

## Schedule Format

```json
{
  "id": "unique_id",
  "startHour": 9,
  "startMinute": 0,
  "endHour": 17,
  "endMinute": 0,
  "days": [true, true, true, true, true, false, false],
  "enabled": true
}
```

## 🧠 Intelligent Priority Logic

### **Priority Hierarchy**
1. **🔴 Manual OFF** - Absolute priority, cancels timer and override
2. **🟡 Timer vs Manual ON** - First-come, first-served:
   - If Manual ON before Timer → Manual wins
   - If Timer before Manual ON → Timer wins
3. **🟢 Schedules** - Only active when no manual override or timer

### **Behavioral Examples**
- **Manual ON → Timer**: Manual wins (stays ON until timer expires)
- **Timer → Manual ON**: Manual wins (stays ON until manual OFF)
- **Timer expires → Manual ON active**: Timer cancels, cooler stays ON
- **Manual ON → Schedule starts**: Schedule ignored, cooler stays ON
- **Manual OFF**: Always wins, cancels everything immediately

## 🔧 Configuration

- **Max Schedules**: 10
- **Timer Max**: Unlimited hours
- **Auto-refresh**: 3 seconds
- **WiFi**: Auto-connect with 15-second timeout + AP fallback
- **Manual Override**: Until manual OFF (no time limit)
- **Runtime Tracking**: Accurate to the second
- **Schedule Check**: Every 30 seconds (respects manual override)
- **Timer Check**: Every 1 second (precise countdown)

## Smart WiFi Features

- **Development Mode**: Connects to saved WiFi automatically
- **Production Mode**: 15-second timeout → AP fallback if WiFi unavailable
- **Portable Setup**: Creates "Cooler-Setup" hotspot for configuration
- **Dual Access**: Both WiFi setup and direct AP mode control
- **No Stuck Device**: Always accessible via hotspot

## 🛠️ Troubleshooting

### **v2.1 Common Issues**
- **❌ Manual control turning off automatically**: 
  - ✅ Fixed in v2.1 - Manual override prevents schedule interference
- **⏰ Timer not respecting manual control**: 
  - ✅ Fixed in v2.1 - First-come, first-served logic implemented
- **📅 Schedules overriding manual ON**: 
  - ✅ Fixed in v2.1 - Manual override completely blocks schedule checks

### **General Troubleshooting**
- **🔌 Hardware Issues**: 
  - Check relay connections to pin 5
  - Verify power supply stability
  - Test relay module independently
- **📶 Network Issues**: 
  - Try AP mode if WiFi unavailable
  - Check Serial Monitor for IP address
  - Verify WiFi credentials
  - Access via http://cooler.local if mDNS working
- **⚡ Performance Issues**: 
  - Reset ESP32 if unresponsive
  - Check Serial Monitor for error messages
  - Verify sufficient power supply
- **🧹 Memory Issues**: 
  - Reset device if web interface becomes slow
  - Check schedule count (max 10)
  - Clear schedules if needed via reset endpoint

### **Debug Information**
- **Schedule Check Interval**: 30 seconds
- **Timer Check Interval**: 1 second  
- **Manual Override Status**: Persistent until manual OFF
- **Runtime Accuracy**: Millisecond precision
- **Priority Status**: Manual OFF > (Timer vs Manual ON) > Schedule
