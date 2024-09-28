# Aurora Pak Loader

**Beta Version**

Plugin to load Pak Plugin at runtime and activate their GameFeatures.  
Was not tested on worlds with WorldPartition enabled.

# Packaging 

## Project Settings to be adjusted:
- [ ] Untick `Use IO Store`, `Use Zen Store` and `Share Material Shader Code` in `Project Settings > Project > Packaging`
- [ ] Tick `Generate No Chunks` in `Project Settings > Project > Packaging`

To test in the editor, we also need to do the following in the testing project:
- [ ] Tick `Allow Cooked Content in the Editor` in `Project Settings > Engine > Cooker`
- [ ] Add the line `s.AllowUnversionedContentInEditor=1` in the `Config\DefaultEngine.ini` file, under the category `[/Script/UnrealEd.CookerSettings]`. This should look like this:
```
[/Script/UnrealEd.CookerSettings]
cook.AllowCookedDataInEditorBuilds=True
s.AllowUnversionedContentInEditor=1
```

## Package Project

In the content browser, right click on `All` or `Content` and under `Aurora`, select `Create a Packaged Project`.

## Package DLC Pak

In the content browser, right click on any content folder, or select the items to packaged and right click and under `Aurora`, select `Create a Content DLC Pak`.

# Common issues

It is currently not possible to Mount a DLC Pak which would have the same Mount Point as a plugin.
If you are exporting the plugin `Stage_01` as DLC, make sure to remove it from the `Plugins` folder before trying to load the DLC of the same name.
Similarly, make sure the plugin does not end up cooked as part of the Base Game Package.

# Legacy Packaging process, if newer version does not work

To work as expected, there are a few settings that need to be adjusted manually (for now at least) on both the packaging project and the playing project.  
It is important to note that to package individual plugins, we are leveraging the DLC feature of UE.  
This is not made to package individual plugins but we can use it to do so.  

Packaging a DLC needs a **named Base Game package**, which should contain everything that will be common to every plugin.  
When a DLC is packaged, everything that was not in this **named Base Game package** will be packaged.  
To make this work for us, the goal is to package the project without the plugin to use this as the base of the DLC, then add the plugin and package the DLC.

*Note:  
The above describe my current understanding the packaging process in UE, it might not be fully accurate*

## Setting up the Project Settings and Launch Profiles

The packaging project is the project in which the GameFeature plugin will be created and packaged from.  
We need to package a base version of the project that does not contain any of the plugin information.  
We will then package the plugin as a DLC, which will contain **all files not present in the base version of the project**.


### Project Settings to be adjusted:
- [ ] Untick `Use IO Store`, `Use Zen Store` and `Share Material Shader Code` in `Project Settings > Project > Packaging`
- [ ] Tick `Generate No Chunks` in `Project Settings > Project > Packaging`

To test in the editor, we also need to do the following in the testing project:
- [ ] Tick `Allow Cooked Content in the Editor` in `Project Settings > Engine > Cooker`
- [ ] Add the line `s.AllowUnversionedContentInEditor=1` in the `Config\DefaultEngine.ini` file, under the category `[/Script/UnrealEd.CookerSettings]`. This should look like this:
```
[/Script/UnrealEd.CookerSettings]
cook.AllowCookedDataInEditorBuilds=True
s.AllowUnversionedContentInEditor=1
```


### Custom Base Game Launch Profiles:
To package the plugin, we need to create two Launch Profiles, one to package the project (that will need to be run first), and one to package the actual plugin.
In the main UE Toolbar (next to he Play button), open the `Project Launcher...` under the `Platforms` dropdown menu:
- [ ] In the `Custom Launch Profiles`, add a new `Custom Profile` for the **Base Game Packaging** and name it appropriately (ex: `Base Game`)
  - [ ] In the `Project` section, change the `Project` to `Any Project` (so it compiles the currently opened UE project)
  - [ ] You can adjust the Build Configuration as required under `Build` (tested with Development only)
  - [ ] Under `Cook`, change the cook to `By the book`
    - choose your platform (only tested on `Windows`) and Culture, and leave the Maps unticked
    - Under `Release / DLC / Patching`, tick `Create a release version of the game for distribution` and give a name for the release (like `1.0`). This name will be needed below
      - In `Advanced settings`, make sure that only `Compress Content`, `Save packages without versions` and `Store all content in a single file (UnrealPak)` are ticked
  - [ ] Change `Package` to `Package and store locally` and leave everything unticked
  - [ ] Leave `Archive` unticked and `Deploy` to `Do not Deploy`

