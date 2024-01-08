bl_info = {
    "name": "Export Vertex Animation Textures",
    "author": "Bald Guy (Based on Martin Donald's example)",
    "version": (2023, 10, 15),
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
import struct
from pathlib import Path

def export_mesh(obj, path, name, settings):

    bpy.ops.object.select_all(action='DESELECT')

    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.context.view_layer.update()
    outpath = path + name + ".gltf"

    bpy.ops.export_scene.gltf(
        use_selection = True, 
        filepath = outpath, 
        export_texcoords = True, 
        export_yup = settings.yUp, 
        export_colors = settings.colours, 
        export_tangents = settings.tangents, 
        will_save_settings = settings.save_settings,
        check_existing = True,
        export_apply = settings.modifiers)



#writes UV coord for each vertex into the generated texture
#and stores in a new UV channel on the given object - in 
#other words UV stores the regular texture coords, UV1 stores
#the coords of the current vertex position/normal in the VAT
#texture
def insert_UVs(obj, scale):
    bm = bmesh.new()
    bm.from_mesh(obj.data)

    uv_layer = bm.loops.layers.uv.new("VATs UV")

    pixel_width = 1.0 / len(bm.verts)

    i = 0
    for v in bm.verts:
        for l in v.link_loops:
            uv_data = l[uv_layer]
            uv_data.uv = mathutils.Vector((i * pixel_width, scale))
            uv_data.select = True
            
            #horizontally offset by half a pixel so we'll sample the centre in the shader
            uv_data.uv[0] += (pixel_width / 2)

        i += 1

    bm.to_mesh(obj.data)


def write_image(pixels, filename, size, path, settings):
    image = bpy.data.images.new(filename, width = size[0], height = size[1], is_data = True, float_buffer = settings.float_texture)

    if settings.float_texture:
        bpy.context.scene.render.image_settings.color_depth = '16'
    else:
        bpy.context.scene.render.image_settings.color_depth = '8'

    image.pixels = pixels
    image.save_render(path + filename + ".png", scene = bpy.context.scene)
    bpy.data.images.remove(image)


def write_bin(pixels, filename, path):
    fileBytes = bytearray(struct.pack('%sf' % len(pixels), *pixels))
    file = open(path + filename + ".bin", "wb")
    file.write(fileBytes)
    file.close()

def unsign_vector(vec, yUp):

    if yUp == True:
        temp = vec[2]
        vec[2] = -vec[1]
        vec[1] = temp

    vec += mathutils.Vector((1.0, 1.0, 1.0))
    vec /= 2.0

    return list(vec.to_tuple())



def data_from_frame(obj, settings):


    if settings.tangents:
        obj.data.calc_tangents()
    else:
        obj.data.calc_normals_split()


    vertex_data = [None] * len(obj.data.vertices)


    for face in obj.data.polygons:
        for vert in [obj.data.loops[i] for i in face.loop_indices]:
            index = vert.vertex_index

            #updates the position if the animation includes transforms
            position = obj.data.vertices[index].co.copy()
            position = obj.matrix_world @ position

            #dividing by the scale normalises the vector and sets the
            #max range, which is then rescaled in the shader. We might
            #not need this when writing binaries as we can load the result
            #directly into a floating point texture in-engine
            position = unsign_vector(position / settings.scale, settings.yUp)
            position.append(1.0)

            normal = unsign_vector(vert.normal.copy(), settings.yUp)
            normal.append(1.0)            

            tangent = list((1.0, 1.0, 1.0))

            if settings.tangents:
                tangent = unsign_vector(vert.tangent.copy(), settings.yUp)

            tangent.append(1.0)

            vertex_data[index] = [position, normal, tangent]

    return vertex_data



def object_from_frame(obj, frame):
    scene = bpy.context.scene
    scene.frame_set(frame)

    depsgraph = bpy.context.view_layer.depsgraph
    eval_obj = obj.evaluated_get(depsgraph)
    retval = bpy.data.objects.new('frame_0', bpy.data.meshes.new_from_object(eval_obj, preserve_all_data_layers = True, depsgraph = depsgraph))


    retval.matrix_world = obj.matrix_world
    retval.data.use_auto_smooth = obj.data.use_auto_smooth
    retval.data.auto_smooth_angle = obj.data.auto_smooth_angle   

    return retval



def export_textures(obj, frame_range, path, settings):
    filename = Path(path).stem
    filepath = os.path.dirname(os.path.abspath(path))
    filepath += "/"


    positions = list()
    normals = list()
    tangents = list()
    width = 0

    frame_count = frame_range[1] - frame_range[0]
    frame_count //= settings.frame_skip

    for i in range(frame_count):
        frame = frame_range[0] + (i * settings.frame_skip)
        curr_obj = object_from_frame(obj, frame)
        pixel_data = data_from_frame(curr_obj, settings)
        width = len(pixel_data)

        for pixel in pixel_data:
            positions += pixel[0]
            normals += pixel[1]

            if settings.tangents:
                tangents += pixel[2]

    height = frame_count

    write_image(positions, filename + '_position', [width, height], filepath, settings)
    write_image(normals, filename + '_normal', [width, height], filepath, settings)

    #TODO decide if there's no need to compress/normalise the positions when storing
    #the data as aa binary file rather than an image
    write_bin(positions, filename + '_position', filepath)
    write_bin(normals, filename + '_normal', filepath)

    if settings.tangents == True:
        write_image(tangents, filename + '_tangent', [width, height], filepath, settings)
        write_bin(tangents, filename + '_tangent', filepath)

    base_frame = object_from_frame(obj, 0)
    insert_UVs(base_frame, settings.scale)
    export_mesh(base_frame, filepath, filename, settings)


    #write some meta-data for engine loading
    file = open(filepath + filename + ".vat", 'w')
    file.write("vat %s\n{\n" % filename)
    file.write("    model = \"%s.cmt\"\n" % filename)
    file.write("    width = %f\n" % width)
    file.write("    height = %f\n" % height)
    file.write("    scale = %f\n" % settings.scale)
    file.write("    frame_count = %d\n" % frame_count)
    file.write("    frame_rate = %f\n" % (bpy.context.scene.render.fps / settings.frame_skip))
    file.write("    position = \"%s_position.png\"\n" % filename)
    file.write("    normal = \"%s_normal.png\"\n" % filename)
    if settings.tangents:
        file.write("    tangent = \"%s_tangent.png\"\n" % filename)
    file.write("}")
    file.close()


    #tidy up temp object
    bpy.data.objects.remove(base_frame)

    #restore original selection
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def show_message_box(message = "", title = "Message Box", icon = 'INFO'):

    def draw(self, context):
        self.layout.label(text = message)

    bpy.context.window_manager.popup_menu(draw, title = title, icon = icon)


class ExportSettings:
    def __init__(self):
        self.yUp = True
        self.tangents = False
        self.colours = True
        self.save_settings = True
        self.modifiers = True
        self.frame_skip = 1
        self.scale = 1
        self.float_texture = False


class ExportVat(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    '''Export glTF with vertex animation data stored in RGBA textures'''
    bl_idname = "export.vats"
    bl_label = 'Export Vertex Animation Texture'
    bl_options = {'PRESET'}
    filename_ext = ".glb"
    position_scale: bpy.props.FloatProperty(name = "Scale", description = "Maximum range of movement", default = 4.0, min = 0.0001)
    frame_skip: bpy.props.IntProperty(name = "Frame Skip", description = "Render every Nth frame of animation", default = 1, min = 1)    
    yUp: bpy.props.BoolProperty(name = "Y-Up", description = "Export with Y coordinates upwards", default = True)
    export_tangents: bpy.props.BoolProperty(name = "Export Tangents", description = "Include tangent data", default = False)
    export_colour: bpy.props.BoolProperty(name = "Export Vertex Colours", description = "Include vertex colour data", default = True)
    float_texture: bpy.props.BoolProperty(name = "Floating Point Textures", description = "Save textures as 16 bit floating point", default = False)
    apply_modifiers: bpy.props.BoolProperty(name = "Apply Modifiers", description = "Apply modifiers (except armatures) when exporting", default = True)


    def execute(self, context):

        if self.export_tangents and not context.object.data.uv_layers:
            self.report({'ERROR'}, "No UV map found")
            show_message_box("No UV map found, cannot export tangent data.", "Error", 'ERROR')
        else:

            if context.mode != 'OBJECT':
                bpy.ops.object.mode_set(mode = 'OBJECT')

            frame_0 = context.scene.frame_start
            frame_1 = context.scene.frame_end


            settings = ExportSettings()
            settings.yUp = self.yUp
            settings.tangents = self.export_tangents
            settings.colours = self.export_colour
            settings.save_settings = False #self.save_settings
            settings.modifiers = self.apply_modifiers
            settings.frame_skip = self.frame_skip
            settings.scale = self.position_scale
            settings.float_texture = self.float_texture


            current_frame = context.scene.frame_current

            if bpy.context.selected_objects != None:
                obj = bpy.context.selected_objects[0]
                if len(obj.data.vertices) > 4096:
                    show_message_box("More than 4096 vertices found, this would create a HUGE texture.", "Error", 'ERROR')
                else:
                    export_textures(obj, [frame_0, frame_1], self.properties.filepath, settings)
            context.scene.frame_set(current_frame)


        return {'FINISHED'}


    def check(self, context):
        filepath = bpy.path.ensure_ext(self.filepath, '.glb')
        if filepath != self.filepath:
            self.filepath = filepath
            return True
        return False        


def menu_func(self, context):
    self.layout.operator(ExportVat.bl_idname, text = "glTF with VAT (.glb)")


def register():
    bpy.utils.register_class(ExportVat)
    bpy.types.TOPBAR_MT_file_export.append(menu_func)

def unregister():
    bpy.utils.unregister_class(ExportVat)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func)

if __name__ == "__main__":
    register()