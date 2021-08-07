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

The map file contains metadata about the hole stored as an image. The image is stored with a scale of one pixel per metre. In other words each pixel represents one metre square on the course grid. The XY coordinates of the image, starting at the bottom left, are mapped to the X, -Z coordinates of the 3D world.

The red channel of the map file stores a value representing the current terrain type. Terrains such as rough, fairway, green, bunker or water affect the ball's behaviour. Specific terrain types aredefined in the struct `TerrainID`. The colours in the red channel are multiplied by 10, and divided accordingly on load. This allows for a slight variance in value which may be introduced by image compression. For example the terrain ID for `Rough` has the value 5, which allows the red channel to be between 50-54.

The green channel of the map contains heigh values used to create the surrounding terrain. Values of zero will be below the water plane, whereas values of 255 will raise the terrain at that point to its max height (currently 3.5 metres).

Surface normals for a hole can be stored in an image containing a normal map. When a hole is loaded the directory is automatically searched for a file with the same name as the map, appended with an n. For example `01.png` would use a normal map called `01n.png`. If a normal map is not found all normals are assumed to be vertical. The normals in the normal map are used to describe the surface of the course, and affect how the ball bounces and rolls across the hole, without having to model the terrain accordingly. This is most effective for adding a gradient to the green.