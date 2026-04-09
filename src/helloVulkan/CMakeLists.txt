set(ExeName helloVulkan)
FILE(GLOB Exe_CPP "./*.cpp")
FILE(GLOB Exe_H "${CMAKE_SOURCE_DIR}/include/${ExeName}/*.h")

ADD_EXECUTABLE(${ExeName} ${Exe_CPP} ${Exe_H})

target_compile_features(${ExeName} PRIVATE cxx_std_20)
target_include_directories(${ExeName} PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(${ExeName} PUBLIC ${CMAKE_SOURCE_DIR}/include/${ExeName})

target_include_directories(${ExeName} PUBLIC ${GLM_INCLUDE_DIR})
target_include_directories(${ExeName} PUBLIC ${VULKAN_INCLUDE_DIR})
target_include_directories(${ExeName} PUBLIC ${GLFW_INCLUDE_DIR})


target_link_libraries(${ExeName} PUBLIC ${VULKAN_LIB})
target_link_libraries(${ExeName} PUBLIC ${GLFW_LIB})