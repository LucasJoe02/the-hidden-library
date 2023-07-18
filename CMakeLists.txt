cmake_minimum_required(VERSION 2.8)

project(assignment1)
add_executable(Gallery.out Gallery.cpp)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
target_include_directories(Gallery.out PRIVATE
    ${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})

target_link_libraries(Gallery.out ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})
