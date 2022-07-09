Model Definition Format
-----------------------

Models in crogine are described in a text format that can be loaded via the `ModelDefinition` class. The text format used is the same as that used by all configuration files (loaded with the `ConfigFile/ConfigObj` class), to describe a special set of model properties. The following properties are used but usually not all at the same time, to describe a model to be loaded by crogine.

    //root node. All models require the model root node, along with a name, containing no spaces
    model <model_name>
    {
        //The mesh property. This can be a path in quotation marks to a mesh file,
        //a billboard, or the name of an auto-generated primitive. For example:

        mesh = "path/to/model.cmf"
        //or
        mesh = "path/to/skinned_model.iqm"
        //or
        mesh = "path/to/skinned_model.cmb"
        //or
        mesh = billboard
        //or
        mesh = sphere

        //*.cmf extension mesh files are static mesh in the Crogine Mesh Format. These can be created by loading
        //*.obj, *.dae or *.fbx files into the editor application and exporting them in the *.cmf format. *.iqm meshes are
        //Inter Quake Models - more about authoring those can be founde at http://sauerbraten.org/iqm/

        //NOTE since the addition of the *.cmb (cro model binary) format that this is the preferred file type, as this can contain
        //both static and animated models. This is now the default output for the editor application.

        //billboards are quad primitives which always face the camera no matter the orientation. Billboards have two
        //optional proprties.
        lock_rotation = true
        lock_scale = true

        //When rotation locking is set to true billboards will only rotate to face the camera on the y axis, remaining
        //perpendicular to the ground (x/z) plane. This is usually the preferred behaviour of billboards used to represent
        //grass or foliage for example. When rotation locking is disabled the billboard will always rotate to face the
        //camera.
        //When the scale lock is set to true the billboards will always remain the same scale relative to the display
        //ie no perspective is applied. This might be preferred for health or other status indicators which need to remain
        //visible regardless of the camera distance. Note that rotation locking has no effect in this case, and using
        //a VertexLit material will likely have undesirable results. For more information about how to create billboard
        //geometry see the documentation for the BillboardCollection component.

        //Primitive types are: sphere, cube and quad. Sphere meshes also expect a radius property with a float
        //value greater an 0. Cubes are 1x1x1 by default, but may have a size property with 3 components. 
        //Quads require two properties: size and uv. For example:
        size = 2,3.4
        uv = 0.0,0.0,0.5,1
        //These are used to describe the size of the quad in x,y coordinates, as well as the UV coordinates used
        //used for texturing. UV coordinates are a rectangle described as left,bottom,width,height in normalised
        //coordinates

        //If models are required to cast shadows via shadow mapping then it needs to be explicitly stated although
        //this does not work with billboard geometry.
        cast_shadows = true

        //Models require at least one material to describe how they are lit, and can have as many as they have
        //sub-meshes, unless they are a billboard, in which case they can only have one material. They should be
        //described in the order in which the sub-meshes appear in the mesh file.
        //materials currently have two variations, Unlit and VertexLit which uses blinn-phong shading.
        material Unlit
        {
            //Unlit materials can have the following properties:

            //Supplies a path to a texture to be used for this material
            diffuse = "path/to/image.png"

            //Defines the diffuse colour of the material. If a diffuse map is supplied then it is
            //multiplied by this colour. This is a 4 component normalised value in RGBA order
            colour = 1,0.65,0.8,1

            //Tells the material to use any vertex colour property in the mesh, and multiplies
            //it with the final output colour. This can be omitted rather than set to false
            vertex_coloured = false

            //For *.iqm format meshes with skeletal animation this should be set to true, else it can be omitted
            //If a model appears to have lost any animation data or is not animating as expected, make sure
            //that this property is set.
            skinned = true

            //If a rim property is supplied then rim lighting will be applied to the material.
            //The rim property contains a 4 component normalised colour value, and can have an
            //optional rim_falloff property applied which affects the strength of the rim light effect.
            //As billboards always face the camera this has no visible effect on billboard meshes.
            rim = 1,0.9,0.89,1
            rim_falloff = 0.7

            //The lightmap property supplies an optional path to a pre-baked lightmap. Lightmaps are
            //mapped with the UV channel 2 properties of the mesh, so will not appear correctly
            //if the UV information is missing. This is ignored by billboards.
            lightmap = "path/to/lightmap.png"

            //If a material should recieve shadows from the shadow mapping path
            //then rx_shadows must be set to true, else can be omitted if it is not needed.
            rx_shadows = true

            //Texture properties can also be set which are applied to all textures used in the material.
            //Note that if these textures are shared with other models that the last loaded definition
            //file will overwrite the current texture settings.
            smooth = true
            repeat = false

            //By default materials are rendered with a blend mode of 'none'. This can be overridden
            //with the blendmode property. Be aware this may affect the final output depending on
            //the render order of models - for instance alpha blended materials are always rendered
            //last in a back to front order.
            blendmode = alpha
            //or
            blendmode = add
            //or
            blendmode = multiply

            //Tells the material to discard fragments with an alpha value below this threshold.
            //Useful for full transparent pixels in materials with blend modes other than alpha.
            //The value should be between 0 and 1 and is automatically clamped on load. Only applied
            //to materials which have a diffuse texture.
            alpha_clip = 0.2

            //Remaps the textures used in the materials to a smaller subrectangle. For example
            //a value of 0,0,0.5,0.5 will map the bottom left quadrant of a texture to the model.
            //This is useful for merging several textures into one large texture without having
            //to remap the meshdata, animating textures by updating the subrect with frames
            //of an animation, or assigning variations of a texture to the same mesh data.
            //Note that the rectangle coordinates are normalised values given as 
            //left, bottom, width, height. Note that this is ignored if a material's animated
            //flag is set to true (see below)
            subrect = 0.5,0.0,0.5,0.5

            //depth testing can be disabled on a material, for example when rendering a wireframe
            //this is optional and defaults to false.
            depth_test = true

            //by default materials are single-sided, that is rear-facing polygons are not drawn.
            //Setting this to true will draw both sides of the geometry, for example when rendering
            //semi-transparent objects like glass, or objects which have no thickness to them.
            double_sided = false

            //setting the animated property to true will cause the material texture to scroll
            //through a set of frames defined by dividing the texture by the row_count and col_count
            //properties. Animations play column first, top to bottom, then left to right. For example
            //a row_count of 5 and col_count of 2 creates two columns totalling five frames.
            //animations play on an endless loop. Note that setting this to true will override
            //any subrect value with that of the subrect calculated by the row/col count.
            animated = true

            row_count = 5

            col_count = 2

            //framerate dictates at which rate the animation plays, this has an internal limitation
            //of 60fps - higher values are clamped to this
            framerate = 12.5

        }

        //Vertex lit materials support all of the above, plus accept these further properties
        material VertexLit
        {
            //Supplies a path to a texture containing mask data, used to control the lighting
            //properties of the material. The red channel controls specular intensity, the green
            //channel controls the specular amount, and the blue channel controls self illumination.
            //The alpha channel controls the amount of reflection of the Scene's skybox, if it
            //has one, else reflections appear black. An alpha value of 0 is full reflection,
            //and 1 is no reflection, so by default materials are not reflective.
            mask = "path/to/mask.png"

            //Alternatively a mask colour can be provided in normalised 4 component format. This
            //is ignored if a mask texture is given
            mask_colour = 0.2,1,0.88,1

            //The normal property supplies a path to a normal map used for bump-mapping
            //currently billboards do not support normal mapping as they lack the tangent space data.
            normal = "path/to/normal_map.png"
        }

        //On desktop builds PBR (physically based rendering) materials are available. These have the
        //same properties as VertexLit materials, but some of the properties behave slightly differently.
        //The most notable difference is that the mask values represent the metallicness (metallic based
        //workflow) in the red channel, the roughness in the green channel and the ambient occlusion
        //(ao) in the green channel. The diffuse property (both colour and texture map) is used to
        //store the albedo of the PBR material. PBR materials are not designed with mobile builds in
        //mind, however crogine will still attempt to load them. For PBR materials to render correctly
        //an EnvironmentMap must be loaded from and HDRI file (see EnvironmentMap documentation)
        material PBR
        {
            //see VertexLit for properties.
        }

    }