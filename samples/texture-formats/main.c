/*
 * This sample provides a very basic demonstration of 3D rendering on the Xbox,
 * using pbkit. Based on the pbkit demo sources.
 */
#include <hal/video.h>
#include <hal/xbox.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <pbkit/pbkit.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <hal/debug.h>
#include "math3d.h"
#include <SDL.h>
#include <SDL_image.h>
#include "swizzle.h"

typedef struct FormatInfo {
    SDL_PixelFormatEnum SdlFormat;
    uint32_t XboxFormat;
    bool XboxSwizzled;
    bool RequireConversion;
    char* Name;
} FormatInfo;

#pragma pack(1)
typedef struct Vertex {
    float pos[3];
    float texcoord[2];
    float normal[3];
} Vertex;

#pragma pack()

static Vertex *alloc_vertices;  // texcoords 0 to width/height
static Vertex *alloc_vertices_swizzled; // texcoords normalized 0 to 1
static uint32_t  num_vertices;

MATRIX m_model, m_view, m_proj;

VECTOR v_obj_pos     = {  0,   0,   0,  1 };
VECTOR v_obj_rot     = {  0,   0,   0,  1 };
VECTOR v_cam_pos     = {  0,   0.05,   1.07,  1 };
VECTOR v_cam_rot     = {  0,   0,   0,  1 };
VECTOR v_light_dir   = {  0,   0,   1,  1 };

#include "verts.h"
#include "texture.h"

#define MASK(mask, val) (((val) << (ffs(mask)-1)) & (mask))

static struct {
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    void     *addr;
    int      error;
} texture;

#define MAXRAM 0x03FFAFFF

// TODO: upstream missing nv2a defines
#define NV2A_VERTEX_ATTR_POSITION       0
#define NV2A_VERTEX_ATTR_NORMAL         2
#define NV2A_VERTEX_ATTR_TEXTURE0       9
#define NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8            0x17
#define NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8              0x3B
#define NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8    0x24
#define NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8    0x25
#define NV097_SET_TEXTURE_FORMAT_COLOR_D16                      0x2C    // TODO: proper nvidia name
#define NV097_SET_TEXTURE_FORMAT_COLOR_LIN_F16                  0x31    // TODO: proper nvidia name

static void matrix_viewport(float out[4][4], float x, float y, float width, float height, float z_min, float z_max);
static void init_shader(void);
static void init_textures(void);
static void set_attrib_pointer(unsigned int index, unsigned int format, unsigned int size, unsigned int stride, const void* data);
static void draw_arrays(unsigned int mode, int start, int count);

static int format_map_index = 0;
static const FormatInfo format_map[] = {

    // swizzled
    { SDL_PIXELFORMAT_ABGR8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8, true, false, "A8B8G8R8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8G8B8A8, true, false, "R8G8B8A8" },
    { SDL_PIXELFORMAT_ARGB8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8, true, false, "A8R8G8B8" },
    { SDL_PIXELFORMAT_ARGB8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8, true, false, "X8R8G8B8" },
    { SDL_PIXELFORMAT_BGRA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8, true, false, "B8G8R8A8" },
    { SDL_PIXELFORMAT_RGB565, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5, true, false, "R5G6B5" },
    { SDL_PIXELFORMAT_ARGB1555, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5, true, false, "A1R5G5B5" },
    { SDL_PIXELFORMAT_ARGB1555, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5, true, false, "X1R5G5B5" },
    { SDL_PIXELFORMAT_ARGB4444, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4, true, false, "A4R4G4B4" }, 

    // linear unsigned
    { SDL_PIXELFORMAT_ABGR8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8, false, false, "A8B8G8R8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8, false, false, "R8G8B8A8" },
    { SDL_PIXELFORMAT_ARGB8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8, false, false, "A8R8G8B8" },   
    { SDL_PIXELFORMAT_ARGB8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8, false, false, "X8R8G8B8" },
    { SDL_PIXELFORMAT_BGRA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8, false, false, "B8G8R8A8" },
    { SDL_PIXELFORMAT_RGB565, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5, false, false, "R5G6B5" },
    { SDL_PIXELFORMAT_ARGB1555, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5, false, false, "A1R5G5B5" },
    { SDL_PIXELFORMAT_ARGB1555, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5, false, false, "X1R5G5B5" },
    { SDL_PIXELFORMAT_ARGB4444, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4, false, false, "A4R4G4B4" },

    // TODO: for some that need conversion, can probably use rgba and just see if both xemu and hardware display the same garbage

    // yuv color space
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8, false, true, "YUY2" },
    //{ SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8, false, true, "UYVY" },  // TODO: implement in xemu
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16, false, true, "Y16" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8, true, true, "SZ_Y8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y8, false, true, "Y8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_AY8, true, true, "SZ_AY8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_AY8, false, true, "AY8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8, true, true, "SZ_A8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8Y8, true, true, "SZ_A8Y8" },
    
    // misc formats
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5, false, true, "DXT1" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8, false, true, "DXT3" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8, false, true, "DXT5" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_G8B8, true, true, "SZ_G8B8" },
    //{ SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8, false, true, "G8B8" },    // TODO: implement in xemu
    //{ SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_D16, false, true, "D16" },    // TODO: implement in xemu
    //{ SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_LIN_F16, false, true, "LIN_F16" },    // TODO: implement in xemu
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8B8, true, true, "SZ_R8B8" },
    { SDL_PIXELFORMAT_RGBA8888, NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5, true, true, "R6G5B5" }
    // TODO: define others here
};

