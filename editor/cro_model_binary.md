Crogine Model Binary
--------------------

This is the currently preferred binary format for crogine models. The format can contain both static and animated meshes, alongside any skeletal animation data, skeleton notification data and skeletal attachment points. The format can have either a single mesh, a skeleton complete with sets of animations, or both. The file extension is *.cmb and is the default output of the editor application. To import animated meshes into the editor the gl Transfer Format (glTF) is preferred, in the *.glb (binary) format. For more information on glTF see [here](https://github.com/KhronosGroup/glTF). It is also possible to import the IQM inter-quake model format - see https://github.com/lsalzman/iqm for details on how to export IQM files from Blender.

As of January 2022 the skeletal animation data format has been updated, so older model binaries will need to be re-exported via the editor for use with newer versions of crogine.

For implementation details of the model format see ModelBinary.hpp in the crogine/include/detail directory of the source code.