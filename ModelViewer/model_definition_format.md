Model Definition Format
-----------------------

Models in crogine are described in a text format that can be loaded via the `ModelDefinition` class. The text format used is the same as that used by all configuration files (loaded with the `ConfigFile/ConfigObj` class), to describe a special set of model properties. The following properties are used but often not all at the same time, to describe a model to be loaded by crogine.

    //root node. All models require the model root node, along with a name, containing no spaces
    model <model_name>
    {
        //the mesh property. This can be a path in quotation marks to a mesh file,
        //a billboard or the name of an auto-generated primitive. For example:

        mesh = "path/to/model.cmf"
        //or
        mesh = "path/to/skinned_model.iqm"
        //or
        mesh = billboard
        //or
        mesh = sphere

        //*.cmf extension mesh files are static mesh in the Crogine Mesh Format. These can be created by loading
        //*.obj files into the model viewer application and exporting them in the *.cmf format. *.iqm meshes are
        //Inter Quake Models - more about authoring those can be founde at http://sauerbraten.org/iqm/

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

        //primitive types are: sphere, cube and quad. Sphere meshes also expect a radius property with a float
        //value greater an 0. Cubes are 1x1x1 by default, but may have a size property with 3 components. 
        //Quads require two properties: size and uv. For example:
        size = 2,3.4
        uv = 0.5,1
        //these are used to describe the size of the quad in x,y coordinates, as well as the UV coordinates used
        //used for texturing.

        //if models are required to cast shadows via shadow mapping then it needs to be explicitly stated
        //these do not work with billboard geometry.
        cast_shadows = true

        //models require at least one material to describe how they are lit, and can have as many as they have
        //sub-meshes, unless they are a billboard, in which case they can only have one material. They should be
        //described in the order in which the sub-meshes appear in the mesh file.
        //materials currently have two variations, Unlit and VertexLit which uses the default blinn-phong shading.
        material Unlit
        {
            //unlit materials can have the following properties:

            //supplies a path to a texture to be used for this material
            diffuse = "path/to/image.png"

            //supplies a colour to multiply the material output by. This will
            //either affect the colour output of the diffuse texture, or
            //set the colour if the material. Colour is a 4 component normalised value
            colour = 1,0.65,0.8,1

            //tells the material to use any vertex colour property in the mesh, and multiplies
            //it with the final output colour. This can be omitted rather than set to false
            vertex_coloured = false

            //for *.iqm format meshes with skeletal animation this should be set to true, else it can be omitted
            skinned = true

            //if a rim property is supplied then rim lighting will be applied to the material.
            //the rim property contains a 4 component normalised colour value, and can have an
            //optional rim_falloff property applied which affects the strength of the rim light effect
            //as billboards always face the camera this has no visible effect on billboard meshes.
            rim = 1,0.9,0.89,1
            rim_falloff = 0.7

            //the lightmap property supplies an optional path to a pre-baked lightmap. Lightmaps are
            //mapped with the UV channel 2 properties of the mesh, so will not appear correctly
            //if the UV information is missing. This is ignored by billboards.
            lightmap = "path/to/lightmap.png"

            //if a material should recieve shadows from the shadow mapping path
            //then rx_shadows can be set to true
            rx_shadows = true

            //texture properties can also be set which are applied to all textures used in the material
            smooth = true
            repeat = false

            //by default materials are rendered with a blend mode of 'none'. This can be overriden
            //with the blendmode property. Be aware this may affect the final output depending on
            //the render order of models - for instance alpha blended materials are always rendered
            //last in a back to front order.
            blendmode = alpha
            //or
            blendmode = add
            //or
            blendmode = multiply

            //tells the material to discard fragments with an alpha value belowe this threshold.
            //useful for full transparent pixels in materials with blend modes other than alpha.
            //value should be between 0 and 1 and is automatically clamped on load. Only applied
            //to materials which have a diffuse texture.
            alpha_clip = 0.2

        }

        //vertex lit materials support all of the above, plus accept these further properties
        material VertexLit
        {
            //supplies a path to a texture containing mask data, used to control the lighting
            //properties of the material. The red channel controls specular intensity, the green
            //channel controls the specular amount, and the blue channel controls self illumination
            mask = "path/to/mask.png"

            //alternatively a mask colour can be provided in normalised 4 component format. This
            //is ignored if a mask texture is given
            mask_colour = 0.2,1,0.88,1

            //the normal property supplies a path to a normal map used for bump-mapping
            //currently billboards do not support normal mapping as they lack the tangent space data.
            normal = "path/to/normal_map.png"
        }

    }