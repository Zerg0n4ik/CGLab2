#include "application.hpp"
#include "math.hpp"

#include <imgui.h>

#include <cmath>
#include <cstring>
#include <iostream>

namespace {

    using namespace graphics::internal;

    constexpr float PI = 3.14159265358979323846f;

    
    struct ObjectState {
        float position[3] = { 0.0f, 0.0f, 0.0f };
        float rotation[3] = { 0.0f, 0.0f, 0.0f };
        float scale[3] = { 1.0f, 1.0f, 1.0f };
        float color[3] = { 1.0f, 1.0f, 1.0f };
        float orbit_radius = 2.0f;
        float orbit_height = 0.5f;
    };

    ObjectState object;

    
    float camera_position[3] = { 0.0f, 0.0f, 5.0f };
    float camera_yaw = -90.0f;
    float camera_pitch = 0.0f;
    float camera_fov = 60.0f;
    float ortho_half_height = 3.5f;
    bool use_perspective = true;

    
    bool animate = true;
    float anim_speed = 4.0f;
    float anim_time = 0.0f;
    double last_time = 0.0;

    
    float specular_color[3] = { 1.0f, 1.0f, 1.0f };
    float shininess = 48.0f;

    float ambient_color[3] = { 0.12f, 0.12f, 0.15f };
    float ambient_intensity = 1.0f;

    float directional_direction[3] = { -0.4f, -1.0f, -0.3f };
    float directional_color[3] = { 1.0f, 0.95f, 0.85f };
    float directional_intensity = 1.0f;

    struct PointLight {
        float position[3];
        float color[3];
        float intensity;
    };

    struct SpotLight {
        float position[3];
        float direction[3];
        float color[3];
        float intensity;
        float inner_angle;
        float outer_angle;
    };

    PointLight point_lights[MAX_POINT_LIGHTS] = {
        { { -2.0f, 1.0f, 1.0f }, { 1.0f, 0.25f, 0.15f }, 8.0f },
        { {  2.0f, 1.0f, 1.0f }, { 0.2f, 0.45f, 1.0f }, 8.0f },
    };

    SpotLight spot_lights[MAX_SPOT_LIGHTS] = {
        { { 0.0f, 2.5f, 2.0f }, { 0.0f, -1.0f, -0.6f },
          { 0.3f, 1.0f, 0.4f }, 12.0f, 18.0f, 30.0f },
    };

    uint32_t point_count = 2;
    uint32_t spot_count = 1;
    bool show_light_visualizers = true;

    void normalize3(float vector[3]) {
        const float length = std::sqrt(
            vector[0] * vector[0] +
            vector[1] * vector[1] +
            vector[2] * vector[2]);

        if (length > 0.000001f) {
            vector[0] /= length;
            vector[1] /= length;
            vector[2] /= length;
        }
        else {
            
            vector[0] = 0.0f;
            vector[1] = 0.0f;
            vector[2] = -1.0f;
        }
    }

    void cross3(const float a[3], const float b[3], float result[3]) {
        result[0] = a[1] * b[2] - a[2] * b[1];
        result[1] = a[2] * b[0] - a[0] * b[2];
        result[2] = a[0] * b[1] - a[1] * b[0];
    }

    
    void lookAt(const float eye[3], const float direction[3], math::Mat4 view) {
        float forward[3] = { direction[0], direction[1], direction[2] };
        normalize3(forward);

        float up[3] = { 0.0f, 1.0f, 0.0f };
        if (std::fabs(forward[1]) > 0.98f) {
            up[0] = 1.0f;
            up[1] = 0.0f;
            up[2] = 0.0f;
        }

        float side[3]{};
        cross3(forward, up, side);
        normalize3(side);

        float corrected_up[3]{};
        cross3(side, forward, corrected_up);

        math::identity(view);

        view[0][0] = side[0];
        view[1][0] = side[1];
        view[2][0] = side[2];
        view[3][0] = -(side[0] * eye[0] + side[1] * eye[1] + side[2] * eye[2]);

        view[0][1] = corrected_up[0];
        view[1][1] = corrected_up[1];
        view[2][1] = corrected_up[2];
        view[3][1] = -(corrected_up[0] * eye[0] + corrected_up[1] * eye[1] + corrected_up[2] * eye[2]);

        view[0][2] = -forward[0];
        view[1][2] = -forward[1];
        view[2][2] = -forward[2];
        view[3][2] = forward[0] * eye[0] + forward[1] * eye[1] + forward[2] * eye[2];
    }

    
    
