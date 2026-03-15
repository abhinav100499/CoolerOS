# CoolerOS
**ESP32 Cooler Controller** A smart cooler controller with scheduling, timer, and web interface.

## Features

- **Manual Control**: ON/OFF buttons
- **Timer System**: Hours/minutes with priority over schedules
- **Advanced Scheduling**: 
  - Max 10 schedules
  - Multiselect days (MON-SUN)
  - Start/end time with overnight support
  - Enable/disable functionality
  - Edit/delete individual schedules
  - Visual day indication
- **Priority System**: Manual > Timer > Schedule
- **Color-coded UI**: Green (today), Blue (other days), Gray (disabled)

## Hardware

- ESP32
- Relay Module (5V)
- Button (optional for manual control)

## Setup

1. Install ESP32 board in Arduino IDE
2. Connect relay to pin 5
3. Upload code
4. Connect to WiFi via AP mode
5. Access web interface at ESP32 IP

## Web Interface

- **Dashboard**: Real-time status and controls
- **Timer**: Set duration with hours/minutes
- **Schedules**: Create, edit, delete schedules
- **Auto-refresh**: Updates every 3 seconds

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

## Priority Logic

1. **Manual ON/OFF** - Highest priority
2. **Timer Active** - Pauses schedules
3. **Schedules** - Time and day based

## Configuration

- **Max Schedules**: 10
- **Timer Max**: 12 hours
- **Auto-refresh**: 3 seconds
- **WiFi**: Auto-connect with fallback AP mode

## Troubleshooting

- Check Serial Monitor for IP address
- Verify WiFi credentials
- Ensure relay connections
- Reset ESP32 if needed
