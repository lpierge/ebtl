# ebtl

## Overview
**ebtl**, aka **e**_(xplorer)_**b**_(ackground)_**t**_(ool)_**l**_(oader)_, is a command line utility used to install, register, unregister and reload the  [ExplorerBgToolRe](https://github.com/lpierge/ExplorerBgToolRe) DLL and to restart the Windows Explorer as needed.

## Features
This utility (also available in binary form in the **Installer** directory of the **ExplorerBgToolRe** repository) supports the following command-line syntax:

`ebtl [option] [DLL pathname/installation directory]`

If launched with no arguments, the program displays the current registration status of the DLL.

Valid options/arguments are:

`-h`  Show this help message.

`-i [directory]`  Install the DLL in the default/specified directory.

`-r <DLL pathname>`  Register the specified DLL.

`-u [DLL pathname]`  Unregister the (specified) DLL.

`-d`  Force the system to reload the DLL.

`-e`  Force the system to restart the Explorer.

(_note: square brackets [ ] indicate optional parameters, while angle brackets < > indicate mandatory parameters)_

## Project dependencies
Source files that are not part of the core **ebtl** project but are used by it as external dependencies can be found in the **Include** and **Library** repositories. The **ExplorerBgToolRe** DLL project is also obviously required bacause the compiled DLL is included into the **ebtl** executable as a resource, to be extracted during the installation process:

* [ebtl](https://github.com/lpierge/ebtl) — this project
* [ExplorerBgToolRe](https://github.com/lpierge/ExplorerBgToolRe) — the ExplorerBgToolRe DLL
* [Include](https://github.com/lpierge/Include) — Shared header (.h) files
* [Library](https://github.com/lpierge/Library) — Shared source (.c/.cpp) files

## Implementation notes
**Important note on projects structure:**

The Visual Studio project for **ebtl** is hardcoded to search for dependencies using absolute paths starting from the root of a virtual L: drive. The expected directory structure is as follows:

```text
L:\
  |-- ebtl\
  |-- ExplorerBgToolRe\
  |-- Include\
  |-- Library\
```
Instead of changing the Visual Studio settings in the project file, I recommend mapping a local folder to a virtual L: drive with the Windows SUBST command:
- Create a directory on your local drive, for example `C:\DEV`.
- Download and extract all the repositories inside that directory.
- Open the Windows Command Prompt (press `Win + R` to open the Run dialog, type `cmd.exe` and press `Enter`) and from the Console run the following command: `SUBST L: C:\DEV`

## Windows binaries and Installer

The **Installer** directory in the **ExplorerBgToolRe** repository contains the DLL loader (**ebtl.exe**), already compiled for Windows and provided in a zipped archive.

After downloading and unzipping the file, open a Command Prompt (press `Win + R`, type `cmd.exe` and press Enter), navigate to the folder where you extracted the `ebtl.exe` file, close all the running programs and run the following command:

`ebtl -i`

This will install and register the DLL in the default folder `C:\ExplorerBgToolRe`. The installation process will also create two subdirectories (`Image` and `Chibi`), containing sample images, and a `config.ini` configuration file. Make sure to read the comments inside the `config.ini` carefully before modifying it.

P.S. If you are into the manga/anime genre and have run out of sources to download images from, or if you are tired of manually saving them one by one, [Calimero](https://github.com/lpierge/Calimero/tree/main/wchg/res#readme) (a Windows desktop utility available [here](https://github.com/lpierge/Calimero/tree/main/Installer)) includes a module to download and manage images directly from sites like **Picsum**, **Pexels**, **Reddit**, and **Danbooru**. Reddit and Danbooru are excellent sources for this kind of artworks, just keep in mind that most content on Danbooru is NSFW.

## Screenshots
_**This PC and Documents folders**_

![Calimero](https://i.ibb.co/x8SkYLXM/screenshot01.jpg)

_**Standard (foreground) and Special (background) folders**_

![Calimero](https://i.ibb.co/9H1s2vpG/screenshot02.jpg)

_**Standard folders**_

![Calimero](https://i.ibb.co/F4GfFkv5/screenshot03.jpg)

Luca P.
