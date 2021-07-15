#### Course File Format
Courses are defined by sets of two different kinds of file (subject to change)

###### Course File
A course folder should contain one file with the course.data. It has the following format:

    course course_01 //name or identifier, doesn't have to be unique
    {
        skybox = "assets/images/skybox/sky.ccm" //a path to the skybox to use when the course is loaded

        description = "This is a test course. Why not have a play?" //a brief description, shown in the game's UI


        //a list of paths to *.hole files. These should be in the order in which the holes are to be played.
        hole = "assets/golf/courses/course_01/01.hole"
        hole = "assets/golf/courses/course_01/02.hole"
    }

The course file contains a list of paths to *.hole files which describe the individual holes that make up a course. This way holes can be recycled thoughout different course layouts should it be so desired. As many holes as needed can be added to a course (within reason) although 9 and 18 are the most common numbers.

###### Hole file
Hole files describe which models/assets make up the hole hole, as well as the position of the tee, the hole and the par for that hole.

    hole 01 //hole ID
    {
        map = "assets/golf/holes/01.png" //path to map file, see below
        model = "assets/golf/models/hole_01.cmt" //path to the hole model
        pin = 22, 172 //XY coordinates on the map file to the hole. This is automatically converted to world units when loaded
        tee = 243, 30 //XY coordinates on the map file to the tee
        par = 3 //par for this hole.
    }

The map file contains metadata about the hole stored as an image. The image is stored with a scale of one pixel per metre. In other words each pixel represents one metre square on the course grid. The XY coordinates of the image, starting at the bottom left, are mapped to the X, -Z coordinates of the 3D world. The red and green channels represent the XZ axis of the slope normal for the current grid square. This is used to update the ball physics as if the ground was sloped/uneven. The ball will roll along this slope, or bounce accordingly to its reflection.

The blue channel of the map file stores a value representing the current terrain type. Terrains such as rough, fairway, green, bunker or water affect the ball's behaviour. The format for this is yet to be finalised.