# Custom Avatars

Custom avatars can be added to VGA golf. First a sprite sheet must be created, which contains the images of the avatar used to display the idle and swing animations. Crogine sprite sheets contain the information such as sprite names, animation names, and rectangle definitions used to create frames within an image atlas. For an example of this see the files ending in `*.spt` in the `assets/sprites` or `assets/golf/sprites` directory, which can be opened in a text editor. (Eventually the crogine asset editor will allow GUI style manipulation of these files.)

Sprite sheets used for VGA Golf avatars need to follow a few rules:
 - A single image must contain all the animation frames for two sprites:
 - The two sprites must be defined as 'wood' and 'iron' depicting each of the two types of club
 - Each sprite must have two animations 'idle' and 'swing'

Secondly, and optional audioscape can be created which defines the audio snippets played when the player performs a particuilar action. TBD

Once the resources have been created an avatar definition file needs to be added to the `assets/golf/avatars` directory. This file is a text file with the following layout:

    avatar
    {
       sprite = "assets/golf/sprites/player01.spt" //path to the sprite sheet for the avatar
       audio = "assets/golf/sound/avatars/player01.xas" //path to the audioscape file for the avatar
       uid = 10 //see below
    }

The file must be saved with an `*.avt` extension for VGA Golf to find it. Each avatar file requires a unique ID associated with it - this is used to syncronise avatars across network games. The number should be between 0 and 255, and not have the same value as another avatar in the the current directory. If an exisiting UID is already loaded then the avatar file will be skipped when VGA Golf loads, printing a message to the console warning which uid was duplicated.

To use an avatar in multiple clients each `*.avt`, sprite sheet, sprite image and optional audioscape must be copied to all player's assets directory.