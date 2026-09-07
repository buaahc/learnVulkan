#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>


int main() {
    std::cout << "程序运行完毕，请按回车键退出..." << std::endl;
    std::cin.get(); // 等待用户输入回车
    return EXIT_SUCCESS;
}