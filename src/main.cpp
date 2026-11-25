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
#include "mesh.h"
#include "renderer.h"
#include "scene.h"

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

void processInput(GLFWwindow* window, Camera* camera) {
    static const float cameraSpeed = 0.5f; // Adjust accordingly

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera->updatePosition(camera->origin - camera->w * cameraSpeed); // move forward
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera->updatePosition(camera->origin + camera->w * cameraSpeed); // move backward
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera->updatePosition(camera->origin - camera->u * cameraSpeed); // move left
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera->updatePosition(camera->origin + camera->u * cameraSpeed); // move right
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static const float sensitivity = 0.1f;  // Mouse sensitivity
    static float lastX = 400, lastY = 300;  // Initial mouse position
    static bool firstMouse = true;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;  // Reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // Retrieve the camera from the user pointer
    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera) {
        camera->updateOrientation(xoffset, yoffset);
    }
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

    // Scene setup (models are loaded from the repo's objs/ directory)
    Scene scene;
    build_default_scene(scene);

    // Camera setup
    Vector3 vup(0, 1, 0);
    float aspect_ratio = float(image_width) / float(image_height);

    Camera camera(scene.camera_from, scene.camera_at, vup, scene.vfov, aspect_ratio, 0.1f);

    // Set the user pointer to the camera
    glfwSetWindowUserPointer(window, &camera);

    // Set the mouse callback
    glfwSetCursorPosCallback(window, mouse_callback);

    // Capture the mouse cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Render loop
    vector<Color> image(image_width * image_height);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_width, image_height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    while (!glfwWindowShouldClose(window)) {
        processInput(window, &camera);
        

        render_frame(camera, scene.shapes, scene.light, image_width, image_height, image);

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
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

