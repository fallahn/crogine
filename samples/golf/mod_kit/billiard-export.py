bl_info = {
    "name": "Export billiard table data",
    "author": "Bald Guy",
    "version": (2022, 4, 9),
    "blender": (2, 80, 0),
    "location": "File > Export > Billiard Table",
    "description": "Export pocket position and model data",
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


def WriteProperty(file, name, value):
    file.write("    %s = \"%s\"\n\n" % (name, value))


def WritePocket(file, location, value, radius):
    file.write("    pocket\n    {\n")
    file.write("        position = %f,%f\n" % (location[0], location[1]))
    file.write("        value = %d\n" % value)
    file.write("        radius = %f\n" % radius)
    file.write("    }\n\n")

def WriteSpawn(file, location, halfWidth, halfHeight):
    file.write("    spawn\n    {\n")
    file.write("        position = %f,%f\n" % (location[0], location[1]))
    file.write("        size = %f,%f\n" % (halfWidth, halfHeight))
    file.write("    }\n\n")    

class ExportInfo(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    '''Export object position and rotation info'''
    bl_idname = "export.billiards"
    bl_label = 'Export Table Info'
    filename_ext = ".table"

    def execute(self, context):
        file = open(self.properties.filepath, 'w')

        scene = bpy.context.scene

        file.write("table %s\n{\n" % Path(self.properties.filepath).stem)

        if scene.get('model_path') is not None:
            file.write("    model=\"%s\"\n" % scene['model_path'])
        else:
            file.write("    model=\"assets/golf/models/hole_19/%s.cmt\"\n\n" % Path(self.properties.filepath).stem)


        if scene.get('collision_path') is not None:
            file.write("    collision=\"%s\"\n" % scene['collision_path'])
        else:
            file.write("    collision=\"assets/golf/models/hole_19/%s_collision.cmt\"\n" % Path(self.properties.filepath).stem)

        if scene.get('ruleset') is not None:
            file.write("    ruleset=\"%s\"\n\n" % scene['ruleset'])
        else:
            file.write("    ruleset=\"8_ball\"\n\n")

        file.write("    ball_skin=\"assets/golf/images/billiards/pool_red.png\"\n\n")
        

        for ob in bpy.context.selected_objects:

            if ob.type == 'EMPTY':

                worldLocation = None

                if ob.parent is None:
                    worldLocation = ob.location
                else:
                    worldLocation = ob.matrix_world @ ob.location


                #pockets
                if ob.empty_display_type == 'CIRCLE':

                    pocketValue = 0

                    if ob.get('value') is not None:
                        pocketValue = ob['value']

                    pocketRadius = ob.empty_display_size
                    
                    WritePocket(file, worldLocation, pocketValue, pocketRadius)

                #spawn area
                elif ob.empty_display_type == 'CUBE':

                    halfWidth = ob.empty_display_size * ob.scale[0]
                    halfHeight = ob.empty_display_size * ob.scale[1]

                    WriteSpawn(file, worldLocation, halfWidth, halfHeight)




        file.write("}")
        file.close()
        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(ExportInfo.bl_idname, text="Billiard Table Data (.table)")


def register():
    bpy.utils.register_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)