// bitscan forward
int bsf(int val) {
    __asm bsf eax, val
}

int update_texture_memory(SDL_PixelFormatEnum format, int width, int height, bool swizzled)
{
    // create source surface
    SDL_Surface *gradient_surf = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA8888);
    if (gradient_surf == NULL)
        return 1;

    if (SDL_LockSurface(gradient_surf))
        return 2;

    // generate basic gradient pattern
    uint32_t *pixels = (uint32_t*)gradient_surf->pixels;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int xScale = x * 255.0f / width;
            int yScale = y * 255.0f / height;
            pixels[y * width + x] = SDL_MapRGB(gradient_surf->format, yScale, xScale, yScale);
        }
    }

    SDL_UnlockSurface(gradient_surf);

    // convert to desired destination format
    SDL_Surface *new_surf = SDL_ConvertSurfaceFormat(gradient_surf, format, 0);
    if (!new_surf)
        return 3;

    SDL_FreeSurface(gradient_surf);
    if (texture.addr)
        MmFreeContiguousMemory(texture.addr);

    // TODO: texture conversions if necessary, will need to pass in additional argument(s)

    // allocate texture memory
    uint8_t *dst_tex_buf = MmAllocateContiguousMemoryEx(height * new_surf->pitch, 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE);
    if (!dst_tex_buf) {
        SDL_FreeSurface(new_surf);
        return 4;
    }

    // copy pixels over to texture memory, swizzling if desired
    if (swizzled) {
        swizzle_rect((uint8_t*)new_surf->pixels, new_surf->w, new_surf->h, dst_tex_buf, new_surf->pitch, new_surf->format->BytesPerPixel);
    } else {
        memcpy(dst_tex_buf, new_surf->pixels, new_surf->pitch * new_surf->h);
    }

    // HACK: update global texture info
    texture.width = new_surf->w;
    texture.height = new_surf->h;
    texture.pitch = new_surf->pitch;
    texture.addr = dst_tex_buf;

    SDL_FreeSurface(new_surf);

    return 0;
}

SDL_GameController *gameController;

