cmake_minimum_required (VERSION 3.24)
PROJECT(learnVulkan C CXX)
set(Vulkan_BASE_DIR "" CACHE PATH "vulkan dependency base dir")
set(GLFW_BASE_DIR "" CACHE PATH "glfw dependency base dir")
set(GLM_INCLUDE_DIR "" CACHE PATH "glm dependency base dir")

find_library(VULKAN_LIB vulkan-1.lib ${Vulkan_BASE_DIR}/lib)
find_Path(VULKAN_INCLUDE_DIR vulkan/vulkan.h ${Vulkan_BASE_DIR}/Include)

find_library(GLFW_LIB glfw3.lib ${GLFW_BASE_DIR}/lib)
find_Path(GLFW_INCLUDE_DIR GLFW/glfw3.h ${GLFW_BASE_DIR}/Include)

message("VULKAN_LIB-----${VULKAN_LIB}")
message("VULKAN_INCLUDE_DIR-----${VULKAN_INCLUDE_DIR}")
message("GLFW_LIB---${GLFW_LIB}")
message("GLFW_INCLUDE_DIR---${GLFW_INCLUDE_DIR}")
message("GLM_INCLUDE_DIR---${GLM_INCLUDE_DIR}")

add_subdirectory (src)
#---------------