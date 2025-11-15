[English](README.md) | [Português (BR)](README.pt-BR.md)

# OBS GameDetector – OBS Studio Plugin

**GameDetector** is a plugin for **OBS Studio** that automatically detects games running on the computer and allows you to trigger automations, switch scenes, send messages to Twitch chat, and much more.

It makes streamers’ lives easier by identifying the game currently being played and executing configurable actions automatically.

## Features

### Automatic Game Detection

* Continuous system process monitoring every 5s directly from memory.
* Support for **manual** and **automatic** game lists.
* Detection of game start, end, and active game changes.
* Asynchronous processing to avoid freezing OBS.

### Twitch Chat Integration

* Automatic message sending when a game starts.
* Quick token generation via browser.
* Secure token storage inside OBS.
* Minimal required permission: `user:write:chat`.

## How to Use

### 1. Installation

Download the installer from the GitHub releases section:

[→ Click here to view the releases](https://github.com/FabioZumbi12/OBSGameDetector/releases)

Follow the instructions, or download the zip file, extract it, and copy the `data` and `obs-plugins` folders to:

```
C:\Program Files\obs-studio\
```

Restart OBS.

### 2. Opening the configuration panel

In OBS:

**Menu → Tools → Game Detector Settings**

Here you can:

* Edit the game list
* Scan for games in known Steam and Epic folders (does not scan all folders on the PC)
* Configure your Twitch token
* Save changes

### 3. Configure automatic Twitch messages

1. Click **Generate Token**
2. Your browser will open requesting minimal permissions
3. Copy the `ACCESS TOKEN` and `CLIENT ID` and paste them into the configuration fields

## Compatibility

* OBS Studio **29+**
* Windows **10/11**
* PowerShell
* Built with:
  * OBS SDK
  * Qt 6
  * C++17

## Contributing

Pull requests and suggestions are welcome!

You can:

* Report bugs
* Suggest new features
* Improve documentation
* Create tests