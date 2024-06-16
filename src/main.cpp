#include <fstream>
#include <iostream>
#include <vector>
#include <limits>
#include <GL/glew.h>  // Include GLEW before GLFW
#include <GLFW/glfw3.h>
#include "vector.h"
#include "ray.h"
#include "camera.h"
#include "light.h"
#include "color.h"
#include "material.h"
#include "shape.h"

using namespace std;


// Utility function to write the image to a PPM file
void write_image(const string& filename, const vector<Color>& image, int width, int height) {
    ofstream file(filename);
    file << "P3\n" << width << " " << height << "\n255\n";
    for (const auto& color : image) {
        int ir = static_cast<int>(255.99 * color.r);
        int ig = static_cast<int>(255.99 * color.g);
        int ib = static_cast<int>(255.99 * color.b);
        file << ir << " " << ig << " " << ib << "\n";
    }
    file.close();
}

Camera camera(Vector3(3, 3, 2), Vector3(0, 0, -1), Vector3(0, 1, 0), 20.0f, 4.0f / 3.0f, 0.1f);

void processInput(GLFWwindow* window) {
    static const float cameraSpeed = 0.05f; // Adjust accordingly

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.origin = camera.origin + Vector3(0, 0, -cameraSpeed);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.origin = camera.origin + Vector3(0, 0, cameraSpeed);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.origin = camera.origin + Vector3(-cameraSpeed, 0, 0);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.origin = camera.origin + Vector3(cameraSpeed, 0, 0);

    // Update the camera parameters based on new position
    camera.update_camera(camera.origin, camera.origin + Vector3(0, 0, -1), Vector3(0, 1, 0), 20.0f, 4.0f / 3.0f, 0.1f);
}



// Function to calculate the color of a ray by checking for intersections with shapes
Color ray_color(const Ray& ray, const vector<Shape*>& shapes, const Light& light) {
    float t_min = 0.001;
    float t_max = numeric_limits<float>::max();
    float t = t_max;
    Shape* hit_shape = nullptr;

    for (const auto& shape : shapes) {
        float t_temp;
        if (shape->intersect(ray, t_temp) && t_temp < t) {
            t = t_temp;
            hit_shape = shape;
        }
    }

    if (hit_shape) {
        Vector3 hit_point = ray.point_at_parameter(t);
        Vector3 normal = hit_shape->get_normal(hit_point);
        Vector3 light_dir = (light.get_position() - hit_point).normalized();
        float diff = max(normal.dot(light_dir), 0.0f);

        Color diffuse = (hit_shape->material.diffuse * diff) * light.get_intensity();
        return diffuse.clamped();
    }

    // Background color
    Vector3 unit_direction = ray.direction.normalized();
    float t_background = 0.5 * (unit_direction.y + 1.0);
    return (Color(1.0, 1.0, 1.0) * (1.0 - t_background) + Color(0.5, 0.7, 1.0) * t_background).clamped();
}

int main() {
    // Initialize GLFW
    if (!glfwInit()) {
        cerr << "Failed to initialize GLFW" << endl;
        return -1;
    }

    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(800, 600, "Raytracer", NULL, NULL);
    if (!window) {
        cerr << "Failed to create GLFW window" << endl;
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    if (glewInit() != GLEW_OK) {
        cerr << "Failed to initialize GLEW" << endl;
        return -1;
    }

    // Image dimensions
    const int image_width = 800;
    const int image_height = 600;

    // Camera setup
    Vector3 lookfrom(3, 3, 2);
    Vector3 lookat(0, 0, -1);
    Vector3 vup(0, 1, 0);
    float vfov = 20.0;
    float aspect_ratio = float(image_width) / float(image_height);
    //Camera camera(lookfrom, lookat, vup, vfov, aspect_ratio);

    // Light setup
    Light light(Vector3(5, 5, 5), Vector3(1, 1, 1));

    // Scene setup
    vector<Shape*> shapes;
    Material red(Color(1, 0, 0), Color(1, 1, 1), 32);
    Material green(Color(0, 1, 0), Color(1, 1, 1), 32);
    Material blue(Color(0, 0, 1), Color(1, 1, 1), 32);

    shapes.push_back(new Sphere(Vector3(0, 0, -1), 0.5, red));
    shapes.push_back(new Sphere(Vector3(0, -100.5, -1), 100, green));
    shapes.push_back(new Plane(Vector3(0, -1, 0), Vector3(0, 1, 0), blue));

    // Render loop
    vector<Color> image(image_width * image_height);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_width, image_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        for (int j = image_height - 1; j >= 0; --j) {
            for (int i = 0; i < image_width; ++i) {
                float u = float(i) / float(image_width);
                float v = float(j) / float(image_height);
                Ray ray = camera.get_ray(u, v);
                Color pixel_color = ray_color(ray, shapes, light);
                image[j * image_width + i] = pixel_color;
            }
        }

        // Convert image to pixel data
        vector<unsigned char> pixel_data(image_width * image_height * 3);
        for (int i = 0; i < image.size(); ++i) {
            pixel_data[i * 3 + 0] = static_cast<unsigned char>(255.99 * image[i].r);
            pixel_data[i * 3 + 1] = static_cast<unsigned char>(255.99 * image[i].g);
            pixel_data[i * 3 + 2] = static_cast<unsigned char>(255.99 * image[i].b);
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_width, image_height, GL_RGB, GL_UNSIGNED_BYTE, pixel_data.data());

        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw the texture
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);

        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
        glEnd();

        glDisable(GL_TEXTURE_2D);

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }
    // Clean up
    glDeleteTextures(1, &texture);
    for (auto shape : shapes) {
        delete shape;
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