### Custom Plugin Launch Profiles:
The setup is the same as the Base Game setup apart from the `Cook` settings:
- [ ] In the `Custom Launch Profiles`, add a new `Custom Profile` for the **Plugin Packaging** and name it appropriately (ex: `Stage_01 Plugin`)
  - [ ] *In the `Project` section, change the `Project` to `Any Project` (so it compiles the currently opened UE project)*
  - [ ] *You can adjust the Build Configuration as required under `Build`*
  - [ ] *Under `Cook`, change the cook to `By the book`*
    - *choose your platform (only tested on `Windows`) and Culture, and leave the Maps unticked*
    - Under `Release / DLC / Patching`:
      - make sure `Create a release version of the game for distribution` is unticked, 
      - make sure `Name of the new release to create` has no name
      - fill `Release version this is based on` with the **name used for the release of the Base Game in the Base Game launch profile** (like `1.0`)
      - tick `Build DLC`, and under `Name of the DLC to build`, write the **Plugin Name** (ex: `Stage_01`) (This does not have to be the same, but some files were be missed if not matching)
      - make sure `Include Engine Content` is unticked. We want to use the Engine content from the project itself, but we will need to make sure it is packaged.
      - *In `Advanced settings`, make sure that only `Compress Content`, `Save packages without versions` and `Store all content in a single file (UnrealPak)` are ticked*
  - [ ] *Change `Package` to `Package and store locally` and leave everything unticked*
  - [ ] *Leave `Archive` unticked and `Deploy` to `Do not Deploy`*
  - [ ] Run this launch profile, it should be successful. The packaged plugin content should be (by default) in the plugin folder `\Saved\StagedBuilds\Windows\ForPackage\Plugins\GameFeatures\<plugin-name>\`. You can copy this folder (containing the `uplugin` and `Content` subfolder) somewhere else (by default in the `<ProjectFolder>/DLC`. **We will need to give the path of this folder (the path to `\<plugin-name>\`) to the AuroraPakLoader plugin to load this GameFeature**

### Package the plugin

- [ ] Before Packaging the plugin, we need to package the project.  
We also need to make sure that the plugin files are not packaged with the project, otherwise they will not be part of the plugin .pak file.
The current most reliable way is the following:
  - Deactivate the plugin to package in `Edit > Plugins`. No need to restart the editor.
  - Package the base game in `Platforms > Project Launcher > Base Game Profile > Run`
  - When done, ensure the plugin files are absent in the Game .pak file with the UnrealPak.exe tool:
```
"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealPak.exe" "D:\<path>\<ProjectName>\Plugins\GameFeatures\<plugin-name>\Saved\StagedBuilds\Windows\<ProjectName>\Plugins\GameFeatures\<plugin-name>\Content\Paks\Windows\<plugin-name><ProjectName>-Windows.pak" -list
```
- [ ] We are now ready to package the plugin.
  - Re-activate the plugin to package in `Edit > Plugins`. No need to restart the editor
  - Package the DLC plugin in `Platforms > Project Launcher > Plugin Profile > Run`
  - When done, ensure the plugin files are present in the Plugin .pak file with the UnrealPak.exe tool:
```
"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealPak.exe" "D:\<path>\<ProjectName>\Plugins\GameFeatures\<plugin-name>\Saved\StagedBuilds\Windows\<ProjectName>\Plugins\GameFeatures\<plugin-name>\Content\Paks\Windows\<plugin-name><ProjectName>-Windows.pak" -list
```

## Test the plugin

By default, the AuroraPakLoader plugin will Mount all the Pak Plugins located in the `<ProjectFolder>/DLC` folder.  
If Play the level, the Output log should give you information about the mounting and loading process. Also, you should see the content of the pak file in the `Content` panel as a new Mounting Point was added in the `Source` folder.  
If you stop PIE, the content is will be unmounted.  
**If you unmount a currently loaded map, this will currently crash the game/editor**

To manually load a map, you could call the BP function `Open Level (by Name)` and give the path to the level, like `/Stage_01/Maps/Stage_01_Map`.  
Start playing and the map should get loaded.

By default, GameFeatures are not AutoActivated (they could be if preferred), but you can use `GF Pak Loader Subsystem` to retrieve all the Pak Plugins with a Status of `Mounted` and call the BP function `Activate Game Feature`


# Additional information

- As we need to package the plugin, I expect that the UE version of the project used to package needs to match the UE version of the project that will use the packaged content. To be tested.
- Currently, we code is setup in a way where we need to give a path to the packaged plugin directory containing the `.uplugin` file:
```
  // We expect a packaged plugin directory to have these files and folders:
  // plugin-name/
  //		plugin-name.uplugin
  //		Content/Paks/<platform>/plugin-name-and-additional-things.pak
```
We need the `.uplugin` for the `GameFeatureSubsystem` to work, and this also allow us to retrieve the name of the plugin and description (and all other information) which we could pull for the UI. 
