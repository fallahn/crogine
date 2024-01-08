#### Course File Format
Courses are defined by sets of multiple configuration files (subject to change)

###### Course File
A course folder should contain one file with the name `course.data`. It has the following format:

    course course_01 //name or identifier, doesn't have to be unique
    {
        skybox = "assets/golf/skyboxes/sky.sbf" //a path to the skybox definition file to use (see below)
        shrubbery = "assets/golf/shrubs/cliffs.shb" //path to the shrubbery definition file (see below)
        instance_model = "assets/golf/models/reeds_large.cmt" //optional path to a model which is instanced around the water's edge. Defaults to reeds if this is omitted
        audio = "assets/golf/sound/ambience.xas" //optional path to a crogine audioscape file for environment sounds (see below)


        title = "St. Billybob's links, Isle of Wibble" //course title, shown in the game lobby
        description = "This is a test course. Why not have a play?" //a brief description, shown in the game lobby


        //a list of paths to *.hole files. These should be in the order in which the holes are to be played.
        hole = "assets/golf/courses/course_01/01.hole"
        hole = "assets/golf/courses/course_01/02.hole"
    }

The course file contains a list of paths to `*.hole` files which describe the individual holes that make up a course. This way holes can be recycled thoughout different course layouts should it be so desired. Up to 18 holes can be added to a course (any further holes will be ignored when the course file is loaded). The optional `audio` property is a path to an `AudioScape` file. This should contain definitions for up to 6 looped sound effects, labelled 01 - 06. These are placed evenly throughout the scene and should be *mono* for positional audio to take effect. Optionally up to two emitters labelled `incidental01` and `incidental02` can be added. These should be stereo and are played at random intervals, for example this could be the sound of a plane flying overhead. These should not be looped. Finally an emitter called `music` can be defined to play the incidental music during hole transitions. This should be stereo, and also not looped. See `assets/golf/sound/ambience.xas` for an example `AudioScape`.

###### Hole file
Hole files describe which models/assets make up the hole, as well as the position of the tee, the position of the cup and the par for that hole. Optionally prop objects can be added to the hole to display models as props in the game. Crowd objects can also optionally be added to place a small crowd of spectators. By default crowds are aligned left to right along the X axis and can be rotated with the `rotation` property. Particle objects denote an emitter settings file and world position for particle effects. These are triggered as the hole is loaded.

    hole 01 //hole ID
    {
        map = "assets/golf/holes/01.png" //path to map file, see below
        model = "assets/golf/models/hole_01.cmt" //path to the hole model
        pin = 22, 0, -172 //world coordinates on the map file to the hole.
        target = 22, 0, -172 //initial direction which the player should be looking when the hole is loaded. Usually the pin, but can be different if hole is sharp dogleg
        tee = 243, 2.5, -30 //world coordinates on the map file to the tee
        par = 3 //par for this hole.

        prop
        {
            position = 70,0,45 //position in world units
            rotation = 45 //rotation around the Y axis (in degrees)
            scale = 1.0,1.0,1.0 //scale of  the model
            model = "assets/golf/models/cart.cmt" //path to a model description to use as a prop
        }

        crowd
        {
            position = -130, 0, -65 //position of the left-most crowd member in world units
            rotation = 180 //rotation about the Y axis at the left-most point
            lookat = 100, 2.4, 64 //since 1.11 an optional point for the crowd to face. If this is omitted they will look at the hole if it is close enough, or straight forward.
        }

        particles
        {
            path = "assets/particles/fire.cps" //path to particle emitter settings
            position = 120, 2, 97 //world position of particle emitter
        }
    }


Since 1.7.0 crowds and props can be given paths to follow. These have the following format:

    prop
    {
        model = "assets/golf/models/speed_boat.cmt"
        position = -1.989228,0.005000,-65.556274
        rotation = 0.000000
        scale = 1.000000,1.000000,1.000000

        path
        {
            point = 9.993729,1.633960,-65.031258
            point = 14.973561,1.633960,-61.878002
            point = 20.381990,1.633960,-59.367649

            loop = false
            delay = 8.000000
            speed = 6.0
        }
        emitter = "boat"
        particles = "assets/golf/particles/water_spray.cps"
    }

    crowd
    {
        position = 103.052010,1.262347,-135.670990
        rotation = -377.916374

        path
        {
            point = 95.192429,1.262303,-140.468903
            point = 118.227936,1.262303,-133.075958
            point = 134.671310,1.262303,-129.906708
            point = 151.243515,1.262302,-127.346008
            point = 163.376205,1.262303,-125.559677
        }
    }

