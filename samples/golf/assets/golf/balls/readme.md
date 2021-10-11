Ball Definitions
----------------

To add a custom ball first create a model approximately 0.021 units in radius, and place it in the model directory. Ball models should have vertex colouring (textures are ignored) and use an Unlit material (see material definition for more information).

Then create a ball definition and place it in this directory with the extension .ball

    ball
    {
        model = "assets/golf/models/my_ball_model.cmt" //path to the ball model file
        uid = 45 //this must be unique. UIDs will be ignored if a ball file has already been loaded with the given UID
        tint = 0.23, 0.44, 0.288, 1 //this is the tint colour of the ball as it is seen from a distance. Omitting this will use a white colour.
    }

Ball models are syncronised across network games via their UIDs. If a ball has no matching UID on a client, then the default ball model is loaded. You must share your ball definition and model files with all clients should you want other players to be able to see them.