Crogine Mesh Format
-------------------

The crogine mesh format (*.cmf) is a binary vertex data format used to store static meshes. Meshes are composed of a single vertex array with up to 32 index arrays. No material data is stored in the mesh file, rather it is described as part of the model defintion (*.cmt) file. See the model definition format markdown file for more information on materials.

Vertices can store up to 7 properties, which are flagged in a description byte in the following order:

    Position
    Colour
    Normal
    Tangent
    Bitangent
    UV0 - for diffuse mapping
    UV1 - for light mapping

All properties are optional except the position property. Vertex data is stored as floating point values. All properties are 3 component vectors (alpha values are currently not supported for the colour property) with the exception of UV0 and UV1 which are both 2 component. Colour, normal, tangent and bitangent are all expected to be normalised (per component for the colour property, length for the others). A cmf can be loaded directly with the `cro::StaticMeshBuilder` although it is generally recommended to create a model definition file which references the *.cmf file and loads that instead via `cro::ModelDefinition`. The data of a mesh file is laid out as follows:

###### uint8 flags.
The flags byte indicate which of the vertex properties are contained in the vertex data, flagged in the above order - for example Position = (1 << 0) Colour = (1 << 1) and so on.

###### uint8 index array count.
This byte contains the number of index arrays (sub-meshes, usually each assigned their own material) in the mesh file. This is often at least 1, but a value of zero can be used to assume all vertices should be drawn in their existing order. This value can be no more than 32.

###### int32 index array offset.
This is the the offset, in bytes, from the beginning of the file to the beginning of the first index array. Zero is only a valid value if the index array count is also zero.

###### int32[] indices size array.
An array of 4 byte values describing the size *in bytes* of each of the index arrays in the mesh file. It is the size of `index array count` or should contain a single 0xffffffff value if `index array count` is zero.

###### float[] vertex array.
The vertex data. The size of each vertex can be calculated from the flag data stating which properties the vertex is expected to have multiplied by the size of float multiplied by the number of components of each property. The total length of the array is measured from the current file offset (after the header has been read) substracted from the `index array offset` value. The length can be validated by making sure it divides equally by the size of a vertex.

###### index arrays.
The index arrays are contiguously packed uint32 values. There should be `index array count` number of arrays, each one `indices size array[x]` in size. The order of these arrays should match the order in which materials appear in any model definition file which may refer to the current mesh.

All values are little endian.
