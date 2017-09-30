//
//  main.cpp
//  ByteVisualizer
//
//  Created by Jas S on 2017-09-29.
//  Copyright © 2017 Jas S. All rights reserved.
//

#define SDL_MAIN_HANDLED
    #include <SDL2/SDL.h>
#include <stdio.h>

#ifdef _WIN32
    #include <GL/glew.h>
    #undef main
#else
    #include <OpenGL/gl3.h>
#endif

#include <algorithm>

#define TITLE "ByteVisualizer"
#define WIDTH 1024
#define HEIGHT 768
#define SDL_Exception(error) ({ printf("%s : %s\n", error, SDL_GetError()); return -1; })

namespace GLUtil
{
    inline void error_check() { int error = glGetError(); if(error != 0) printf("OpenGL error %i.\n", error); }
    bool _shader_status(void (*get_iv)(GLuint, GLenum, GLint*), void (*get_info_log)(GLuint, GLsizei, GLsizei*, GLchar*), GLenum pstatus, const char* error_message, GLuint id)
    {
        GLint status = 0;
        get_iv(id, pstatus, &status);
        if(status == GL_FALSE) {
            int length = 0;
            get_iv(id, GL_INFO_LOG_LENGTH, &length);
            char* error = new char[length];
            get_info_log(id, length, &length, error);
            
            printf("%s:\n%s\n", error_message, error);
            delete[] error;
            return false;
        }
        return true;

    }
    bool vertex_shader_status(GLuint id) { return _shader_status(glGetShaderiv, glGetShaderInfoLog, GL_COMPILE_STATUS, "Vertex shader compilation error", id); }
    bool fragment_shader_status(GLuint id) { return _shader_status(glGetShaderiv, glGetShaderInfoLog, GL_COMPILE_STATUS, "Fragment shader compilation error", id); }
    bool program_status(GLuint id) { return _shader_status(glGetProgramiv, glGetProgramInfoLog, GL_LINK_STATUS, "Program link error", id); }
}

namespace Renderer
{
    const float SCREEN_QUAD[12] =
    {
        // top triangle
        1.0f,  1.0f, // top right
        -1.0f,  1.0f, // top left
        -1.0f, -1.0f, // bottom left
        // bottom triangle:
        1.0f,  1.0f, // top right
        -1.0f, -1.0f, // bottom left
        1.0f, -1.0f  // bottom right
    };
    
    GLuint program;
    const char* VertexSource =
        "#version 330\n"
        "in vec2 vertex;"
        "out vec2 uv;"
        ""
        "void main()"
        "{"
        "   gl_Position = vec4(vertex, 0, 1);"
        "   uv = (vertex + vec2(1)) * 0.5;"
        "}"
    ;
    const char* FragmentSource =
        "#version 330\n"
        "in vec2 uv;"
        "uniform sampler2D tex;"
        "out vec4 color;"
        ""
        "void main()"
        "{"
        "   color = texture(tex, uv);"
        "}"
    ;
    
    GLuint texture = 0;
    GLuint texture_id = 0;
    GLuint _screen_vao = 0;
    GLuint _screen_vbo = 0;
    
    int initalize()
    {
        int ret = 0;
        glGenVertexArrays(1, &_screen_vao);
        glBindVertexArray(_screen_vao);
        
        glGenBuffers(1, &_screen_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, _screen_vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(SCREEN_QUAD), SCREEN_QUAD, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*) 0);
        
        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(vertex_shader, 1, &VertexSource, 0);
        glShaderSource(fragment_shader, 1, &FragmentSource, 0);
        glCompileShader(vertex_shader);
        glCompileShader(fragment_shader);
        if(!GLUtil::vertex_shader_status(vertex_shader) || !GLUtil::fragment_shader_status(fragment_shader)) {
            ret = -1;
            goto ERROR;
        }
        
