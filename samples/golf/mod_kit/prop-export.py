bl_info = {
    "name": "Export golf hole data",
    "author": "Bald Guy",
    "version": (2022, 3, 6),
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
import mathutils

def vecMultiply(vec, vec2):
    temp = []

    for x, i in enumerate(vec):
        temp.append(i * vec2[x])

    return mathutils.Vector(temp)


def WriteProperty(file, propName, location):
    file.write("    %s = %f,%f,%f\n\n" % (propName, location[0], location[2], -location[1]))

def WriteProp(file, modelName, location, rotation, scale):
    file.write("    prop\n    {\n")
    file.write("        model = \"%s\"\n" % modelName)
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("        rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))
    file.write("        scale = %f,%f,%f\n" % (scale[0], scale[2], scale[1]))
    file.write("    }\n\n")

def WriteCrowd(file, location, rotation):
    file.write("    crowd\n    {\n")
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("        rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))
    file.write("    }\n\n")

def WriteParticles(file, path, location):
    file.write("    particles\n    {\n")
    file.write("        path = \"%s\"\n" % path)
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("    }\n\n")

class ExportInfo(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    '''Export object position and rotation info'''
    bl_idname = "export.golf"
    bl_label = 'Export Object Info'
    filename_ext = ".hole"

    def execute(self, context):
        file = open(self.properties.filepath, 'w')

        scene = bpy.context.scene

        file.write("hole %s\n{\n" % Path(self.properties.filepath).stem)

        if scene.get('map_path') is not None:
            file.write("    map=\"%s/%s.png\"\n" % (scene['map_path'], Path(self.properties.filepath).stem))
        else:
            file.write("    map=\"assets/golf/courses/course_0/%s.png\"\n" % Path(self.properties.filepath).stem)

        if scene.get('model_path') is not None:
            file.write("    model=\"%s\"\n" % scene['model_path'])
        else:
            file.write("    model=\"assets/golf/models/course_0/%s.cmt\"\n" % Path(self.properties.filepath).stem)
        
        if scene.get('par') is not None:
            file.write("    par = %d\n\n" % scene['par'])
        else:
            file.write("    par = 4\n\n")

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
                worldScale = None

                if ob.parent is None:
                    worldLocation = ob.location
                    worldRotation = ob.rotation_euler
                    worldScale = ob.scale
                else:
                    worldLocation = ob.matrix_world @ ob.location
                    worldRotation = ob.matrix_world.to_euler('XYZ')
                    worldScale = vecMultiply(ob.parent.scale, ob.scale)


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
                        WriteProp(file, modelName, worldLocation, worldRotation, worldScale)
                    elif ob.type == 'EMPTY':
                        if ob.get('type') is not None:
                            if ob['type'] == 1:
                            # is a particle emitter
                                if ob.get('path') is not None:
                                    WriteParticles(file, ob['path'], worldLocation)
                                else:
                                    WriteParticles(file, "path_missing", worldLocation)
                    elif ob.type == 'SPEAKER':
                        self.report({'INFO'}, "Found a speaker")

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