The Blender export script will automatically generate these paths if a Path object is created in Blender and the prop or crowd model is parented to it. Paths with prop models parented to them can also have the extra properties `loop`, `delay` and `speed` added to them. The `loop` property determines if the prop returns to the start point when it reaches the end, if set to true, else the model will travel back along the path in the reverse direction. The `delay` property defines the number of seconds the prop waits before moving along the path again after reaching the end. `speed` is a value in metres per second at which the prop will travel along the path. These can all be added in Blender as 'custom properties' and will be exported by the Blender export script.
These properties are ignored by paths assigned to crowds, as the crowd members will walk up and down the path until the hole is unloaded.

Also since 1.7.0 props may have `emitters` added to them and `particles`. The `particles` property contains a path to a particle settings file which, if loaded successfully, will be parented to the prop, and started once the hole loads. Parented particles follow the prop as it moves along a path, so can be used for the spray behind a boat for example. The `emitter` property contains a single name which references an audio emitter defined in `assets/golf/sound/props.xas`. If the emitter exists in this file it will be parented to the prop, useful for creating effects such as the noise of a boat engine. Both emitters and particles can be defined in Blender, using a Sound object for emitters, or an Empty set to draw as a point for particle emitters (see `placeholders.blend`). If these are parented to a prop model in Blender the export script will automatically append them to the exported file.


###### Hole Model Materials
Materials used for the hole models should be set to VertexLit to enable the `mask colour` property. The mask colour is used to control a variety of effects on the materials when rendered. By default the mask value is WHITE which mean no effect... so to increase the value of a specific effect you must use `1.0 - value` - which might be counterintuitive at first.

 - Red Channel. Tilt shading. 1 = no effect, 0 = full effect. As materials tilt in the world they are gradually shaded, eg the sides of bunkers, or on the green to highlight slopes.
 - Green Channel. Water dither. 1 = no effect, 0 = full effect. As the material approaches the waterline a dithered, darkening effect is added.
 - Blue channel. Stone face. Used on stone materials it allows a stone pattern texture to be blended with materials as they become more vertical.



From version 1.15 it is possible to place all prop data in a single configuration file and include it in multiple `*.hole` files. EG
Props.include

    include
    {
        crowd
        {
            position = 103.052010,1.262347,-135.670990
            rotation = -377.916374

            path
            {
                point = 95.192429,1.262303,-140.468903
                point = 118.227936,1.262303,-133.075958
                point = 134.671310,1.262303,-129.906708
                point = 151.243515,1.262302,-127.346008
                point = 163.376205,1.262303,-125.559677
            }
        }        
    }



01.hole
    
    hole 01 //hole ID
    {
        map = "assets/golf/holes/01.png"
        model = "assets/golf/models/hole_01.cmt"
        pin = 22, 0, -172
        target = 22, 0, -172
        tee = 243, 2.5, -30
        par = 3
        include = "assets/golf/holes/props.include"
    }


02.hole
    
    hole 02
    {
        map = "assets/golf/holes/02.png"
        model = "assets/golf/models/hole_01.cmt"
        pin = 42, 0.5, -24
        target = 28, 1.05, -72
        tee = 243, 2.5, -60
        par = 4
        include = "assets/golf/holes/props.include"
    }


This way it is possible to use one definition for properties in multiple holes, useful particularly when these holes share the same model, eg the pitch n putt courses.





Hole file creation can be aided with the export script for blender. See readme.txt for more details.

The map file contains metadata about the hole stored as an image. The image is stored with a scale of one pixel per metre. In other words each pixel represents one metre square on the course grid. The XY coordinates of the image, starting at the bottom left, are mapped to the X, -Z coordinates of the 3D world. Map sizes are fixed at 320 pixels by 200 pixels.

The red channel of the map file stores a value representing the current terrain type. This is used by the client to decide where foliage billboards are placed. Pixels with a value between 50 and 59 will have large foliage rendered on them. Values of 0 - 9 will have long grass rendered on them. It is important to paint these values carefully as excessive foliage can affect performance, as well as obscure player views. Normally foliage would appear only around the border of the hole model, any further away and the billboards are not visible and are technically wasted. Areas which should diplay a slope indicator (ie the green) should be painted with the value 25. Remaining pixels should be filled with the value 15 for the fairway, or 45 outside of the hole area. Note that these values are the same as the collision colours stored in the vertex data - therefore it maybe a useful shortcut to bake the vertex colours to an image in Blender.

The green channel of the map contains height values used to create the surrounding terrain. Values of zero will be below the water plane, whereas values of 255 will raise the terrain at that point to its max height (currently 4.5 metres). A heightmap can be created by sculpting a 3D plane and baking its values to a texture in software such as Blender. See readme.txt for more details.

Note that although the blue and alpha channels are currently unused the image MUST be in RGBA format for it to be loaded correctly. These channels maybe be used in the future.


