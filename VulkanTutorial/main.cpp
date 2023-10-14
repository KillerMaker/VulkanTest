#include<iostream>
#include"VulkanRenderer.h"

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 800;

GLFWwindow* window;

void initWIndow(const char* name, unsigned int width = 800, unsigned int height = 600);

int main()
{
    VulkanRenderer vkRenderer;

    //Initializes the window
    initWIndow("TestWindow", WIDTH, HEIGHT);

    //Initializes the vkRenderer
    if (vkRenderer.init(window) == EXIT_FAILURE)
        return EXIT_FAILURE;
    
    //Render Loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        vkRenderer.draw();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return EXIT_SUCCESS;
}

void initWIndow(const char* name, unsigned int width, unsigned int height)
{
    //Initializes GLFW
    glfwInit();

    //Window Hints
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    //Window Instanciation
    window = glfwCreateWindow(width, height, name, nullptr, nullptr);
}