        program = glCreateProgram();
        glAttachShader(program, vertex_shader);
        glAttachShader(program, fragment_shader);
        glBindAttribLocation(program, 0, "vertex");
        glBindFragDataLocation(program, 0, "color");
        glLinkProgram(program);
        if(!GLUtil::program_status(program)) {
            if(glIsProgram(program) == GL_TRUE) glDeleteProgram(program);
            ret = -1;
            goto ERROR;
        }
        
        texture_id = glGetUniformLocation(program, "tex");
        glUseProgram(program);
        
    ERROR:
        if(glIsShader(vertex_shader) == GL_TRUE) glDeleteShader(vertex_shader);
        if(glIsShader(vertex_shader) == GL_TRUE) glDeleteShader(fragment_shader);
        
        return ret;
    }
    
    int load_texture(const char* path)
    {
        FILE* file = fopen(path, "rb");
        if(!file) {
            printf("Error: Could not open file in path [%s]!\n", path);
            return -1;
        }
        
        fseek(file, 0L, SEEK_END);
        unsigned long length = ftell(file);
        rewind(file);
        
        int pixel_count = (int) ceil(((float)length) / 3.0f);
        int dimension = ((int) ceil(sqrt(pixel_count))) + 1;
        unsigned long size = dimension * dimension;

        unsigned long buffer_size = std::max(size, length);
        char* data = new char[buffer_size];
        memset(data, 0, buffer_size);
        if(fread(data, 1, length, file) != length) {
            printf("Error reading file [%s]! Corrupt or invalid!\n", path);
            return -1;
        }
        fclose(file);
        
        printf("Picture length: %lu pixels.\n", length);
        printf("Picture buffer size: %lu pixels.\n", buffer_size);
        printf("Picture dimensions: %i pixels.\n", dimension);
        
        if(glIsTexture(texture) == GL_TRUE) glDeleteTextures(1, &texture);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, dimension, dimension, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        delete[] data;
        return 0;
    }
    
    void render()
    {
        glClear(GL_COLOR_BUFFER_BIT);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1i(texture_id, 0);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    
    void free_memory()
    {
        if(glIsTexture(texture) == GL_TRUE) glDeleteTextures(1, &texture);
        if(glIsVertexArray(_screen_vao) == GL_TRUE) glDeleteVertexArrays(1, &_screen_vao);
        if(glIsBuffer(_screen_vbo) == GL_TRUE) glDeleteBuffers(1, &_screen_vbo);
        if(glIsProgram(program) == GL_TRUE) glDeleteProgram(program);
    }
}

int main(int argc, const char * argv[])
{
    /*
    if(argc != 2) {
        printf("Invalid number of arguments:\nUsage: ./ByteVisualizer [file_path]");
        return -1;
    }
     */
    const char* path = "/Users/jass/Documents/ByteVisualizer/ByteVisualizer/sample.txt";
    //const char* path = "/Users/jass/Downloads/Movies.pdf";
    
    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) SDL_Exception("SDL initalization error");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow(TITLE,
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WIDTH, HEIGHT,
                                          SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if(!window) SDL_Exception("SDL window creation error");
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if(!context) SDL_Exception("SDL OpenGL context creation error");
    
#ifdef _WIN32
    glewExperimental = true;
    if (glewInit() != GLEW_OK) {
        puts("GLEW initalization error.");
        return -1;
    }
#endif
    
    if(Renderer::initalize() != 0) return -1;
    if(Renderer::load_texture(path) != 0) return -1;
    
    const Uint8* keyboard = SDL_GetKeyboardState(NULL);
    SDL_Event event;
    bool running = true;
    while(running)
    {
        while(SDL_PollEvent(&event)) {
            switch(event.type)
            {
                case SDL_QUIT: running = false; break;
                default: break;
            }
        }
        
        if(keyboard[SDL_SCANCODE_ESCAPE]) running = false;
        
        GLUtil::error_check();
        Renderer::render();
        SDL_GL_SwapWindow(window);
    }
    
    Renderer::free_memory();
    
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
