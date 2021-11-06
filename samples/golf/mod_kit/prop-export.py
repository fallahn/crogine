bl_info = {
    "name": "Export golf hole data",
    "author": "Bald Guy",
    "version": (2021, 11, 3),
    "blender": (2, 80, 0),
    "location": "File > Export > Golf Hole",
    "description": "Export position and rotation info of selected objects",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Import-Export"}

import os, math
from pathlib import Path
import bpy
import bpy_extras.io_utils

def WriteProperty(file, propName, location):
    file.write("    %s = %f,%f,%f\n\n" % (propName, location[0], location[2], -location[1]))

def WriteProp(file, modelName, location, rotation):
    file.write("    prop\n    {\n")
    file.write("        model = \"%s\"\n" % modelName)
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("        rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))
    file.write("    }\n\n")

def WriteCrowd(file, location, rotation):
    file.write("    crowd\n    {\n")
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("        rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))
    file.write("    }\n\n")

class ExportInfo(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    '''Export object position and rotation info'''
    bl_idname = "export.golf"
    bl_label = 'Export Object Info'
    filename_ext = ".hole"

    def execute(self, context):
        file = open(self.properties.filepath, 'w')

        file.write("hole %s\n{\n" % Path(self.properties.filepath).stem)
        file.write("    map=\"path/to/map.png\"\n")
        file.write("    model=\"path/to/model.cmt\"\n")
        file.write("    para = 4\n\n")

        teeWritten = False
        pinWritten = False
        targetWritten = False

        for ob in bpy.context.selected_objects:

            if ob.type == 'MESH' or ob.type == 'EMPTY':

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


                if "crowd" in modelName.lower():
                    WriteCrowd(file, worldLocation, worldRotation)
                elif modelName.lower() == "pin":
                    if pinWritten == False:
                        WriteProperty(file, "pin", worldLocation)
                        pinWritten = True
                elif modelName.lower() == "target":
                    if targetWritten == False:
                        WriteProperty(file, "target", worldLocation)
                        targetWritten = True
                elif modelName.lower() == "tee":
                    if teeWritten == False:
                        WriteProperty(file, "tee", worldLocation)
                        teeWritten = True
                else:
                    if ob.type == 'MESH':
                        WriteProp(file, modelName, worldLocation, worldRotation)

        file.write("}")
        file.close()
        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(ExportInfo.bl_idname, text="Golf Hole Data (.hole)")


def register():
    bpy.utils.register_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)
