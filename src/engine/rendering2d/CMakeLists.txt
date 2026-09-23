add_library(anxiety_rendering2d STATIC
    TextureAtlas.cpp
    SpriteBatch.cpp
    SpriteInstancer.cpp
    Renderer2D.cpp)

target_include_directories(anxiety_rendering2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(anxiety_rendering2d PUBLIC
    anxiety::core)

add_library(anxiety::rendering2d ALIAS anxiety_rendering2d)