/* Main program function */
int main(void)
{
    uint32_t *p;
    int       i, status;
    int       width, height;
    float     m_viewport[4][4];
    int format_map_index = 0;
    bool toggleFormat;

    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

    // initialize input for the first gamepad
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    gameController = SDL_GameControllerOpen(0);
    if (!gameController) {
        debugPrint("Failed to initialize input for gamepad 0");
        Sleep(2000);
        return 1;
    }

    if ((status = pb_init())) {
        debugPrint("pb_init Error %d\n", status);
        Sleep(2000);
        return 1;
    }

    pb_show_front_screen();

    /* Basic setup */
    width = pb_back_buffer_width();
    height = pb_back_buffer_height();

    /* Load constant rendering things (shaders, geometry) */
    init_shader();

    // real nv2a hardware seems to cache this and not honor updates? have separate vertex buffers for swizzled and linear for now...
    alloc_vertices = MmAllocateContiguousMemoryEx(sizeof(vertices), 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE);
    alloc_vertices_swizzled = MmAllocateContiguousMemoryEx(sizeof(vertices), 0, MAXRAM, 0, PAGE_WRITECOMBINE | PAGE_READWRITE);
    memcpy(alloc_vertices, vertices, sizeof(vertices));
    memcpy(alloc_vertices_swizzled, vertices, sizeof(vertices));
    num_vertices = sizeof(vertices)/sizeof(vertices[0]);
    for (int i = 0; i < num_vertices; i++) {
        // TODO: programmatic generation of buffers to support variable non-square sizes
        if (alloc_vertices[i].texcoord[0]) alloc_vertices[i].texcoord[0] = 256.0f;
        if (alloc_vertices[i].texcoord[1]) alloc_vertices[i].texcoord[1] = 256.0f;
    }

    /* Create view matrix (our camera is static) */
    matrix_unit(m_view);
    create_world_view(m_view, v_cam_pos, v_cam_rot);

    /* Create projection matrix */
    matrix_unit(m_proj);
    create_view_screen(m_proj, (float)width/(float)height, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 10000.0f);

    /* Create viewport matrix, combine with projection */
    matrix_viewport(m_viewport, 0, 0, width, height, 0, 65536.0f);
    matrix_multiply(m_proj, m_proj, (float*)m_viewport);

    /* Create local->world matrix given our updated object */
    matrix_unit(m_model);

    while(1) {

        // cycle current texture based on A or B button presses
        SDL_GameControllerUpdate();
        bool aPress = SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_A);
        bool bPress = SDL_GameControllerGetButton(gameController, SDL_CONTROLLER_BUTTON_B);
        if (aPress || bPress) {
            if (toggleFormat) {
                format_map_index = (format_map_index + (aPress ? 1 : -1)) % (sizeof(format_map) / sizeof(format_map[0]));
            }
            toggleFormat = false;
        } else toggleFormat = true;

        pb_wait_for_vbl();
        pb_reset();
        pb_target_back_buffer();

        /* Clear depth & stencil buffers */
        pb_erase_depth_stencil_buffer(0, 0, width, height);
        pb_fill(0, 0, width, height, 0xff202020);
        pb_erase_text_screen();

        while(pb_busy()) {
            /* Wait for completion... */
        }

        /*
         * Setup texture stages
         */

        /* Enable texture stage 0 */
        /* FIXME: Use constants instead of the hardcoded values below */
        p = pb_begin();

        // first one seems to be needed
        p = pb_push1(p, NV097_SET_FRONT_FACE, NV097_SET_FRONT_FACE_V_CCW);
        p = pb_push1(p, NV097_SET_DEPTH_TEST_ENABLE, true);

        texture.error = update_texture_memory(format_map[format_map_index].SdlFormat, 256, 256, format_map[format_map_index].XboxSwizzled);
        DWORD format_mask = MASK(NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA, 1) |
            MASK(NV097_SET_TEXTURE_FORMAT_CUBEMAP_ENABLE, 0) |
            MASK(NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE, NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE_COLOR) |
            MASK(NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY, 2) |
            MASK(NV097_SET_TEXTURE_FORMAT_COLOR, format_map[format_map_index].XboxFormat) |
            MASK(NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS, 1) |
            MASK(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U, format_map[format_map_index].XboxSwizzled ? bsf(texture.width) : 0) |
            MASK(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V, format_map[format_map_index].XboxSwizzled ? bsf(texture.height) : 0) |
            MASK(NV097_SET_TEXTURE_FORMAT_BASE_SIZE_P, 0);
        p = pb_push2(p,NV20_TCL_PRIMITIVE_3D_TX_OFFSET(0),(DWORD)texture.addr & 0x03ffffff,format_mask); //set stage 0 texture address & format
        if (!format_map[format_map_index].XboxSwizzled) {
            p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_NPOT_PITCH(0),texture.pitch<<16); //set stage 0 texture pitch (pitch<<16)
            p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_NPOT_SIZE(0),(texture.width<<16)|texture.height); //set stage 0 texture width & height ((witdh<<16)|height)
        }
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_WRAP(0),0x00030303);//set stage 0 texture modes (0x0W0V0U wrapping: 1=wrap 2=mirror 3=clamp 4=border 5=clamp to edge)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_ENABLE(0),0x4003ffc0); //set stage 0 texture enable flags
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_FILTER(0),0x04074000); //set stage 0 texture filters (AA!)

        pb_end(p);

        /* Disable other texture stages */
        p = pb_begin();
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_ENABLE(1),0x0003ffc0);//set stage 1 texture enable flags (bit30 disabled)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_ENABLE(2),0x0003ffc0);//set stage 2 texture enable flags (bit30 disabled)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_ENABLE(3),0x0003ffc0);//set stage 3 texture enable flags (bit30 disabled)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_WRAP(1),0x00030303);//set stage 1 texture modes (0x0W0V0U wrapping: 1=wrap 2=mirror 3=clamp 4=border 5=clamp to edge)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_WRAP(2),0x00030303);//set stage 2 texture modes (0x0W0V0U wrapping: 1=wrap 2=mirror 3=clamp 4=border 5=clamp to edge)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_WRAP(3),0x00030303);//set stage 3 texture modes (0x0W0V0U wrapping: 1=wrap 2=mirror 3=clamp 4=border 5=clamp to edge)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_FILTER(1),0x02022000);//set stage 1 texture filters (no AA, stage not even used)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_FILTER(2),0x02022000);//set stage 2 texture filters (no AA, stage not even used)
        p = pb_push1(p,NV20_TCL_PRIMITIVE_3D_TX_FILTER(3),0x02022000);//set stage 3 texture filters (no AA, stage not even used)
        pb_end(p);

        /* Send shader constants
         *
         * WARNING: Changing shader source code may impact constant locations!
         * Check the intermediate file (*.inl) for the expected locations after
         * changing the code.
         */
        p = pb_begin();

        /* Set shader constants cursor at C0 */
        p = pb_push1(p, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_ID, 96);

        /* Send the model matrix */
        pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 16);
        memcpy(p, m_model, 16*4); p+=16;

        /* Send the view matrix */
        pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 16);
        memcpy(p, m_view, 16*4); p+=16;

        /* Send the projection matrix */
        pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 16);
        memcpy(p, m_proj, 16*4); p+=16;

        /* Send camera position */
        pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 4);
        memcpy(p, v_cam_pos, 4*4); p+=4;

        /* Send light direction */
        pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 4);
        memcpy(p, v_light_dir, 4*4); p+=4;

        /* Send shader constants */
        float constants_0[4] = {0, 0, 0, 0};
        pb_push(p++, NV20_TCL_PRIMITIVE_3D_VP_UPLOAD_CONST_X, 4);
        memcpy(p, constants_0, 4*4); p+=4;

        /* Clear all attributes */
        pb_push(p++,NV097_SET_VERTEX_DATA_ARRAY_FORMAT,16);
        for(i = 0; i < 16; i++) {
            *(p++) = 2;
        }
        pb_end(p);

        /*
         * Setup vertex attributes
         */

        Vertex *vptr = format_map[format_map_index].XboxSwizzled ? alloc_vertices_swizzled : alloc_vertices;

        /* Set vertex position attribute */
        set_attrib_pointer(NV2A_VERTEX_ATTR_POSITION, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F,
                           3, sizeof(Vertex), &vptr[0].pos);

        /* Set texture coordinate attribute */
        set_attrib_pointer(NV2A_VERTEX_ATTR_TEXTURE0, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F,
                           2, sizeof(Vertex), &vptr[0].texcoord);

        /* Set vertex normal attribute */
        set_attrib_pointer(NV2A_VERTEX_ATTR_NORMAL, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F,
                           3, sizeof(Vertex), &vptr[0].normal);
        
        /* Begin drawing triangles */
        draw_arrays(NV097_SET_BEGIN_END_OP_TRIANGLES, 0, num_vertices);

        /* Draw some text on the screen */
        pb_print("N: %s\n", format_map[format_map_index].Name);
        pb_print("F: 0x%x\n", format_map[format_map_index].XboxFormat);
        pb_print("SZ: %d\n", format_map[format_map_index].XboxSwizzled);
        pb_print("C: %d\n", format_map[format_map_index].RequireConversion);
        pb_print("W: %d\n", texture.width);
        pb_print("H: %d\n", texture.height);
        pb_print("P: %d\n", texture.pitch);
        pb_print("ERR: %d\n", texture.error);
        pb_draw_text_screen();

        while(pb_busy()) {
            /* Wait for completion... */
        }

        /* Swap buffers (if we can) */
        while (pb_finished()) {
            /* Not ready to swap yet */
        }
    }

    /* Unreachable cleanup code */
    SDL_GameControllerClose(gameController);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    MmFreeContiguousMemory(alloc_vertices);
    MmFreeContiguousMemory(texture.addr);
    pb_show_debug_screen();
    pb_kill();
    return 0;
}

