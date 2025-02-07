#version 330 core

layout(location=0) in vec2 v_pos;
layout(location=1) in vec2 v_uv;

out vec2 f_uv;
uniform float waviness;
uniform float time;
uniform float depth;
uniform mat4 projection;

void main()
{
    f_uv = v_uv;

    vec2 moved_pos = v_pos;
    
    moved_pos = floor(moved_pos);

    moved_pos.x += sin((v_pos.x+v_pos.y+time*1.1))*waviness;
    moved_pos.y += cos((v_pos.x+v_pos.y+time*1.1))*waviness;

    if(depth < 0.1)
        gl_Position = vec4(0,0,0,0);
        
    gl_Position = projection * vec4(moved_pos.xy, depth, 1.0);
}