###### Skybox File (since 1.6.0)
The skybox definition file is used to declare the colour of the skybox, as well as the layout of any models used in the background. Its syntax is very similar to the hole definition file, with the same declarations used for props.

    skybox
    {
        sky_top = 1.0,1.0,1.0,1.0 //normalised colour for the top part of the sky gradient
        sky_bottom = 0.9, 0.8, 0.9, 1.0 //normalised colour for the bottom part of the sky gradient
        clouds = "path/to/clouds.spt" //path to a sprite sheet containing cloud sprites. Note that previous to 1.6.0 this was part of the course definition file.

        prop
        {
            model = "assets/golf/models/skybox/horizon01.cmt"
            position = 0,0,0 //note that in the case of sky boxes the limit is a 10 unit radius around 0,0,0 with a minimum of 3 units
            scale = 1,1,1 //models such as the horizon are specifically designed for skyboxes. Other props will work but should be scaled down to around 1/64 to look correct.
            rotation = 10.2 //rotation around the Y axis
        }
    }

For an example of creating models for a skybox see the `skybox.blend` file. Note that many models include 'fake' reflections modelled directly as part of the mesh, for cases where they are rendered outside the world's reflection plane. The vertex colour of the mesh is used to decide how much 'tint' the model receives in the reflected part to take on the water colour, where 1 (white) is no tint, and 0 (black) is completely water colour.
As of 1.11.0 the `clouds` property is deprecated and ignored.


As of 1.8.0 typing `tree_ed` into the console when at the game menu opens the treeset editor (see below). This can be used to preview and modify skybox files.


###### Shrubbery (since 1.8.0)
Up to and including version 1.7.0 shrubbery billboards were defined by two files in the the `course.data` file. These have been moved to a single file with the following format:

    shrub
    {
        model = "assets/golf/models/shrubbery_autumn.cmt" //billboard model file
        sprite = "assets/golf/sprites/shrubbery_autumn.spt" //sprite sheet defining billboard sizes at 64px per metre.

        treeset = "assets/golf/treesets/oak.tst" //path to a treeset file - see below

        //since 1.9.0 these have been moved from course.data
        grass = 1,0.5,0,1 //surrounding grass colour. This is optional and will default to dark green from the colordome-32 palette
        grass_tint = 1.0, 1.0, 0.5, 1 //affects the colour of the noise applied to the grass colour. This is optional, by default just darkens the grass colour
    }

Shrub files have the extension `*.shb` and should be stored in `assets/golf/shrubs`. The `.cmt` file is a standard billboard definition file - see the crogine model definition format. The `.spt` file is a crogine format sprite sheet, which defines a specific set of sprites:

 - grass01
 - grass02
 - hedge01
 - hedge02
 - flowers01
 - flowers02
 - flowers03
 - tree01
 - tree02
 - tree03
 - tree04

The sprites are mapped to the billboards at 64 pixels per metre (game unit). The tree sprites will be substituted with a treeset if it is available and the tree quality is set to 'high' in the game options. It is also possible to supply a second model and spritesheet definition for 'classic' rendering - ie when the game option for tree quality is set to 'classic'. To do this add a second definition file for each with the same name appended with `_low`. For example `shrubbery_autumm_low.cmt` and `shrubbery_autumn_low.spt`. These are automatically loaded by the game if they are found. Each sprite has a colour property in its sprite sheet entry, which can be used to determine how much the wind affects the billboard when it is rendered. Colour values are RGBA normalised, and default to {1,1,1,1} (White). Reducing the value of the red channel *increases* the effect of low frequency movement, reducing the value of the green channel increases the amount of high frequency movement, and decreasing the blue channel value increases the amount of bend applied along the wind direction vector. The mod kit contains a palette file named `shrub_wind_colours.ase` which contains presets found to be generally acceptable for hedge, flower and grass type billboards. Any values deemed fit can be used, of course.


###### Treesets (since 1.8.0)
Treesets can be used to define 3D models to render trees instead of the default billboarded trees. These are optional and, if omitted from the shrubbery file, will fall back to drawing tree01-tree04 as defined in the shrub sprite sheet. As treesets are added they replace their corresponding sprite, so for example if two treesets are added to the shrubbery file then sprites tree01 and tree02 will not be drawn. Tree03 and tree04 will still appear as the shrub file defines them.


    treeset 
    {
        model = "oak01.cmt" //model to draw. Must be RELATIVE to the treeset file (ie in the same directory)
        texture = "oak_leaf.png" //texture used by the leaf material - RELATIVE to the treeset file
        colour = 0.494118,0.427451,0.215686 //colour multiplied with the leaf texture
        randomness = 0.196000 //see editor window
        leaf_size = 0.600000 //see editor window
        branch_index = 0 //submesh index of the branch part of the model. There can be multiple entries for this
        leaf_index = 1 //submesh index of the leaf part of the model. There can be multiple entries for this
    }

