glslc ./shader.vert -o vert.spv
glslc ./shader.frag -o frag.spv

glslc ./compute.vert -o computevert.spv
glslc ./compute.frag -o computefrag.spv
glslc ./compute.comp -o Comp.spv
pause