    void writeMarkerModel(ModelUniform* uniform, const float position[3],
                          float radius, const float color[3]) {
        math::Mat4 translation{};
        math::Mat4 scale{};
        math::Mat4 model{};

        math::translate(position[0], position[1], position[2], translation);
        math::scale(radius, radius, radius, scale);
        math::multiply(translation, scale, model);
        std::memcpy(uniform->model, model, sizeof(model));

        for (int i = 0; i < 3; ++i) {
            uniform->albedo[i] = color[i];
            uniform->specular_color[i] = 1.0f;
        }

        uniform->albedo[3] = 1.0f;
        uniform->specular_color[3] = 1.0f;
        uniform->material_params[0] = 32.0f;
        uniform->material_params[1] = 1.0f; 
        uniform->material_params[2] = 0.0f;
        uniform->material_params[3] = 0.0f;
    }

    
    
    void writeRodModel(ModelUniform* uniform, const float a[3], const float b[3],
                       const float color[3]) {
        float axis_z[3] = {
            b[0] - a[0],
            b[1] - a[1],
            b[2] - a[2],
        };

        const float length = std::sqrt(
            axis_z[0] * axis_z[0] +
            axis_z[1] * axis_z[1] +
            axis_z[2] * axis_z[2]);

        if (length < 0.00001f) {
            writeMarkerModel(uniform, a, 0.001f, color);
            return;
        }

        axis_z[0] /= length;
        axis_z[1] /= length;
        axis_z[2] /= length;

        float reference[3] = { 0.0f, 1.0f, 0.0f };
        if (std::fabs(axis_z[1]) > 0.95f) {
            reference[0] = 1.0f;
            reference[1] = 0.0f;
            reference[2] = 0.0f;
        }

        float axis_x[3]{};
        cross3(reference, axis_z, axis_x);
        normalize3(axis_x);

        float axis_y[3]{};
        cross3(axis_z, axis_x, axis_y);

        math::identity(uniform->model);
        for (int row = 0; row < 3; ++row) {
            uniform->model[0][row] = axis_x[row] * 0.008f;
            uniform->model[1][row] = axis_y[row] * 0.008f;
            uniform->model[2][row] = axis_z[row] * (length * 0.5f);
            uniform->model[3][row] = (a[row] + b[row]) * 0.5f;
        }

        for (int i = 0; i < 3; ++i) {
            uniform->albedo[i] = color[i];
            uniform->specular_color[i] = 1.0f;
        }

        uniform->albedo[3] = 1.0f;
        uniform->specular_color[3] = 1.0f;
        uniform->material_params[0] = 32.0f;
        uniform->material_params[1] = 1.0f;
        uniform->material_params[2] = 0.0f;
        uniform->material_params[3] = 0.0f;
    }

    bool flushModelUniform(int model_index) {
        auto& context = graphics::internal::context;
        const VkResult result = vmaFlushAllocation(
            context.allocator,
            context.model_uniform_buffer_allocations[model_index],
            0,
            VK_WHOLE_SIZE);

        if (result != VK_SUCCESS) {
            std::cerr << "Failed to flush model uniform buffer #" << model_index << '\n';
            return false;
        }

        return true;
    }

} 

namespace application {

    bool initialize() {
        return true;
    }

    void shutdown() {
        auto& context = graphics::internal::context;
        vkQueueWaitIdle(context.graphics_queue);
    }

    void update(double time) {
        auto& context = graphics::internal::context;

        
        const float delta_time = static_cast<float>(time - last_time);
        last_time = time;
        if (animate) {
            anim_time += delta_time * anim_speed;
        }

        
        
        
        
        ImGui::Begin("Lab 2: Dodecahedron, Camera and Lighting");

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat3("Position##camera", camera_position, -10.0f, 10.0f);
            ImGui::SliderFloat("Yaw (deg)", &camera_yaw, -180.0f, 180.0f);
            ImGui::SliderFloat("Pitch (deg)", &camera_pitch, -89.0f, 89.0f);

            if (ImGui::RadioButton("Perspective", use_perspective)) {
                use_perspective = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Orthographic", !use_perspective)) {
                use_perspective = false;
            }

            if (use_perspective) {
                ImGui::SliderFloat("Field of view (deg)", &camera_fov, 25.0f, 110.0f);
            }
            else {
                ImGui::SliderFloat("Orthographic half-height", &ortho_half_height, 1.0f, 12.0f);
            }
        }

