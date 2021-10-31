VGA Golf Mod Kit
----------------

This folder contains Blender files and tools for creating custom balls and golf courses for VGA Golf.

See http://fallahn.itch.io/vga-golf for more details.


ball_format.md - description of the *.ball files used to load custom balls. Written in markdown format.
ball_template.blend - ball template file for Blender
collision_colours.ase - A Photoshop palette which contains colours for terrain collision. Not an Aseprite file.
collision_colours.kpl - A Krita palette (importable to Blender) which contains the colours used for terrain type detection.
course_format.md - describes the .course and .hole files used to create custom courses, as well as the files associated with hole model files. Written in markdown format.
hole_template.blend - example hole model in Blender format
prop-export.py - Export script for Blender written in python. Used to export the positions of prop models and crowds in the *.hole format.



Converter
---------

This folder contains the model converter 'editor.exe' which will convert models exported from Blender to the crogine model format, used by VGA Golf. The first time it is run it is required to set the working directory, by going to View->Options and clicking Browse under working directory. Browse to the VGA Golf directory which contains the 'assets' folder, and select OK. The editor will now use this for all exported models and materials.

Export the models from Blender in gltf format, y-up and with vertex colours if exporting a ball model. Make sure to only export the selected model and not the entire scene. Use the File->Import Model option of the model converter to import the gltf file, and click 'convert' under the Model tab on the left of the converter window. Select the appropriate model directory of VGA Golf to save the converted model.

The converted model should automatically be loaded with the default magenta PBR material applied. Under the material tab make sure to switch the shader type to Unlit, change the diffuse colour to white, and either load any textures into the diffuse slot, or check the 'use vertex colours' box if it is a ball model.

Models should have a scale of 1 Blender unit to 1 metre. For further examples open existing models from the VGA Golf model directory via File->Open Model



Balls
-----

Balls are 0.021 Blender units in radius, and have the origin at the bottom of the geometry, rather than the centre. Balls are also vertex coloured, any textures will be ignored, however this may change in the future. See the included ball_template Blender file for an example. Balls can be modeled however you like, although bear in mind that they appear very small in game and fine detail will not be very visible. Once a ball model has been converted to crogine format (see above) it requires a ball definition file created in a text editor, and placed in the 'balls' directory. See ball_format.md for an explanation of this file.



Holes
-----
Holes actually consist of multiple files. Full details of these files are explained in course_format.md, and can mostly be created in Blender. Hole geometry is expected to have a textured material, although vertex colours can be useful for creating collision data, used in the red channel of the collision map (see course_format.md). Using Blender Cycles vertex colours can be baked to a 320x200 pixel texture as a starting point for a collision map. Using the baking technique sculpted meshes for the surrounding terrain can be converted to a height map for the green channel of the collision map, or baked as a normal map to describe the slopes of the hole. See the Blender documentation for details of the baking process.

As a rule of thumb try not to make the green larger than approximately 10m radius from the hole. Greens larger than this require long tedious putts, which can infuriate the player!

Further models can be created in blender and used as props, for example vehicles or buildings. These should be exported and converted in the same way as other models first, then in Blender add a custom property named 'model_path' with the releative path of  the model in the assets directory as its value - eg 'assets/golf/models/cart.cmt'. This is used with the prop-export.py script (enabled in Blender with Edit->Prefernces->Add Ons->Install...) to export the positions of prop models about the hole to a text file. This appears as File->Export->Object Positions in Blender. The output of this file can then be easily copy/pasted into a *.hole definition file.


Collision Colours
-----------------
Different types of terrain a represented by different colour values. These colours are stored in the Krita palette file, collision_colours.kpl. This file can be opened in the free software Krita, or imported to Blender with the palette import add-on enabled. This allows easily setting, for example, vertex colours of course geometry so that VGA golf can determine which part of a hole is which terrain. The colour values are (in RGB format):

        Rough   = 05,05,05
        Fairway - 15,15,15
        Green   - 25,25,25
        Bunker  - 35,35,35
        Water   - 45,45,45
        Scrub   - 55,55,55
        Hole    - 65,65,65