/* Construct a viewport transformation matrix */
static void matrix_viewport(float out[4][4], float x, float y, float width, float height, float z_min, float z_max)
{
    memset(out, 0, 4*4*sizeof(float));
    out[0][0]  = width/2.0f;
    out[1][1]  = height/-2.0f;
    out[2][2] = (z_max - z_min)/2.0f;
    out[3][3] = 1.0f;
    out[3][0] = x + width/2.0f;
    out[3][1] = y + height/2.0f;
    out[3][2] = (z_min + z_max)/2.0f;
}

/* Load the shader we will render with */
static void init_shader(void)
{
    uint32_t *p;
    int       i;

    /* Setup vertex shader */
    uint32_t vs_program[] = {
        #include "vs.inl"
    };

    p = pb_begin();

    /* Set run address of shader */
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_START, 0);

    /* Set execution mode */
    p = pb_push1(p, NV097_SET_TRANSFORM_EXECUTION_MODE,
                 MASK(NV097_SET_TRANSFORM_EXECUTION_MODE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_MODE_PROGRAM)
                 | MASK(NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE, NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE_PRIV));

    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN, 0);
    pb_end(p);

    /* Set cursor and begin copying program */
    p = pb_begin();
    p = pb_push1(p, NV097_SET_TRANSFORM_PROGRAM_LOAD, 0);
    pb_end(p);

    /* Copy program instructions (16-bytes each) */
    for (i=0; i<sizeof(vs_program)/16; i++) {
        p = pb_begin();
        pb_push(p++, NV097_SET_TRANSFORM_PROGRAM, 4);
        memcpy(p, &vs_program[i*4], 4*4);
        p+=4;
        pb_end(p);
    }

    /* Setup fragment shader */
    p = pb_begin();
    #include "ps.inl"
    pb_end(p);
}

/* Set an attribute pointer */
static void set_attrib_pointer(unsigned int index, unsigned int format, unsigned int size, unsigned int stride, const void* data)
{
    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_FORMAT + index*4,
        MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE, format) | \
        MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE, size) | \
        MASK(NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE, stride));
    p = pb_push1(p, NV097_SET_VERTEX_DATA_ARRAY_OFFSET + index*4, (uint32_t)data & 0x03ffffff);
    pb_end(p);
}

/* Send draw commands for the triangles */
static void draw_arrays(unsigned int mode, int start, int count)
{
    uint32_t *p = pb_begin();
    p = pb_push1(p, NV097_SET_BEGIN_END, mode);

    p = pb_push1(p, 0x40000000|NV097_DRAW_ARRAYS, //bit 30 means all params go to same register 0x1810
                 MASK(NV097_DRAW_ARRAYS_COUNT, (count-1)) | MASK(NV097_DRAW_ARRAYS_START_INDEX, start));

    p = pb_push1(p, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END);
    pb_end(p);
}
