//
//  glslp.c
//  Pal
//
//  Created by palxex on 11/7/18.
//

#include "glslp.h"

GLSLP gGLSLP;

void clear_slots(shader_param *param) {
    memset(&param->slots, -1, sizeof(texture_unit_slots) );
    memset(&param->orig_slots, -1, sizeof(texture_unit_slots) );
    memset(&param->pass_slots, -1, sizeof(texture_unit_slots)*MAX_INDEX );
    memset(&param->prev_slots, -1, sizeof(texture_unit_slots)*MAX_INDEX );
    memset(&param->passprev_slots, -1, sizeof(texture_unit_slots)*MAX_INDEX );
}

bool parse_glslp(const char *filename) {
    //todo: actual parse, instead of the FAKE PARSER NOW
    destroy_glslp();

//    TestCase 1: metacrt
//    gGLSLP.shaders = 1;
//    gGLSLP.shader_params = malloc(1*sizeof(shader_param));
//    memset(gGLSLP.shader_params, 0, 1*sizeof(shader_param));
//    {
//        shader_param *param = &gGLSLP.shader_params[0];
//        clear_slots(param);
//        param->shader = "crt/shaders/metacrt/bufC.glsl"; //NOTICE here! need `dirname absolute filename` +
//    }
//
//    gGLSLP.textures = 2;
//    gGLSLP.texture_params = malloc(2*sizeof(texture_param));
//    memset(gGLSLP.texture_params, 0, 2*sizeof(texture_param));
//    {
//        texture_param *param = &gGLSLP.texture_params[0];
//        param->texture_name = "cubeMap";
//        param->texture_path = "crt/shaders/metacrt/basilica.png";
//        param->wrap_mode = REPEAT;
//        param = &gGLSLP.texture_params[1];
//        param->texture_name = "table";
//        param->texture_path = "crt/shaders/metacrt/woodgrain.png";
//        param->wrap_mode = REPEAT;
//    }
    
//    TestCase 2: film
//    gGLSLP.shaders = 1;
//    gGLSLP.shader_params = malloc(1*sizeof(shader_param));
//    memset(gGLSLP.shader_params, 0, 1*sizeof(shader_param));
//    {
//        shader_param *param = &gGLSLP.shader_params[0];
//        clear_slots(param);
//        param->shader = "film/shaders/film_noise.glsl"; //NOTICE here! need `dirname absolute filename` +
//        param->frame_count_mod = 137;
//    }
//
//    gGLSLP.textures = 1;
//    gGLSLP.texture_params = malloc(gGLSLP.textures*sizeof(texture_param));
//    memset(gGLSLP.texture_params, 0, gGLSLP.textures*sizeof(texture_param));
//    {
//        texture_param *param = &gGLSLP.texture_params[0];
//        memset(param->slots,-1,sizeof(param->slots));
//        param->texture_name = "noise1";
//        param->texture_path = "film/resources/film_noise1.png";
//        param->wrap_mode = REPEAT;
//    }
    
    //Demo 3(WIP) : xbrz-freescale
    gGLSLP.shaders = 2;
    gGLSLP.shader_params = malloc(2*sizeof(shader_param));
    memset(gGLSLP.shader_params, 0, 2*sizeof(shader_param));
    {
        shader_param *param = &gGLSLP.shader_params[0];
        clear_slots(param);
        param->shader = "xbrz/shaders/xbrz-freescale-multipass/xbrz-freescale-pass0.glsl"; //NOTICE here! need `dirname absolute filename` +
        param->scale_type = SOURCE;
        param = &gGLSLP.shader_params[1];
        clear_slots(param);
        param->shader = "xbrz/shaders/xbrz-freescale-multipass/xbrz-freescale-pass1.glsl"; //NOTICE here! need `dirname absolute filename` +
        param->scale_type = VIEWPORT;
    }

    //uniform parameter is not needed since currently SDLPal lacks runtime configuation alter method like RetroArch GUI.
    //lets consider it after for exam quake console introduced:)
    
    return true;
}

void destroy_glslp() {
    free(gGLSLP.shader_params);
    for( int i = 0; i < gGLSLP.shaders; i++ )
        if(gGLSLP.shader_params[i].sdl_texture )
            SDL_DestroyTexture( gGLSLP.shader_params[i].sdl_texture );
    for( int i=0; i<gGLSLP.textures; i++ )
        if(gGLSLP.texture_params[i].sdl_texture )
            SDL_DestroyTexture(gGLSLP.texture_params[i].sdl_texture);
    free(gGLSLP.texture_params);
    free(gGLSLP.uniform_params);
    memset(&gGLSLP, 0, sizeof(GLSLP));
}
