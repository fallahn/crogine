bl_info = {
    "name": "Export object positions",
    "author": "Bald Guy",
    "version": (2021, 9, 24),
    "blender": (2, 93, 0),
    "location": "File > Export > Object Positions",
    "description": "Export position and rotation info of selected objects",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Import-Export"}

import os, struct, math
import bpy
import bpy_extras.io_utils

def WriteProp(file, modelName, location, rotation):
    file.write("prop\n{\n")
    file.write("    model = \"%s\"\n" % modelName)
    file.write("    position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("    rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))
    file.write("}\n\n")

def WriteCrowd(file, location, rotation):
    file.write("crowd\n{\n")
    file.write("    position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("    rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))
    file.write("}\n\n")

class ExportInfo(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    '''Export object position and rotation info'''
    bl_idname = "export.golf"
    bl_label = 'Export Object Info'
    filename_ext = ".txt"

    def execute(self, context):
        file = open(self.properties.filepath, 'w')

        for ob in bpy.context.selected_objects:

            modelName = ob.name
            if ob.get('model_path') is not None:
                modelName = ob['model_path']

            worldLocation = None
            worldRotation = None

            if ob.parent is None:
                worldLocation = ob.location
                worldRotation = ob.rotation_euler
            else:
                worldLocation = ob.matrix_world @ ob.location
                worldRotation = ob.matrix_world.to_euler('XYZ')


            if "crowd" not in modelName.lower():
                WriteProp(file, modelName, worldLocation, worldRotation)
            else:
                WriteCrowd(file, worldLocation, worldRotation)


        file.close()
        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(ExportInfo.bl_idname, text="Object Positions (.txt)")


def register():
    bpy.utils.register_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)