Treesets have a few special requirements when creating the model. The trunk part of the model requires a separate material assigned to it from the leaf part, as the game uses different shaders to render each part. The models should also use vertex colours to define how much they are affected by wind in game. The blue channel contains the strength of the bend, so a value of 0 (black) is usually placed on the vertices at the roots of the trunk with a higher (0.5 - 1.0) value on the vertices at the tips. The red channel contains the amount of high frequency 'jitter' caused by the wind effect so leaf vetices are usually painted a magenta colour, which is created from a high red and blue channel value. The `mod_kit` directory contains `wind_colours.ase` which can be used as a starter palette for these values. Materials for the branch parts of tree models are textured, and should be UV mapped. When converting the model file using the crogine editor, branch materials should be set to unlit with a diffuse texture, and leaf materials should be unlit with vertex colouring. Once the model is correctly converted and saved in `assets/golf/treesets/` the treeset metadata which controls the final appearance can be defined using the treeset editor.

The treeset editor is found by launching the game, opening the console with F1 (when the game is on the main menu) and entering the command `tree_ed`. In the new window which opens it is possible to define which parts of the model are assigned the branch material, and which is the leaf material. The leaf material also requires a texture to be loaded for the leaves, and optionally a leaf colour can be picked from the palette window. Once the look has been defined, selecting `save preset` from the file menu will write the treeset file which can be used to define treesets in the `course.data` file. IMPORTANT: as the game allows setting the tree quality in the options menu the game requires a billboard version of each tree which can be used to draw the model on low quality mode. To aid this the Billboard window provides a flat 2D view of the tree model, where an image can be quickly saved by clicking the `save` button, for later editing into the sprite sheet used for this treeset's shrubbery file.

The treeset editor can also be used to open and modify skybox files (see above). To use this feature open the skybox editor window from the `View` menu. Here it is possible to open skybox files, add or remove models from the skybox, set the sky colour, and save the skybox file again.

###### Hole Model Collision Data (since 1.11.0)
The vertex colours of hole models are used to set the collision data used by the game. Up to version 1.10.0 all three channels represented the terrain type, and were required to match that of the the vertices current material. From 1.11.0 collision data is now set per-triangle and can be defined independently of the material. Terrain type is now also stored only in the red channel of the vertex, although the values are the same as the existing TerrainID enum. The green channel is used for special trigger values, which are defined by TriggerID. These values do not overlap those of the terrain for backwards compatibility. The blue channel is used to blend in the colour property of the active material, with that of the texture of the current material. This way transitions can be created between triangles assigned to different materials. Note that the previous values stored in the colour propery used to mask the 'tilt' and 'water level' amount, as introduced in 1.10.0, have been moved to the colour_mask property of materials. This also means that the default terrain material, as seen in the model editor, is now VertexLit, not Unlit, as it was in previous versions.

###### Prop Models
Prop models are textured models, and can be static or contain skeletal animation. Animated models will, by default, play animation zero once the hole is loaded, and the animation is stopped again when the hole is unloaded. Static models can use the vertex colours to allow wind effects. The red channel dicatates the amount of low frequency effect, the green channel the high frequency, and blue channel the amount of bend. These colours can also be applied to billboarded geometry using the billboard's Sprite colour property. NOTE this is DIFFERENT from treeset models *sigh* which use red for high frequncy and blue for bend - the green channel is unused in these cases (see above).

###### Lighting
From 1.15.0, when night time mode was introduced, it is possible to add point lights.

    light
    {
        position = 1.0, 1.0, 1.0
        colour = 1.0, 1.0, 1.0, 1.0
        radius = 4.0
        animation = "klnknknlnknlkmm"
        preset = "bollard"
    }

Postion, colour and radius are all required properties, however animation is optional. The animation is a string as used in the Quake engine games (read more [here](https://www.alanzucconi.com/2021/06/15/valve-flickering-lights/)) - in brief the character 'm' represents full bright lightness, with 13 steps to zero ('a') and 13 steps to double bright ('z') Animations are played back at approximately 10 frames per second. In Blender lights are exported from Point style lights, and the postion, colour and radius are automatically read from the object properties. Animations can be added as a custom property with the type 'string'. In place of the colour and radius properties it is possible to use a preset file. Presets are contained in the `lights` directory and have the same format, omitting the `position` and `preset` properties. To use a preset with Blender add a custom property to the light object named `preset` with the type 'string' and set its value to one of the names light presets (without the file extension) in the `lights` directory. Presets override any existing colour radius and animation properties.