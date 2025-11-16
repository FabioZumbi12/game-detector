[English](README.md) | [Português (BR)](README.pt-BR.md) 

# 🎮 Game Detector OBS Plugin
Plugin to detect installed games and integrate with Twitch · OBS Studio Support

---

## 📘 About Game Detector OBS Plugin

OBS GameDetector is a plugin for OBS Studio that automatically identifies games installed on your PC (Steam and Epic Games), allowing:

- Automatic game selection  
- Twitch integration (Client ID + Access Token)  
- Editing and correction of detected game names and executables  
- Automatic metadata creation  
- User-friendly interface inside OBS  

The focus is speed, accurate detection, and zero performance impact.

---

## 📥 Installation

After downloading the installer or ZIP file:

### **Installer Setup (recommended)**
1. Download **GameDetector-Setup.exe** from the [Releases](../../releases) page.
2. Run the installer.
3. Open OBS and confirm the plugin appears under **Tools → Settings for Game Detector**.

### **Manual ZIP Installation**
1. Extract the ZIP.
2. Copy:
   - `obs-plugins/64bit/game-detector.dll` → into the OBS plugins folder  
   - `data/obs-plugins/game-detector/` → into the OBS data folder  
3. Restart OBS.

---

## 🔧 Twitch Configuration

The plugin requires two mandatory fields for Twitch integration:

- **Client ID**
- **Access Token**

### How to fill them:

1. Open OBS.
2. Go to **Tools → GameDetector**.
3. Inside the settings panel, click the **Generate Token** button.
4. You will be redirected to:

   👉 https://twitchtokengenerator.com

5. On the website, generate your token normally.
6. Copy **exactly these two fields**:
   - **ACCESS TOKEN**
   - **CLIENT ID**
7. Paste them inside the plugin fields:
   - **Client ID**
   - **Access Token**
8. Click **Save**.

⚠️ No Twitch password is requested or used.  
⚠️ Only the two fields above are required.

---

## 🎮 Detected Games Table

After scanning, the plugin displays a table with all detected games.

Detection is fast because the plugin **does not scan your entire PC**, only:

- ✔️ Steam Library folders  
- ✔️ Default Epic Games directories  

This prevents slowdowns, false positives, and unnecessary file crawling.

---

## ✏️ Editing Detected Games

The table allows editing:

### ✔️ Game name  
Useful when the detected name doesn’t match the desired one.

### ✔️ Executable name (.exe)  
Useful when a game has multiple executables or the detected file is not the main launcher.

### ✔️ Full path  
Only for manual adjustments if needed.

All changes are saved automatically.

---

## 🔄 Re-scan Games

You can run the scan again at any time:

📌 Click the **Re-scan** button inside the plugin window.

---

## 🖼️ Screenshots (placeholders)

> Replace the images below with real screenshots.

### Main window:
![main-ui](./screenshots/main.png)

### Game detection:
![games-list](./screenshots/games.png)

### Settings:
![settings](./screenshots/settings.png)

---

## 🧩 Compatibility

| Feature                | Support |
|------------------------|---------|
| OBS Studio             | ✔️ 29+  |
| Windows                | ✔️ 10/11 64-bit |
| Steam Games            | ✔️ |
| Epic Games             | ✔️ |
| Other launchers        | ❌ (planned for future) |

---

## 🛠️ Technologies Used

- C++  
- libobs  
- Qt6  
- OBS Frontend API  
- Twitch API  
- Inno Setup  

---

## 🤝 Credits

Developed by **Fábio F. Magalhães (FabioZumbi12)**.  
Contributions and PRs are welcome!

---

## 📄 License

This project is distributed under the **MIT** license.

---

## ⭐ Support the Project

If the plugin helped you, consider leaving a ⭐ on GitHub!
