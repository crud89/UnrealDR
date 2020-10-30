# UnrealDR

Diminished Reality toolkit for UE4.

## What is Diminished Reality?

Diminished Reality is a third category of virtual environments. Where Virtual Reality (VR) tries to simulate a whole environment and Augmented Reality tries to add to a real environment, Diminished Reality wants to alter real environments.

## Installation

First, download or clone the repository and move the contents into `MyProject/PlugIns/UnrealDR/`. Alternatively, you can add a [git submodule](https://git-scm.com/book/de/v2/Git-Tools-Submodule) to your project, to automatically restore the latest version on every checkout:

```ps
git submodule add https://github.com/Aschratt/UnrealDR.git Plugins/UnrealDR/
```

Next, open the `UnrealDR.uplugin` file and modify the `EngineVersion` property to fit your current version of Unreal Engine. When you open your project in Unreal Editor, the Plug-In should automatically activate and compile. If you are using a Visual Studio version, make sure to re-create the Solution afterwards.

## Prerequisites

The plug-in is designed to work with Virtual Reality HMDs that implement an OpenVR driver. Currently it is only tested on a HTC Vive Pro. UnrealDR does not use any dependencies that are not already included in Unreal Engine.

## Documentation

The documentation can be found in the [project wiki](https://github.com/Aschratt/UnrealDR/wiki).
