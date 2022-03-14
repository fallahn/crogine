uniform sampler2D u_texture; //input scene texture
uniform float u_time = 1.0; //optional - increases perpetually, in seconds
uniform vec2 u_scale = vec2(1.0); //optional - contains the current pixel scale
uniform vec2 u_resolution = vec2(1.0); //optional - contains the current window resolution

in vec2 v_texCoord; //texture coordinates for the input texture
in vec4 v_colour; //vertex colours

out vec4 FRAG_OUT;

void main()
{
    vec4 colour = TEXTURE(u_texture, v_texCoord) * v_colour;

    //do fancy processing to colour here.

    FRAG_OUT = vec4(vec3(1.0) - colour.rgb, colour.a); 
}