        if (ImGui::CollapsingHeader("Object (Laboratory 1)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Play animation", &animate);
            ImGui::SliderFloat("Animation speed", &anim_speed, 0.0f, 5.0f);
            ImGui::SliderFloat3("Position##object", object.position, -3.0f, 3.0f);
            ImGui::SliderFloat3("Rotation (deg)", object.rotation, -180.0f, 180.0f);
            ImGui::SliderFloat3("Scale", object.scale, 0.1f, 3.0f);
            ImGui::ColorEdit3("Object color / albedo", object.color);
            ImGui::SliderFloat("Orbit radius", &object.orbit_radius, 0.0f, 3.0f);
            ImGui::SliderFloat("Orbit height", &object.orbit_height, -1.0f, 1.0f);
        }

        if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Specular color", specular_color);
            ImGui::SliderFloat("Shininess", &shininess, 1.0f, 256.0f);
            ImGui::ColorEdit3("Ambient color", ambient_color);
            ImGui::SliderFloat("Ambient intensity", &ambient_intensity, 0.0f, 2.0f);
        }

        if (ImGui::CollapsingHeader("Directional light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat3("Direction", directional_direction, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color##directional", directional_color);
            ImGui::SliderFloat("Intensity##directional", &directional_intensity, 0.0f, 5.0f);
        }

        if (ImGui::CollapsingHeader("Point lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (point_count < MAX_POINT_LIGHTS && ImGui::Button("+ Add point light")) {
                point_lights[point_count] = {
                    { 0.0f, 2.0f, 2.0f }, { 1.0f, 1.0f, 1.0f }, 5.0f
                };
                ++point_count;
            }

            ImGui::SameLine();
            ImGui::Text("%u / %u", point_count, MAX_POINT_LIGHTS);

            for (uint32_t i = 0; i < point_count; ++i) {
                ImGui::PushID(1000 + static_cast<int>(i));
                if (ImGui::TreeNode("Point light")) {
                    ImGui::SliderFloat3("Position", point_lights[i].position, -6.0f, 6.0f);
                    ImGui::ColorEdit3("Color", point_lights[i].color);
                    ImGui::SliderFloat("Intensity", &point_lights[i].intensity, 0.0f, 50.0f);

                    if (ImGui::Button("Remove point light")) {
                        for (uint32_t j = i + 1; j < point_count; ++j) {
                            point_lights[j - 1] = point_lights[j];
                        }
                        --point_count;
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        if (ImGui::CollapsingHeader("Spot lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (spot_count < MAX_SPOT_LIGHTS && ImGui::Button("+ Add spotlight")) {
                spot_lights[spot_count] = {
                    { 0.0f, 3.0f, 2.0f }, { 0.0f, -1.0f, -1.0f },
                    { 1.0f, 1.0f, 1.0f }, 10.0f, 15.0f, 30.0f
                };
                ++spot_count;
            }

            ImGui::SameLine();
            ImGui::Text("%u / %u", spot_count, MAX_SPOT_LIGHTS);

            for (uint32_t i = 0; i < spot_count; ++i) {
                ImGui::PushID(2000 + static_cast<int>(i));
                if (ImGui::TreeNode("Spotlight")) {
                    SpotLight& light = spot_lights[i];
                    ImGui::SliderFloat3("Position", light.position, -6.0f, 6.0f);
                    ImGui::SliderFloat3("Direction", light.direction, -1.0f, 1.0f);
                    ImGui::ColorEdit3("Color", light.color);
                    ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 50.0f);
                    ImGui::SliderFloat("Inner angle (deg)", &light.inner_angle, 1.0f, 60.0f);
                    ImGui::SliderFloat("Outer angle (deg)", &light.outer_angle, 2.0f, 80.0f);

                    if (light.outer_angle < light.inner_angle) {
                        light.outer_angle = light.inner_angle;
                    }

                    if (ImGui::Button("Remove spotlight")) {
                        for (uint32_t j = i + 1; j < spot_count; ++j) {
                            spot_lights[j - 1] = spot_lights[j];
                        }
                        --spot_count;
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        ImGui::Checkbox("Show light visualizers", &show_light_visualizers);
        ImGui::TextWrapped("Point and spot lights use inverse-square attenuation. Spotlight edges are softened between the inner and outer angles.");
        ImGui::End();

        
        
        
        
        const float aspect = context.swapchain_extent.height > 0
            ? static_cast<float>(context.swapchain_extent.width) /
              static_cast<float>(context.swapchain_extent.height)
            : 1.0f;

        math::Mat4 projection{};
        math::Mat4 view{};

        if (use_perspective) {
            math::perspective(camera_fov * PI / 180.0f, aspect, 0.1f, 100.0f, projection);
        }
        else {
            const float height = ortho_half_height;
            math::ortho(-height * aspect, height * aspect, -height, height, 0.1f, 100.0f, projection);
        }
        projection[1][1] = -projection[1][1];

        const float yaw = camera_yaw * PI / 180.0f;
        const float pitch = camera_pitch * PI / 180.0f;
        const float camera_direction[3] = {
            std::cos(pitch) * std::cos(yaw),
            std::sin(pitch),
            std::cos(pitch) * std::sin(yaw),
        };
        lookAt(camera_position, camera_direction, view);

        std::memcpy(context.scene_uniform_buffer_mapped->projection, projection, sizeof(projection));
        std::memcpy(context.scene_uniform_buffer_mapped->view, view, sizeof(view));

        float normalized_directional_direction[3] = {
            directional_direction[0], directional_direction[1], directional_direction[2]
        };
        normalize3(normalized_directional_direction);

        for (int axis = 0; axis < 3; ++axis) {
            context.scene_uniform_buffer_mapped->camera_position[axis] = camera_position[axis];
            context.scene_uniform_buffer_mapped->ambient_color_intensity[axis] = ambient_color[axis];
            context.scene_uniform_buffer_mapped->directional_direction[axis] = normalized_directional_direction[axis];
            context.scene_uniform_buffer_mapped->directional_color_intensity[axis] = directional_color[axis];
        }

        context.scene_uniform_buffer_mapped->camera_position[3] = 1.0f;
        context.scene_uniform_buffer_mapped->ambient_color_intensity[3] = ambient_intensity;
        context.scene_uniform_buffer_mapped->directional_direction[3] = 0.0f;
        context.scene_uniform_buffer_mapped->directional_color_intensity[3] = directional_intensity;
        context.scene_uniform_buffer_mapped->light_counts[0] = static_cast<float>(point_count);
        context.scene_uniform_buffer_mapped->light_counts[1] = static_cast<float>(spot_count);
        context.scene_uniform_buffer_mapped->light_counts[2] = 0.0f;
        context.scene_uniform_buffer_mapped->light_counts[3] = 0.0f;

        
        for (uint32_t i = 0; i < point_count; ++i) {
            PointLightGpu& gpu_light = context.point_lights_mapped[i];
            for (int axis = 0; axis < 3; ++axis) {
                gpu_light.position[axis] = point_lights[i].position[axis];
                gpu_light.color_intensity[axis] = point_lights[i].color[axis];
            }
            gpu_light.position[3] = 1.0f;
            gpu_light.color_intensity[3] = point_lights[i].intensity;
        }

        
        for (uint32_t i = 0; i < spot_count; ++i) {
            const SpotLight& light = spot_lights[i];
            SpotLightGpu& gpu_light = context.spot_lights_mapped[i];
            float direction[3] = {
                light.direction[0], light.direction[1], light.direction[2]
            };
            normalize3(direction);

            for (int axis = 0; axis < 3; ++axis) {
                gpu_light.position[axis] = light.position[axis];
                gpu_light.direction[axis] = direction[axis];
                gpu_light.color_intensity[axis] = light.color[axis];
            }

            gpu_light.position[3] = 1.0f;
            gpu_light.direction[3] = 0.0f;
            gpu_light.color_intensity[3] = light.intensity;
            gpu_light.cone[0] = std::cos(light.inner_angle * PI / 180.0f);
            gpu_light.cone[1] = std::cos(light.outer_angle * PI / 180.0f);
            gpu_light.cone[2] = 0.0f;
            gpu_light.cone[3] = 0.0f;
        }

        if (vmaFlushAllocation(context.allocator, context.scene_uniform_buffer_allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS ||
            vmaFlushAllocation(context.allocator, context.point_lights_allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS ||
            vmaFlushAllocation(context.allocator, context.spot_lights_allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS) {
            std::cerr << "Failed to flush scene or light buffers\n";
            return;
        }

        
        
        
        const float object_x = object.position[0] + std::cos(anim_time) * object.orbit_radius;
        const float object_y = object.position[1] + std::sin(anim_time * 1.7f) * object.orbit_height;
        const float object_z = object.position[2] + std::sin(anim_time) * object.orbit_radius;

        math::Mat4 translate{};
        math::Mat4 rotate_x{};
        math::Mat4 rotate_y{};
        math::Mat4 rotate_z{};
        math::Mat4 scale{};
        math::Mat4 model{};

        math::translate(object_x, object_y, object_z, translate);
        math::rotateX(object.rotation[0] * PI / 180.0f + anim_time, rotate_x);
        math::rotateY(object.rotation[1] * PI / 180.0f + anim_time, rotate_y);
        math::rotateZ(object.rotation[2] * PI / 180.0f + anim_time, rotate_z);
        math::scale(object.scale[0], object.scale[1], object.scale[2], scale);

        math::multiply(translate, rotate_z, model);
        math::multiply(model, rotate_y, model);
        math::multiply(model, rotate_x, model);
        math::multiply(model, scale, model);

        ModelUniform* main_model = context.model_uniform_buffer_mapped[0];
        std::memcpy(main_model->model, model, sizeof(model));

        for (int axis = 0; axis < 3; ++axis) {
            main_model->albedo[axis] = object.color[axis];
            main_model->specular_color[axis] = specular_color[axis];
        }
        main_model->albedo[3] = 1.0f;
        main_model->specular_color[3] = 1.0f;
        main_model->material_params[0] = shininess;
        main_model->material_params[1] = 0.0f; 
        main_model->material_params[2] = 0.0f;
        main_model->material_params[3] = 0.0f;

        if (!flushModelUniform(0)) {
            return;
        }

        
        for (uint32_t i = 0; i < point_count; ++i) {
            const int model_index = 1 + static_cast<int>(i);
            writeMarkerModel(context.model_uniform_buffer_mapped[model_index],
                point_lights[i].position, 0.09f, point_lights[i].color);
            if (!flushModelUniform(model_index)) {
                return;
            }
        }

        
        for (uint32_t i = 0; i < spot_count; ++i) {
            const int model_index = 1 + static_cast<int>(MAX_POINT_LIGHTS + i);
            writeMarkerModel(context.model_uniform_buffer_mapped[model_index],
                spot_lights[i].position, 0.11f, spot_lights[i].color);
            if (!flushModelUniform(model_index)) {
                return;
            }
        }

        
        
        const int cone_model_start = 1 + static_cast<int>(MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS);
        const float white[3] = { 1.0f, 1.0f, 1.0f };

        for (uint32_t i = 0; i < spot_count; ++i) {
            const SpotLight& light = spot_lights[i];
            float direction[3] = {
                light.direction[0], light.direction[1], light.direction[2]
            };
            normalize3(direction);

            float reference[3] = { 0.0f, 1.0f, 0.0f };
            if (std::fabs(direction[1]) > 0.95f) {
                reference[0] = 1.0f;
                reference[1] = 0.0f;
                reference[2] = 0.0f;
            }

            float edge_x[3]{};
            cross3(direction, reference, edge_x);
            normalize3(edge_x);

            float edge_y[3]{};
            cross3(direction, edge_x, edge_y);

            const float cone_radius = std::tan(light.outer_angle * PI / 180.0f);
            for (uint32_t segment = 0; segment < SPOT_CONE_SEGMENTS * 2; ++segment) {
                const uint32_t side = segment % SPOT_CONE_SEGMENTS;
                const float angle0 = 2.0f * PI * static_cast<float>(side) / SPOT_CONE_SEGMENTS;
                const float angle1 = 2.0f * PI * static_cast<float>((side + 1) % SPOT_CONE_SEGMENTS) / SPOT_CONE_SEGMENTS;

                float start[3] = { light.position[0], light.position[1], light.position[2] };
                float edge0[3]{};
                float edge1[3]{};

                for (int axis = 0; axis < 3; ++axis) {
                    edge0[axis] = light.position[axis] + direction[axis] + cone_radius *
                        (std::cos(angle0) * edge_x[axis] + std::sin(angle0) * edge_y[axis]);
                    edge1[axis] = light.position[axis] + direction[axis] + cone_radius *
                        (std::cos(angle1) * edge_x[axis] + std::sin(angle1) * edge_y[axis]);
                }

                const int model_index = cone_model_start +
                    static_cast<int>(i * SPOT_CONE_LINES_PER_LIGHT + segment);
                ModelUniform* cone_model = context.model_uniform_buffer_mapped[model_index];

                if (segment < SPOT_CONE_SEGMENTS) {
                    writeRodModel(cone_model, start, edge0, white);
                }
                else {
                    writeRodModel(cone_model, edge0, edge1, white);
                }

                if (!flushModelUniform(model_index)) {
                    return;
                }
            }
        }
    }

    void render(const graphics::internal::FrameData& frame) {
        auto& context = graphics::internal::context;

        const VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(frame.command_buffer, &begin_info);

        const VkClearValue clear_values[] = {
            { .color = { .float32 = { 0.025f, 0.03f, 0.045f, 1.0f } } },
            { .depthStencil = { 1.0f, 0 } },
        };

        const VkRenderPassBeginInfo render_pass = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = context.render_pass,
            .framebuffer = frame.framebuffer,
            .renderArea = { .extent = context.swapchain_extent },
            .clearValueCount = 2,
            .pClearValues = clear_values,
        };

        vkCmdBeginRenderPass(frame.command_buffer, &render_pass, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.graphics_pipeline);

        const VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(context.swapchain_extent.width),
            .height = static_cast<float>(context.swapchain_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const VkRect2D scissor = { .extent = context.swapchain_extent };
        vkCmdSetViewport(frame.command_buffer, 0, 1, &viewport);
        vkCmdSetScissor(frame.command_buffer, 0, 1, &scissor);

        const VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(frame.command_buffer, 0, 1, &context.vertex_buffer, offsets);
        vkCmdBindIndexBuffer(frame.command_buffer, context.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        
        const VkDescriptorSet main_sets[] = {
            context.scene_descriptor_set,
            context.model_descriptor_sets[0],
        };
        vkCmdBindDescriptorSets(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            context.pipeline_layout, 0, 2, main_sets, 0, nullptr);
        vkCmdDrawIndexed(frame.command_buffer, context.index_count, 1, 0, 0, 0);

        if (show_light_visualizers) {
            
            for (uint32_t i = 0; i < point_count; ++i) {
                const int model_index = 1 + static_cast<int>(i);
                const VkDescriptorSet sets[] = {
                    context.scene_descriptor_set,
                    context.model_descriptor_sets[model_index],
                };
                vkCmdBindDescriptorSets(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    context.pipeline_layout, 0, 2, sets, 0, nullptr);
                vkCmdDrawIndexed(frame.command_buffer, context.sphere_index_count, 1,
                    context.sphere_first_index, 0, 0);
            }

            
            for (uint32_t i = 0; i < spot_count; ++i) {
                const int model_index = 1 + static_cast<int>(MAX_POINT_LIGHTS + i);
                const VkDescriptorSet sets[] = {
                    context.scene_descriptor_set,
                    context.model_descriptor_sets[model_index],
                };
                vkCmdBindDescriptorSets(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    context.pipeline_layout, 0, 2, sets, 0, nullptr);
                vkCmdDrawIndexed(frame.command_buffer, context.sphere_index_count, 1,
                    context.sphere_first_index, 0, 0);
            }

            
            const int cone_model_start = 1 + static_cast<int>(MAX_POINT_LIGHTS + MAX_SPOT_LIGHTS);
            for (uint32_t i = 0; i < spot_count; ++i) {
                for (uint32_t segment = 0; segment < SPOT_CONE_LINES_PER_LIGHT; ++segment) {
                    const int model_index = cone_model_start +
                        static_cast<int>(i * SPOT_CONE_LINES_PER_LIGHT + segment);
                    const VkDescriptorSet sets[] = {
                        context.scene_descriptor_set,
                        context.model_descriptor_sets[model_index],
                    };
                    vkCmdBindDescriptorSets(frame.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        context.pipeline_layout, 0, 2, sets, 0, nullptr);
                    vkCmdDrawIndexed(frame.command_buffer, context.rod_index_count, 1,
                        context.rod_first_index, 0, 0);
                }
            }
        }

        vkCmdEndRenderPass(frame.command_buffer);
        vkEndCommandBuffer(frame.command_buffer);
    }

} 
