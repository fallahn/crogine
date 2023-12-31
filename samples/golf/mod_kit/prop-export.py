bl_info = {
    "name": "Export golf hole data",
    "author": "Bald Guy",
    "version": (2023, 12, 31),
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

def WriteAIPath(file, path):
    file.write("\n    path ai\n    {\n")
    for p in path.data.splines.active.points:
        worldP = path.matrix_world @ p.co
        file.write("        point = %f,%f,%f\n" % (worldP.x, worldP.z, -worldP.y))
    file.write("\n    }\n")


def WritePath(file, path):
    file.write("\n        path\n        {\n")
    for p in path.data.splines.active.points:
        worldP = path.matrix_world @ p.co
        file.write("            point = %f,%f,%f\n" % (worldP.x, worldP.z, -worldP.y))

    if path.get('loop') is not None:
        if path['loop'] == 0:
            file.write("\n            loop = false")
        else:
            file.write("\n            loop = true")
    else:
        file.write("\n            loop = false")


    if path.get('delay') is not None:
        file.write("\n            delay = %f" % path['delay'])
    else:
        file.write("\n            delay = 4.0")


    if path.get('speed') is not None:
        file.write("\n            speed = %f" % path['speed'])
    else:
        file.write("\n            speed = 6.0")        

    file.write("\n        }\n")


def WriteSpeaker(file, speaker):
    if speaker.get('name') is not None:
        file.write("        emitter = \"%s\"\n" % speaker['name'])

    if speaker.parent is None:
        location = speaker.location
        file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))


def WriteSpeakerSolo(file, speaker):
    file.write("    speaker\n    {\n")
    WriteSpeaker(file, speaker)
    file.write("    }\n\n")


