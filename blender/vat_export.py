bl_info = {
    "name": "Export Vertex Animation Textures",
    "author": "Bald Guy (Based on Martin Donald's example)",
    "version": (2022, 5, 3),
    "blender": (2, 93, 0),
    "location": "File > Export > Vertex Animation Texture",
    "description": "Export active animation on selected object as texture information.",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Import-Export"}

import os, struct, math
import bpy
import bmesh
import mathutils
import bpy_extras
from pathlib import Path

def export_mesh(obj, path, name):
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    outpath = path + name + "_mesh.gltf"

    bpy.ops.export_scene.gltf(use_selection = True, filepath = outpath)

#writes UV coord for each vertex into the generated texture
#and stores in a new UV channel on the given object
def insert_UVs(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)

    uv_layer = bm.loops.layers.uv.new("VATs UV")

    pixel_width = 1.0 / len(bm.verts)

    i = 0
    for v in bm.verts:
        for l in v.link_loops:
            uv_data = l[uv_layer]
            uv_data.uv = mathutils.Vector((i * pixel_width, 0.0))
            #TODO we can add the half pixel UV offset here
        i += 1

    bm.to_mesh(obj.data)


def write_image(pixels, filename, size, path):
    image = bpy.data.images.new(filename, width = size[0], height = size[1], is_data = True) #float_buffer = True)
    #bpy.context.scene.render.image_settings.color_depth = 16
    image.pixels = pixels
    image.save_render(path + filename + ".png", scene = bpy.context.scene)


def unsign_vector(vec, yUp):

    if yUp == True:
        temp = vec[2]
        vec[2] = -vec[1]
        vec[1] = temp

    vec += mathutils.Vector((1.0, 1.0, 1.0))
    vec /= 2.0

    return list(vec.to_tuple())



def data_from_frame(obj, scale, yUp):
    obj.data.calc_tangents()
    vertex_data = [None] * len(obj.data.vertices)


    for face in obj.data.polygons:
        for vert in [obj.data.loops[i] for i in face.loop_indices]:
            index = vert.vertex_index

            #updates the position if the animation includes transforms
            position = obj.data.vertices[index].co.copy()
            position = obj.matrix_world @ position

            position = unsign_vector(position / scale, yUp)
            normal = unsign_vector(vert.normal.copy(), yUp)
            tangent = unsign_vector(vert.tangent.copy(), yUp)

            #pack out to 4 float for pixel
            position.append(1.0)
            normal.append(1.0)
            tangent.append(1.0)

            vertex_data[index] = [position, normal, tangent]

    return vertex_data



def object_from_frame(obj, frame):
    scene = bpy.context.scene
    scene.frame_set(frame)

    depsgraph = bpy.context.view_layer.depsgraph
    eval_obj = obj.evaluated_get(depsgraph)
    #retval = bpy.data.objects.new('frame_0', bpy.data.meshes.new_from_object(eval_obj))
    retval = bpy.data.objects.new('frame_0', bpy.data.meshes.new_from_object(eval_obj, preserve_all_data_layers=True, depsgraph=depsgraph))

    retval.matrix_world = obj.matrix_world

    return retval


def export_textures(obj, frame_range, scale, path, yUp):
    filename = Path(path).stem
    #filepath = bpy.path.dirname(bpy.path.abspath(path))
    filepath = os.path.dirname(os.path.abspath(path))
    filepath += "/"


    positions = list()
    normals = list()
    tangents = list()
    width = 0

    for i in range(frame_range[1] - frame_range[0]):
        frame = frame_range[0] + i
        curr_obj = object_from_frame(obj, frame)
        pixel_data = data_from_frame(curr_obj, scale, yUp)
        width = len(pixel_data)

        for pixel in pixel_data:
            positions += pixel[0]
            normals += pixel[1]
            tangents += pixel[2]

    height = frame_range[1] - frame_range[0]

    write_image(positions, filename + '_position', [width, height], filepath)
    write_image(normals, filename + '_normal', [width, height], filepath)
    write_image(tangents, filename + '_tangent', [width, height], filepath)

    base_frame = object_from_frame(obj, 0)
    insert_UVs(base_frame)
    export_mesh(base_frame, filepath, filename)


    #tidy up temp object
    bpy.data.objects.remove(base_frame)


class ExportVat(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    bl_idname = "export.vats"
    bl_label = 'Export Vertex Animation Texture'
    filename_ext = ".png"
    position_scale: bpy.props.FloatProperty(name = "Scale", description = "Maximum range of movement", default=1.0, min=0.0001)
    yUp: bpy.props.BoolProperty(name = "Y-Up", description = "Export with Y coordinates upwards", default = True)

    def execute(self, context):

        frame_0 = context.scene.frame_start
        frame_1 = context.scene.frame_end

        #not sure how min/max work in bpy so let's do it old school
        if frame_1 < frame_0:
            frame_1 = frame_0


        if bpy.context.selected_objects != None:
            obj = bpy.context.selected_objects[0]
            export_textures(obj, [frame_0, frame_1], self.position_scale, self.properties.filepath, self.yUp)


        return {'FINISHED'}


def menu_func(self, context):
    self.layout.operator(ExportVat.bl_idname, text="Vertex Animation Texture (.png)")


def register():
    bpy.utils.register_class(ExportVat)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ExportVat)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)

if __name__ == "__main__":
    register()