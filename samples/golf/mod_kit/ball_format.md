Ball Definitions
----------------

To add a custom ball first create a model approximately 0.021 units in radius, and place it in the model directory. Ball models should have vertex colouring (textures are ignored) and use an Unlit material (see material definition for more information).

Since version 1.11 ball models can have up to 2 materials. Each material must share the same propreties as stated above, however the second material will be rendered with a cubemap reflection, to simulate 'metallic' materials.

Since 1.16 two new optional properties were added: label and preview_rotation (see below).

Then create a ball definition and place it in the balls directory with the extension .ball

    ball
    {
        model = "assets/golf/models/my_ball_model.cmt" //path to the ball model file
        uid = 45 //this must be unique. UIDs will be ignored if a ball file has already been loaded with the given UID. See below.
        tint = 0.23, 0.44, 0.288, 1 //this is the tint colour of the ball as it is seen from a distance. Omitting this will use a white colour.
        roll = false //optional - if this is true the ball will roll in game, but can be disabled for non-round models
        label = "My Fancy Ball" //optional, but recommended. Contains the string to display as the label in the ball browser
        preview_rotation = 3.14 //optional - rotation, in radians, around the Y axis to orientate the ball when creating a preview
    }

Ball models are synchronised across network games via their UIDs. If a ball has no matching UID on a client, then the default ball model is loaded. You must share your ball definition and model files with all clients should you want other players to be able to see them. Normally you shouldn't have to create your own UID - omitting this field from the file will cause the game to automatically generate on based on the file name the first time it is loaded. However this will not work when running the game from a macOS bundle - this should be done with a build running from the command line instead, before bundling the app. Windows and Linux builds do not have this issue assuming the current user has write permissions to the working directory. The UID should be created before sharing any custom balls.