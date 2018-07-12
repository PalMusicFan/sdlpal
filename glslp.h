//
//  glslp.h
//  Pal
//
//  Created by palxex on 11/7/18.
//

#ifndef glslp_h
#define glslp_h

#include "main.h"

#define MAX_FBO 8
#define MAX_INDEX (MAX_FBO+2)

#define MAX_TEXTURES 8

enum wrap_mode {
    REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER
};

enum scale_type {
    SOURCE,
    VIEWPORT,
    ABSOLUTE
};

typedef struct tagTEXTUREUNITSLOTS {
    int texture_slot;
    int texture_size_slot;
    int input_size_slot;
    int tex_coord_slot;
}texture_unit_slots;

typedef struct tagSHADERPARAM {
    //by defination
    char *shader;
    bool filter_linear;
    enum wrap_mode wrap_mode;
    enum scale_type scale_type, scale_type_x, scale_type_y;
    float scale, scale_x, scale_y;
    bool mipmap_input;
    char *alias;
    bool float_framebuffer;
    bool srgb_framebuffer;
    int frame_count_mod;
    
    //by implementation
    SDL_Texture *sdl_texture;
    int frame_count;
    texture_unit_slots slots;
    texture_unit_slots orig_slots;
    texture_unit_slots pass_slots[MAX_INDEX];
    texture_unit_slots prev_slots[MAX_INDEX];
    texture_unit_slots passprev_slots[MAX_INDEX];
}shader_param;

typedef struct tagTEXTUREPARAMS {
    //by defination
    char *texture_name;
    char *texture_path;
    enum wrap_mode wrap_mode;
    
    //by implementation
    SDL_Texture *sdl_texture;
    int slots[MAX_INDEX];
}texture_param;

typedef struct tagUNIFORMPARAMS {
    char *uniform_key;
    char *uniform_value;
}uniform_param;

typedef struct tagGLSLP {
    int shaders;
    shader_param *shader_params;
    int textures;
    texture_param *texture_params;
    int uniform_parameters;
    uniform_param *uniform_params;
}GLSLP;

extern GLSLP gGLSLP;

bool parse_glslp(const char *);

void destroy_glslp();

#endif /* glslp_h */
