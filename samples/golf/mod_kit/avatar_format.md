# Custom Avatars

Custom avatars can be added to Super Video Golf. Firstly an animated model needs to be created and correctly configured using the crogine model editor. The animated model requires a skeleton with at least one bone, and one attachment point named `hands` to which to attach the golf club model. It is preferred to have animations named `putt`, `drive` and `idle` within the model, although omitting these will not prevent it from being loaded. Avatar models are textured, and support only a single material. For more details on creating and configuring the model see the Super Video Golf wiki page.

Secondly, an optional audioscape can be created which defines the audio snippets played when the player performs a particuilar action.

Once the resources have been created an avatar definition file needs to be added to the `assets/golf/avatars` directory. This file is a text file with the following layout:

    avatar
    {
       model = "assets/golf/models/avatars/player01.cmt" //path to the model for the avatar
       audio = "assets/golf/sound/avatars/player01.xas" //path to the audioscape file for the avatar
       uid = 10 //see below
    }

The file must be saved with an `*.avt` extension for Super Video Golf to find it. Each avatar file requires a unique ID associated with it - this is used to synchronise avatars across network games. If an existing UID is already loaded then the avatar file will be skipped when Super Video Golf loads, printing a message to the console warning which UID was duplicated. To generate a UID for a new avatar omit this property from the file, and the first time the game is run it will create a UID based on the file name and automatically write it back to the file. This should be done once, before sharing any custom avatar files so that they properly sync across network games. Note that this will not work when running the game from a macOS bundle, you need to be using a command-line build with appropriate write permission. This does not affect the Windows and Linux versions, assuming the current user has the correct permissions to write files.

To use an avatar on multiple clients each `*.avt`, model definition, model file and (optional) audioscape must be copied to all player's assets directory.


#### Custom Hair.
It is also possible to add new hair models to Super Video Golf. These models will be attached to any avatar model that has an attachment point named `head` - although not all avatars will have this. Hair models also require a definition file with the extension `*.hct`, placed in the `assets/golf/avatars/hair` directory. The layout is almost identical to that of the avatar definition, with the omission of the audio property. The same rules apply for creating UIDs for hair models.

    hair
    {
       model = "assets/golf/models/avatars/hair/hair01.cmt" //path to the model for the avatar
       uid = 2563436 //see avatar note, above
    }

Hair models are untextured, and have their colour set by the avatar customisation menu in game. Custom colours are applied to white vertices, other vertex colours are maintained.



#### Key Colours
When texturing avatars these colours in RGB, eg 0,0,0 or 30,30,30 will be used to key the colours in the texture with the in-game palette. Other colours are left as they are and will appear unmodified on the model in game.

Avatar colour keys are:

            Dark  Light
    Bottom - 0     10
    Top    - 30    40
    Skin   - 70 (0.275)
    Hair   - 90 (0.392)

In RGB linear space - although textures MUST be RGBA