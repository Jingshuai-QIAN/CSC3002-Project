# CSC3002-Project


## Compile and run

### Compile for the first time

### Option 1: Windows (Batch Script)
If you are using the provided `build.bat` script with MinGW/G++:

1. Open `navigation/codes/build.bat` and ensure the compiler/SFML paths match your local installation. (VERY IMPORTANT!) Also note that the json library path refers to the nlohmann library, which might require download.
2. Run the script:
   ```cmd
   cd navigation/codes
   build.bat
The compiling process may take about 1 min.
   
### Option 2: Bash
```bash
# Go to the program's root folder
cd path_to_navigation_folder
# Compile the program (for the first time)
make
# Run the program
./codes/main.exe
```

### Re-compile

```bash
# Go to the program's root folder
cd path_to_navigation_folder
# Remove all generated build artifacts
make clean
# Re-compile the program
make VERBOSE=1
# Run the program
./codes/main.exe
```


## Structure

```plain text
navigation/
├── README.md
├── Makefile
├── codes/
│   ├── build.bat                # Windows Build Script
│   ├── main.cpp
│   ├── App.h
│   ├── App.cpp
│   ├── Character/               # Player character module
│   │   ├── Character.h
│   │   ├── Character.cpp
│   │   ├── CharacterConfig.h
│   │   └── CharacterConfig.cpp
│   ├── Manager/                 # Game Systems Manager
│   │   ├── TimeManager.h        # Day/Night Cycle Logic
│   │   ├── TimeManager.cpp
│   │   └── TaskManager.h        # Quest & Energy System
│   ├── Config/                  # Configuration manager
│   │   ├── ConfigManager.h
│   │   └── ConfigManager.cpp
│   ├── Input/                   # Input handling
│   │   ├── InputManager.h
│   │   └── InputManager.cpp
│   ├── MapLoader/               # Map loading and TMJ parsing
│   │   ├── MapLoader.h
│   │   ├── MapLoader.cpp
│   │   ├── MapObjects.h
│   │   ├── TileSetManager.h
│   │   ├── TileSetManager.cpp
│   │   ├── TileLayer.h
│   │   ├── TileLayer.cpp
│   │   ├── TMJMap.cpp
│   │   └── TMJMap.h
│   ├── Renderer/                # Rendering subsystem
│   │   ├── Renderer.h
│   │   ├── Renderer.cpp
│   │   ├── TextRenderer.h
│   │   └── TextRenderer.cpp
│   └── Utils/                   # Utility helpers
│   │   ├── Logger.h
│   │   ├── FileUtils.h
│   │   └── StringUtils.h
│   └──  Login/                   # Login screen
│   │   ├── LoginScreen.cpp
│   │   ├── LoginScreen.h
│   │   ├── LoginScreen.o
│   │   ├── MapGuideScreen.cpp
│   │   └── MapGuideScreen.h
├── config/                      # Configuration files
│   ├── app_config.json
│   ├── render_config.json
│   └── character_config.json
├── fonts/
│   └── arial.ttf
├── maps/
│   ├── bus.tmj
│   ├── canteen.tmj
│   ├── LG_campus_map.tmj
│   ├── library.tmj
│   ├── lower_campus_map.tmj
│   ├── shaw.tmj
│   ├── teaching_building.tmj
│   └── spawns.json
└── tiles/
│   ├── exterior.png
│   ├── F_01.png
│   ├── F_05.png
│   ├── M_10.png
│   ├── free_overview.png
│   ├── Interior.png
│   ├── Modern_City.png
│   ├── roguelikeIndoor_transparent.png
│   ├── Small_Battle.png
│   ├── tile_modern.png
│   └── tilemap_packed.png
└── assets/
    ├── keyboard-&-mouse_sheet_default.png
    ├── ui_map_guide.png
    ├── uipack_rpg_sheet.png
    └── panelInset_brown.png
```
