#version 450 core

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec3 in_color;

layout(location = 0) out vec3 world_position;
layout(location = 1) out vec3 world_normal;
layout(location = 2) out vec3 vertex_color;

layout(std140, set = 0, binding = 0) uniform SceneUniforms {
    mat4 projection;
    mat4 view;
    vec4 camera_position;
    vec4 ambient_color_intensity;
    vec4 directional_direction;
    vec4 directional_color_intensity;
    vec4 light_counts;
} scene;

layout(std140, set = 1, binding = 0) uniform ModelUniform {
    mat4 model;
    vec4 albedo;
    vec4 specular_color;
    vec4 material_params;
} material;

void main() {
    vec4 world = material.model * vec4(in_position, 1.0);

    world_position = world.xyz;
    world_normal = normalize(transpose(inverse(mat3(material.model))) * in_normal);
    vertex_color = in_color;

    gl_Position = scene.projection * scene.view * world;
}
