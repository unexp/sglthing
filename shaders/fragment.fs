#version 330 core
#define PI 3.1415926538

out vec4 FragColor;

uniform vec3 camera_position;
uniform float time;
uniform float fog_maxdist;
uniform float fog_mindist;
uniform vec4 fog_color;
uniform int banding_effect;

in vec3 f_pos;
in vec3 f_normal;
in vec4 f_color;
in vec2 f_uv;
in float f_affine;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform sampler2D texture4;
uniform sampler2D texture5;
uniform sampler2D texture6;

void main()
{   
    // calculate diffuse lighting
    vec3 norm = normalize(f_normal);
    vec3 light_dir = -normalize(vec3(1.,-0.75,-0.5)*100000.0 - f_pos);
    float diff = max(dot(norm, light_dir),0.1);
    vec3 diffuse = diff * vec3(0.77,0.8,0.9);

    // calculate specular lighting
    vec3 view_dir = normalize(camera_position - f_pos);
    vec3 reflect_dir = reflect(-light_dir, norm);  
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 4);
    vec3 specular = 0.5 * spec * vec3(0.9,0.95,1.0);  

    ivec2 coord = ivec2(gl_FragCoord.xy - 0.5);

    // affine thing
    vec2 affine_tex_coords = f_uv / f_affine;

    vec4 out_color = texture(texture1, affine_tex_coords);

    // color banding effect
    vec4 out_color_raw = out_color;
    out_color_raw *= 255.0;
    ivec4 i_out_color_raw = ivec4(out_color_raw);
    i_out_color_raw.r = i_out_color_raw.r & banding_effect;
    i_out_color_raw.g = i_out_color_raw.g & banding_effect;
    i_out_color_raw.b = i_out_color_raw.b & banding_effect;
    out_color_raw = vec4(i_out_color_raw);
    out_color_raw /= 255.0;
    out_color = out_color_raw;

    // lighting
    out_color *= vec4(diffuse + specular,1.0);
    
    // fog
    float dist = distance(camera_position.xz, f_pos.xz) + f_pos.y;
    float fog_factor = (fog_maxdist - dist) /
                    (fog_maxdist - fog_mindist);
    fog_factor = clamp(fog_factor, 0.0, 1.0);

    out_color = mix(fog_color, out_color, fog_factor);

    FragColor = vec4(out_color.xyz, fog_factor);
}