def WriteProp(file, modelName, location, rotation, scale, ob):
    file.write("    prop\n    {\n")
    file.write("        model = \"%s\"\n" % modelName)
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("        rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))
    file.write("        scale = %f,%f,%f\n" % (scale[0], scale[2], scale[1]))


    if ob.parent is not None and ob.parent.type == 'CURVE':
        WritePath(file, ob.parent)

    for child in ob.children:
        if child.type == 'SPEAKER':
            WriteSpeaker(file, child)
        elif child.type == 'EMPTY':
            if child.get('type') is not None and child['type'] == 1:
                if child.get('path') is not None:
                    file.write("        particles = \"%s\"\n" % child['path'])
                else:
                    file.write("        particles = \"path_is_missing\"\n")


    file.write("    }\n\n")


def WriteCrowd(file, location, rotation, ob):
    file.write("    crowd\n    {\n")
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("        rotation = %f\n" % (rotation[2] * (180.0 / 3.141)))


    if ob.get('look_at') is not None:
        file.write("        lookat = %f,%f,%f\n" % (ob['look_at'][0], ob['look_at'][2], -ob['look_at'][1]))


    if ob.parent is not None and ob.parent.type == 'CURVE':
        WritePath(file, ob.parent)

    file.write("    }\n\n")


def WriteParticles(file, path, location):
    file.write("    particles\n    {\n")
    file.write("        path = \"%s\"\n" % path)
    file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
    file.write("    }\n\n")


def WriteLight(file, ob):
    light = ob.data
    if light.type == 'POINT':
        location = ob.location
        colour = light.color
        
        file.write("    light\n    {\n")
        file.write("        position = %f,%f,%f\n" % (location[0], location[2], -location[1]))
        file.write("        colour = %f,%f,%f,1.0\n" % (colour.r, colour.g, colour.b))
        file.write("        radius = %f\n" % light.shadow_soft_size)

        if ob.get('animation') is not None:
            file.write("        animation = \"%s\"\n" % ob['animation'])

        if ob.get('preset') is not None:
            file.write("        preset = \"%s\"\n" % ob['preset'])

        file.write("    }\n\n")




def showMessageBox(message = "", title = "Message Box", icon = 'INFO'):

    def draw(self, context):
        self.layout.label(text = message)

    bpy.context.window_manager.popup_menu(draw, title = title, icon = icon)


class ExportInfo(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    '''Export object position and rotation info'''
    bl_idname = "export.golf"
    bl_label = 'Export Hole Info'
    filename_ext = ".hole"

    def execute(self, context):
        file = open(self.properties.filepath, 'w')

        scene = bpy.context.scene

        file.write("hole %s\n{\n" % Path(self.properties.filepath).stem)

        if scene.get('map_path') is not None:
            file.write("    map = \"%s/%s.png\"\n" % (scene['map_path'], Path(self.properties.filepath).stem))
        else:
            file.write("    map = \"assets/golf/courses/course_0/%s.png\"\n" % Path(self.properties.filepath).stem)

        if scene.get('model_path') is not None:
            file.write("    model = \"%s\"\n" % scene['model_path'])
        else:
            file.write("    model = \"assets/golf/models/course_0/%s.cmt\"\n" % Path(self.properties.filepath).stem)
        
        if scene.get('par') is not None:
            file.write("    par = %d\n\n" % scene['par'])
        else:
            file.write("    par = 4\n\n")

        teeWritten = False
        pinWritten = False
        targetWritten = False
        subtargetWritten = False

        for ob in bpy.context.selected_objects:

            if ob.type == 'MESH' or ob.type == 'EMPTY' or ob.type == 'SPEAKER' or ob.type == 'CURVE':

                modelName = ob.name
                if ob.get('model_path') is not None:
                    modelName = ob['model_path']

                worldLocation = ob.location
                worldRotation = ob.rotation_euler
                worldScale = ob.scale

                if ob.parent is not None:
                    if ob.parent.type == 'MESH' or ob.parent.type == 'ARMATURE':
                        worldLocation = ob.matrix_world @ ob.location
                        worldRotation = ob.matrix_world.to_euler('XYZ')
                        worldScale = vecMultiply(ob.parent.scale, ob.scale)


                if "crowd" in modelName.lower() and ob.type == 'MESH':
                    WriteCrowd(file, worldLocation, worldRotation, ob)
                elif "pin" in modelName.lower():
                    if pinWritten == False:
                        WriteProperty(file, "pin", worldLocation)
                        pinWritten = True
                    else:
                        self.report({'WARNING'}, "Multiple pins selected")
                elif "target" == modelName.lower():
                    if targetWritten == False:
                        WriteProperty(file, "target", worldLocation)
                        targetWritten = True
                    else:
                        self.report({'WARNING'}, "Multiple targets selected")

                elif "subtarget" == modelName.lower():
                    if subtargetWritten == False:
                        WriteProperty(file, "subtarget", worldLocation)
                        subtargetWritten = True
                    else:
                        self.report({'WARNING'}, "Multiple sub-targets selected")


                elif "tee" in modelName.lower():
                    if teeWritten == False:
                        WriteProperty(file, "tee", worldLocation)
                        teeWritten = True
                    else:
                        self.report({'WARNING'}, "Multiple tees selected")
                elif ob.type == 'CURVE' and "ai" in modelName.lower():
                    WriteAIPath(file, ob)

                else:
                    if ob.type == 'MESH':
                        WriteProp(file, modelName, worldLocation, worldRotation, worldScale, ob)
                    elif ob.type == 'EMPTY':
                        if ob.get('type') is not None:
                            if ob['type'] == 1 and ob.parent is None:
                            # is a particle emitter not parented to a prop
                                if ob.get('path') is not None:
                                    WriteParticles(file, ob['path'], worldLocation)
                                else:
                                    WriteParticles(file, "path_missing", worldLocation)
                    elif ob.type == 'SPEAKER' and ob.parent is None:
                        WriteSpeakerSolo(file, ob)

            elif ob.type == 'LIGHT':
                WriteLight(file, ob)
        

        file.write("}")
        file.close()


        if not pinWritten or not targetWritten or not teeWritten:
            showMessageBox("Pin or other Tee data was missing from selection", "Warning", 'ERROR')


        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(ExportInfo.bl_idname, text="Golf Hole Data (.hole)")


def register():
    bpy.utils.register_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ExportInfo)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)
