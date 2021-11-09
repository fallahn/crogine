VGA Golf Mod Kit
----------------

This folder contains Blender files and tools for creating custom balls and golf courses for VGA Golf.

See http://fallahn.itch.io/vga-golf for more details.


ball_format.md - description of the *.ball files used to load custom balls. Written in markdown format.
ball_template.blend - ball template file for Blender
collision_colours.ase - A Photoshop palette which contains colours for terrain collision. Not an Aseprite file.
collision_colours.kpl - A Krita palette (importable to Blender) which contains the colours used for terrain type detection.
colordome-32.ase - The main colour palette used by VGA Golf, created by Polyphorge https://lospec.com/poly-phorge
course_format.md - describes the .course and .hole files used to create custom courses, as well as the files associated with hole model files. Written in markdown format.
hole_template.blend - example hole model in Blender format
prop-export.py - Export script for Blender written in python. Used to export the positions of prop models and crowds in the *.hole format.



Converter
---------

This folder contains the model converter 'editor.exe' which will convert models exported from Blender to the crogine model format, used by VGA Golf. The first time it is run it is required to set the working directory, by going to View->Options and clicking Browse under working directory. Browse to the VGA Golf directory which contains the 'assets' folder, and select OK. The editor will now use this for all exported models and materials.

Export the models from Blender in gltf format, y-up and include vertex colours. Make sure to only export the selected model and not the entire scene. Use the File->Import Model option of the model converter to import the gltf file, making sure to check 'Convert Vertex Colourspace'. Once the model is loaded click 'convert' under the Model tab on the left of the converter window. Select the appropriate model directory of VGA Golf to save the converted model. Note that in earlier versions of the model converter the camera view is not optimal, and it may seem that large models (such as a hole) are not visible. They are merely too big for the current view.

The converted model should automatically be loaded with the default magenta PBR material applied. Under the material tab make sure to switch the shader type to Unlit, change the diffuse colour to white, and either load any textures into the diffuse slot, or check the 'use vertex colours' box if it is a ball model.

Models should have a scale of 1 Blender unit to 1 metre. For further examples it is possible to open existing models from the VGA Golf model directory via File->Open Model.



Balls
-----

Balls are 0.021 Blender units in radius, and have the origin at the bottom of the geometry, rather than the centre. Balls are also vertex coloured, any textures will be ignored, however this may change in the future. See the included ball_template Blender file for an example. Balls can be modeled however you like, although bear in mind that they appear very small in game and fine detail will not be very visible. Once a ball model has been converted to crogine format (see above) it requires a ball definition file created in a text editor, and placed in the 'balls' directory. See ball_format.md for an explanation of this file.



Holes
-----
Holes actually consist of multiple files. Full details of these files are explained in course_format.md, and can mostly be created in Blender. Hole geometry is expected to have textured materials, and vertex colours are used for creating collision data. Collision colours (listed below) can be loaded into Blender by enabling the import-export palette add-on from Preferences->Add-ons. It is then possible to import either collision_colours.ase or collision_colours.kpl for use in vertex painting. Each terrain type in the geometry should have its own material assigned (each of which can of course share a single texture) so that when the model is loaded into the game the collision geometry can be correctly broken down by terrain type. See the hole_template.blend file for an example of this.

As a rule of thumb try not to make the green larger than approximately 10m radius from the hole. Greens larger than this require long tedious putts, which can infuriate the player!

The surrounding terrain can be created by sculpting a sub-divided plane and baking the height values to a texture. This texture is then stored in the green channel of the associated map image file. See course_format.md and hole_template.blend for more details.

Tee, hole and initial player target positions can be placed by creating a new Empty for each, and naming them tee, hole and target respectively. A 'single arrow' empty is often a good choice. These empties can be placed in the blender scene to represent where each of the entities will appear in game.

Further models can be created in blender and used as props, for example vehicles or buildings. These should be exported and converted in the same way as other models first, then in Blender add a custom property named 'model_path' with the relative path of the model in the assets directory as its value - eg 'assets/golf/models/cart.cmt'. This is used with the prop-export.py script (enabled in Blender with Edit->Prefernces->Add Ons->Install...) to export the positions of prop models about the hole to a text file. This appears as File->Export->Golf Hole Data in Blender. Selected props, crowds and empties (used for tee and hole positions) will be exported to a *.hole file. See course_format.md for more information on this file.


Collision Colours
-----------------
Different types of terrain a represented by different colour values. These colours are stored in the Krita palette file, collision_colours.kpl, or Photoshop palette file collision_colours.ase. Either file can be imported to Blender with the palette import add-on enabled. This allows easily setting, for example, vertex colours of course geometry so that VGA golf can determine which part of a hole is which terrain. The colour values are (in RGB format):

        Rough   = 05,05,05
        Fairway = 15,15,15
        Green   = 25,25,25
        Bunker  = 35,35,35
        Water   = 45,45,45
        Scrub   = 55,55,55

Each section of mesh with a specific terrain type should also have its own material assigned - even if that is a duplicate of an existing material. This is so that the game will correctly divide the geometry by terrain type when it is loaded.