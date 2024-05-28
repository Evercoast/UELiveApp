# Content Contribution Guide

## Setup the dev environment
Follow the setup page on https://support.evercoast.com/docs/setup Since this project is based on Unreal 5.3, you'll need Visual Studio 2022 on Windows installed.

For large assets that's typical for level designing, git-lfs is also needed for anything larger than 100MB. See https://git-lfs.com/ to install Git LFS environment.

## Directory Structure
The project follows a typical Unreal project structure, where `Content` is the directory for all assets that being recognised by Unreal. Notebly, there are a few special sub-directories to be aware of:

  - BluePrints: where all custom made Blueprint classes and Blueprint UI widgets are stored.
  - Maps: where all the selectable maps are stored
  - PrerecordedVolcaps: Since this project is mainly focused on realtime volcap, this folder is mainly for storing canned ecm/mp4 files that used for prototyping
  - EvercoastVolcapSource: this is a special directory that the EvercoastPlayback Plugin code will utilise for packaging a standalone build. Do not store assets in this directory

Other than those above directories, it's free to put anything in the `Content` directory.

## After making a new map
So you have taken time to import assets and made a map so that realtime volcap actor can live in. In order for the app to display that map, there are a couple of things to do:

- Open BluePrints/BP_Widget_MenuScene in Unreal, then in `Designer` tab, add buttons and texts for the new map
- In the same Blueprint, go to `Graph` tab, follow the same pattern to add event handling for menu items. This will allow the app to open up the map upon user clicking.
- In the map, place an actor of type `EvercoastRealtimeVolcapActor`. This is the main actor that gets streamed from server and displayed.
- Also, in `Content Browser`, drag `Content/UE4_Mannequin_Mobile/Mesh/SK_Mannequin_Mobile` into the map and sub object it under the `EvercoastRealtimeVolcapActor` object just placed.
- Place `BP_UI_Intro` object in the map. This is a Blueprint class that will configure the current map and connects the UI to use the proper realtime volcap actor. 
- In the Details Panel of this `BP_UI_Intro` object, specify the `Realtime Volcap Actor`, `Placeholdrer Mannquin` and `Controlled Lights` if you wish the user can control the overal dynamic lighting intensity in the map
 

After that, the new map should be able to selectable and working in the app.

Guide in Google Docs: https://docs.google.com/document/d/1pKGnoXrl6S2YvfYq-IHbvDV_BMcnKnCITVWOB3N4KIA/edit?usp=sharing