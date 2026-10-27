#version 450 core

layout(location = 0) in vec3 world_position;
layout(location = 1) in vec3 world_normal;
layout(location = 2) in vec3 vertex_color;

layout(location = 0) out vec4 out_color;

layout(std140, set = 0, binding = 0) uniform SceneUniforms {
    mat4 projection;
    mat4 view;
    vec4 camera_position;
    vec4 ambient_color_intensity;
    vec4 directional_direction;
    vec4 directional_color_intensity;
    vec4 light_counts;
} scene;

struct PointLight {
    vec4 position;
    vec4 color_intensity;
};

struct SpotLight {
    vec4 position;
    vec4 direction;
    vec4 color_intensity;
    vec4 cone; 
};

layout(std430, set = 0, binding = 1) readonly buffer PointLightBuffer {
    PointLight lights[];
} point_buffer;

layout(std430, set = 0, binding = 2) readonly buffer SpotLightBuffer {
    SpotLight lights[];
} spot_buffer;

layout(std140, set = 1, binding = 0) uniform ModelUniform {
    mat4 model;
    vec4 albedo;
    vec4 specular_color;
    vec4 material_params; 
} material;


vec3 calculateBlinnPhong(
    vec3 normal,
    vec3 light_direction,
    vec3 view_direction,
    vec3 light_color,
    float intensity)
{
    float diffuse_factor = max(dot(normal, light_direction), 0.0);

    vec3 halfway_direction = normalize(light_direction + view_direction);
    float specular_factor = 0.0;
    if (diffuse_factor > 0.0) {
        specular_factor = pow(
            max(dot(normal, halfway_direction), 0.0),
            max(material.material_params.x, 1.0));
    }

    vec3 diffuse = material.albedo.rgb * vertex_color * diffuse_factor;
    vec3 specular = material.specular_color.rgb * specular_factor;
    return (diffuse + specular) * light_color * intensity;
}

void main() {
    
    if (material.material_params.y > 0.5) {
        out_color = vec4(material.albedo.rgb, 1.0);
        return;
    }

    vec3 normal = normalize(world_normal);
    vec3 view_direction = normalize(scene.camera_position.xyz - world_position);

    vec3 result = material.albedo.rgb * vertex_color *
        scene.ambient_color_intensity.rgb * scene.ambient_color_intensity.a;

    
    vec3 directional_light_direction = normalize(-scene.directional_direction.xyz);
    result += calculateBlinnPhong(
        normal,
        directional_light_direction,
        view_direction,
        scene.directional_color_intensity.rgb,
        scene.directional_color_intensity.a);

    uint point_count = uint(scene.light_counts.x);
    uint spot_count = uint(scene.light_counts.y);

    
    for (uint i = 0; i < point_count; ++i) {
        vec3 delta = point_buffer.lights[i].position.xyz - world_position;
        float distance_squared = max(dot(delta, delta), 0.01);
        vec3 light_direction = delta * inversesqrt(distance_squared);
        float intensity = point_buffer.lights[i].color_intensity.a / distance_squared;

        result += calculateBlinnPhong(
            normal,
            light_direction,
            view_direction,
            point_buffer.lights[i].color_intensity.rgb,
            intensity);
    }

    
    for (uint i = 0; i < spot_count; ++i) {
        SpotLight light = spot_buffer.lights[i];
        vec3 delta = light.position.xyz - world_position;
        float distance_squared = max(dot(delta, delta), 0.01);
        vec3 light_direction = delta * inversesqrt(distance_squared);

        float angle_factor = dot(normalize(-light_direction), normalize(light.direction.xyz));
        float cone_factor = smoothstep(light.cone.y, light.cone.x, angle_factor);
        float intensity = light.color_intensity.a * cone_factor / distance_squared;

        result += calculateBlinnPhong(
            normal,
            light_direction,
            view_direction,
            light.color_intensity.rgb,
            intensity);
    }

    out_color = vec4(result, 1.0);
}
