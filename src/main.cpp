/*
 * mc
 */

#include <GL/glew.h>

#define HAVE_M_PI  // SDL_stdinc.h has M_PI and it collides with Windows M_PI
#define SDL_MAIN_HANDLED
#include "SDL.h"

#include <signal.h>
#include <math.h>
//#include <x86intrin.h>
#include <intrin.h>
#include <dirent.h>

#include "cglm/cglm.h"

#define FBX_IMPLEMENTATION
#include "fbx.h"

#include "meow_intrinsics.h"
#include "meow_hash.h"

#include "shaders.h"

#include "base.h"
#include "storage.cpp"

#include "file.cpp"
#include "img.h"
#include "img.cpp"

#include "render.cpp"
#include "load_asset.cpp"


#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO  // includes stdio.h
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_GLES2_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_sdl_gles2.h"

#define CUSTOM_3D_SPACE_MAX_X   1000.0f
#define CUSTOM_3D_SPACE_MAX_Y   1000.0f
#define CUSTOM_3D_SPACE_MAX_Z   1000.0f
#define CUSTOM_3D_SPACE_MIN_X  -1000.0f
#define CUSTOM_3D_SPACE_MIN_Y  -1000.0f
#define CUSTOM_3D_SPACE_MIN_Z  -1000.0f

#define NK_MAX_VERTEX_MEMORY    512 * 1024
#define NK_MAX_ELEMENT_MEMORY   128 * 1024


typedef struct Camera
{
  vec3 pos;
  float yaw;
  float roll;
  float pitch;
} Camera;

typedef struct Controls
{
  bool32 lmbState;
  bool32 rmbState;
  uint16 mouseX;
  uint16 mouseY;
  float mouseSensitivity;
} Controls;

typedef struct State
{
  bool32 drawGUI;
  GLuint drawMode;
  bool32 running;
} State;

typedef struct GLAtom
{
  GLuint vao;
  GLuint vboVertices;
  GLuint vboVerticesColors;
  GLuint vboVerticesUVs;
  GLuint eboIndices;
  GLuint texture;
} GLAtom;

#define INIT_WINDOW_WIDTH    800
#define INIT_WINDOW_HEIGHT   800

typedef struct WindowParams
{
  uint16 width;
  uint16 height;
} WindowParams;

global_variable Camera camera = {};
global_variable Controls controls = {};
global_variable Controls controlsPrev = {};


global_variable uint64 perfCountFrequency;


void
SignalHandler(int signal)
{
  if (signal == SIGINT || SIGTERM) {
    SDL_Log("Received signal %d", signal);
    exit(EXIT_SUCCESS);
  }
}


global_variable GLAtom glAtom = {};

void
UpdateViewMatrix(Camera* cam, mat4 view)
{
  vec3 trgt = {0.0f, 0.0f, 0.0f};
  vec3 camUpVector = {0.0f, 1.0f, 0.0f};
  float r = 1.0f;
  UNUSED(r);
  camera.pos[0] = cosf(camera.yaw) * cosf(camera.pitch);
  camera.pos[1] = sinf(camera.pitch) * sinf(camera.yaw);
  camera.pos[2] = cosf(camera.pitch) * sinf(camera.yaw);
  glm_lookat(camera.pos, trgt, camUpVector, view);
}


static const char *TYPE_TO_STRING[] = { "Unknown", "Empty", "Model", "Mesh" };
static uint32 depth = 0;

static void print_info_about_model(const fbx_model_t *model)
{
  /* TODO(mtwilliams): Use helpers that deal with inheritance mode. */

  const fbx_vec3_t p = model->transform.position;
  const fbx_quaternion_t r = model->transform.rotation;
  const fbx_vec3_t s = model->transform.scale;

  printf("Positioned at <%f, %f, %f> relative to parent.\n", p.x, p.y, p.z);
  printf("Rotated with <%f, %f, %f, %f> relative to parent.\n", r.x, r.y, r.z, r.w);
  printf("Scaled by <%f, %f, %f> relative to parent.\n", s.x, s.y, s.z);
}

static void print_info_about_mesh(const fbx_mesh_t *mesh)
{
  printf("Composed of %u triangles corresponding to %u vertices.\n", mesh->num_of_faces, mesh->num_of_vertices);
}

static void walk_and_print(const fbx_object_t *object)
{
  printf("%s (%llu)\n", TYPE_TO_STRING[object->type], object->id);

  if (object->name)
    printf("Called \"%s\".\n", object->name);

  switch (object->type) {
    case FBX_MODEL: print_info_about_model((const fbx_model_t *)object->reified); break;
    case FBX_MESH: print_info_about_mesh((const fbx_mesh_t *)object->reified); break;
  }

  for (fbx_uint32_t child = 0; child < object->num_of_children; ++child) {
    depth++;
    walk_and_print(object->children[child]);
    depth--;
  }
}


int
main(int argc, char *argv[])
{
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  Memory mem = {};
  if (!mem.isInitialized)
  {
    mem.isInitialized = true;
    mem.transientMemorySize = Megabytes(8);
    mem.persistentMemorySize = Megabytes(128);
    // TODO(michalc): need a better/cross platform malloc?
    mem.transientMemory = malloc(mem.transientMemorySize);
    mem.transientTail = mem.transientMemory;
    mem.persistentMemory = malloc(mem.persistentMemorySize);
    mem.persistentTail = mem.persistentMemory;
  }
  AssetTable assetTable = {};
  assetTable.storageMemory = &mem;

  LoadAsset("./resources/cube.obj", &assetTable, ASSET_MODEL3D_OBJ);
  Model3D modelOBJ = {};
  RetriveOBJ(0, &assetTable, &modelOBJ);

  char* fontPath = "./resources/fonts/OpenSans-Regular.ttf";
  LoadFont(fontPath);
  GetTextImage("All is well");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    SDL_Log("Failed to SDL_Init");
    exit(EXIT_FAILURE);
  }

  GetDisplayInformation();
  perfCountFrequency = SDL_GetPerformanceFrequency();

  // Need to set some SDL specific things for OpenGL
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  if (SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  if (SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  if (SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  if (SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 8)) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  SDL_Window* window = SDL_CreateWindow(
    "My game",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    INIT_WINDOW_WIDTH, INIT_WINDOW_HEIGHT,
    SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI
  );

  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if (glContext == NULL) {
    SDL_Log("%s", SDL_GetError());
    exit(EXIT_FAILURE);
  }
  glViewport(0, 0, INIT_WINDOW_WIDTH, INIT_WINDOW_HEIGHT);

  glewExperimental = GL_TRUE;
  glewInit();  // glew will look up OpenGL functions
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  //glEnable(GL_CULL_FACE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // NUKLEAR init
  nk_context* nkCtx;
  nkCtx = nk_sdl_init(window);
  nk_font_atlas *atlas;
  nk_sdl_font_stash_begin(&atlas);
  nk_sdl_font_stash_end();

  ////////////////////////////////////////
  //
  // FBX
  //

  ListDirectoryParams filesListParams = {};
  memcpy(filesListParams.path, "./resources\0", strlen("./resources\0"));
  filesListParams.filesList = (char*)(mem.transientMemory);
  memcpy(filesListParams.filter, ".fbx\0", strlen(".fbx\0"));
  filesListParams.filesCount = 0;

  bool32 refreshFilesListPeriodically = true;

  uint32 fbxFilesCount = ListDirectory(filesListParams.path,
                                       (char*)(mem.transientMemory),
                                       filesListParams.filter);
  filesListParams.filesCount = fbxFilesCount;

  // TODO(michalc): this can't be just 8
  char fbxFiles[8][256];
  char* cursor = (char*)mem.transientMemory;

  for (uint32 i = 0; i < fbxFilesCount; ++i)
  {
    memcpy(fbxFiles[i], "./resources/", strlen("./resources/"));
    strncpy(fbxFiles[i] + strlen("./resources/"), cursor, 256);
    cursor += strlen(cursor) + 1;
  }

  // Read 1 MB to compute the hash.
  FILE* fbxFile = fopen(fbxFiles[0], "r");
  fread(mem.transientMemory, 1, Megabytes(1), fbxFile);
  fclose(fbxFile);

  meow_hash mHash = MeowHash_Accelerated(0, Megabytes(1), mem.transientMemory);
  //printf(" [I] Meowhash result: %llx-%llx\n", mHash[0], mHash[1]);

  fbx_import_options_t options;

  options.permanent_memory_pool = Megabytes(2);
  options.transient_memory_pool = Megabytes(1);
  options.strings_memory_pool = Megabytes(1);

  static const char* fbx_path = fbxFiles[0];
  fbx_importer_t *importer = fbx_importer_setup(&options);

  importer->stream = fbx_stream_open_from_path(fbxFiles[0], "r");

  if (!importer->stream) {
    fprintf(stderr, " [E] Cannot open \"%s\"!\n", fbx_path);
    return EXIT_FAILURE;
  }

  printf(" [I] Loading \"%s\"... ", fbx_path);

  if (fbx_importer_run(importer) != FBX_OK)
  {
    fprintf(stderr, "\n [E] Failed to load: %s\n", fbx_path);
  }
  else
  {
    printf("OK\n");
  }

  fbx_stream_close(importer->stream);

  fbx_size_t memory_used_for_import = importer->memory.permanent.offset
                                    + importer->memory.transient.offset
                                    + importer->memory.strings.offset;

  const fbx_scene_t *scene = &importer->fbx->scene;

  fbx_uint32_t num_of_empties = 0;
  fbx_uint32_t num_of_models = 0;
  fbx_uint32_t num_of_meshes = 0;

  for (fbx_uint32_t object = 0; object < scene->num_of_objects; ++object) {
    switch (scene->objects[object]->type) {
      case FBX_EMPTY: num_of_empties += 1; break;
      case FBX_MODEL: num_of_models += 1; break;
      case FBX_MESH: num_of_meshes += 1; break;
    }
  }

  printf("Scene is composed of %u objects.\n"
         " There are %u empties.\n"
         " There are %u models.\n"
         " There are %u meshes.\n",
         importer->fbx->scene.num_of_objects,
         num_of_empties,
         num_of_models,
         num_of_meshes);

  printf("Hierarchy:\n");

  walk_and_print(scene->root);

  //
  // FBX
  //
  ////////////////////////////////////////

  controls.mouseSensitivity = 0.01f;

  // model matrix holds, translation, scaling, rotation
  mat4 model;
  glm_mat4_identity(model);
  glm_scale_uni(model, 0.7f);

  // view matrix represents point of view in the scene - camera
  camera.pos[0] = 0.00f;
  camera.pos[1] = 0.00f;
  camera.pos[2] = 0.30f;
  camera.yaw = M_PI/2.0f;
  camera.pitch = 0.00f;
  camera.roll = 0.00f;
  mat4 view;
  UpdateViewMatrix(&camera, view);

  // projection holds the ortho/persp + frustrum
  mat4 projection;
#ifdef NOT_USING_PERSPECTIVE
  glm_perspective(90.0f,
                  1.0f,
                  CUSTOM_3D_SPACE_MIN_Z,
                  CUSTOM_3D_SPACE_MAX_Z,
                  projection);
#endif
  glm_ortho(
    CUSTOM_3D_SPACE_MIN_X,
    CUSTOM_3D_SPACE_MAX_X,
    CUSTOM_3D_SPACE_MIN_Y,
    CUSTOM_3D_SPACE_MAX_Y,
    CUSTOM_3D_SPACE_MIN_Z,
    CUSTOM_3D_SPACE_MAX_Z,
    projection);

  //
  // Set things UP.
  //
  ////////////////////////////////////////
  //
  // Prepare and LOOP.
  //

  uint8 framerateTarget = 60;
  float frametimeTarget = 1.0f/framerateTarget;

  WindowParams windowParameters = {};
  windowParameters.width = INIT_WINDOW_WIDTH;
  windowParameters.height = INIT_WINDOW_HEIGHT;
  State appState = {};
  appState.drawGUI = true;
  appState.drawMode = GL_FILL;
  appState.running = true;
  SDL_Event event;

  int wireframeToggler = 0;

  uint64 lastCounter = SDL_GetPerformanceCounter();
  uint64 lastCycleCount = __rdtsc();

  while (appState.running) {
    nk_input_begin(nkCtx);

    // EVENTS
    while (SDL_PollEvent(&event))
    {
      switch (event.type)
      {
        case SDL_QUIT: {
          appState.running = false;
        } break;

        case SDL_WINDOWEVENT: {
        } break;

        case SDL_KEYDOWN: {
          switch (event.key.keysym.sym)
          {
            case SDLK_ESCAPE: {
              appState.running = false;
            } break;

            case SDLK_g: {
              appState.drawGUI = !appState.drawGUI;
            } break;

            case SDLK_t: {
              glClearColor(0.0, 0.0, 0.0, 1.0);
              glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
              appState.drawMode = ((wireframeToggler++)%2) ? GL_FILL : GL_LINE;
              glPolygonMode(GL_FRONT_AND_BACK, appState.drawMode);
              glDrawElements( GL_TRIANGLES, modelOBJ.verticesCount, GL_UNSIGNED_INT, 0);
              SDL_GL_SwapWindow(window);
            } break;

            case SDLK_w: {
            } break;

            default: break;
          }
        } break;  // SDL_KEYDOWN

        case SDL_MOUSEMOTION: {
        } break;

        case SDL_MOUSEBUTTONDOWN: {
          switch (event.button.button)
          {
            case SDL_BUTTON_LEFT: {
              controls.lmbState = true;
            } break;
            default: break;
          }
        } break;

        case SDL_MOUSEBUTTONUP: {
          switch (event.button.button)
          {
            case SDL_BUTTON_LEFT: {
              controls.lmbState = false;
            } break;
            default: break;
          }
        } break;

        case SDL_MOUSEWHEEL: {
        } break;

        default: break;
      }  // switch (event.type)

      nk_sdl_handle_event(&event);
    }  // while(SDL_PollEvent(&event)

    nk_input_end(nkCtx);


    ////////////////////////////////////////
    //
    // Input
    //

    controlsPrev = controls;

    if (refreshFilesListPeriodically)
    {
      SDL_TimerID timerIDFilesListRefresh = SDL_AddTimer(1000, ListDirectoryOnTimer, (ListDirectoryParams*)(&filesListParams));
      refreshFilesListPeriodically = false;
    }

    fbxFilesCount = filesListParams.filesCount;

    // TODO(michalc): there is garbage in transientMemory at this point so you
    // can't expect a proper file list till the first timer fire.
    // Even though ListDirectory already filled in the files list at the beginning.
    // In the meantime transientMemory gets used for something else I guess.
    cursor = (char*)mem.transientMemory;

    for (uint32 i = 0; i < fbxFilesCount; ++i)
    {
      memcpy(fbxFiles[i], "./resources/", strlen("./resources/"));
      strncpy(fbxFiles[i] + strlen("./resources/"), cursor, 256);
      cursor += strlen(cursor) + 1;
    }

    for (uint32 i = 0; i < fbxFilesCount; ++i)
    {
      printf(" --- %s\n", fbxFiles[i]);
    }

    ////////////////////////////////////////
    //
    // Timing
    //

    // TODO(mc): think which real64 can be changed to real32
    uint64 endCounter = SDL_GetPerformanceCounter();
    uint64 counterElapsed = endCounter - lastCounter;
    real64 msPerFrame = (((1000.0f * (real64)counterElapsed) /
                          (real64)perfCountFrequency));
    real64 fps = (real64)perfCountFrequency / (real64)counterElapsed;
    lastCounter = endCounter;

    uint64 endCycleCount = __rdtsc();
    uint64 cyclesElapsed = endCycleCount - lastCycleCount;
    real64 mcpf = ((real64)cyclesElapsed); // / (1000.0f * 1000.0f));
    lastCycleCount = endCycleCount;


    ////////////////////////////////////////
    //
    // Render

    SDL_GetWindowSize(window,
                      (int*)(&(windowParameters.width)),
                      (int*)(&(windowParameters.height))
                      );
    glViewport(0, 0, windowParameters.width, windowParameters.height);

    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // check NK_API int nk_window_is_closed(struct nk_context*, const char*);
    if (appState.drawGUI)
    {
      static nk_flags window_flags = 0;
      window_flags = NK_WINDOW_BORDER |
                     NK_WINDOW_MOVABLE |
                     NK_WINDOW_SCALABLE |
                     NK_WINDOW_CLOSABLE |
                     NK_WINDOW_MINIMIZABLE |
                     NK_WINDOW_TITLE;
      if (nk_begin(nkCtx, "FBX Analyzer", nk_rect(10, 10, 500, 500), window_flags))
      {
        nk_layout_row_dynamic(nkCtx, 20, 2);
        nk_label(nkCtx, "FPS (ms per frame)", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT,
                  "%.3f (%.3f)", fps, msPerFrame);
        nk_label(nkCtx, "Mili cycles per frame", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT,
                  "%.3f", mcpf);
        nk_layout_row_dynamic(nkCtx, 20, 2);
        nk_label(nkCtx, "Camera pos", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT,
                  "(%.3f, %.3f, %.3f)",
                  camera.pos[0], camera.pos[1], camera.pos[2]);
        nk_layout_row_dynamic(nkCtx, 20, 2);
        nk_label(nkCtx, "Camera spherical (P,R,Y)", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT,
                  "(%.3f, %.3f, %.3f)",
                  camera.pitch, camera.roll, camera.yaw);
        nk_label(nkCtx, "FBX file", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT, "%s", fbx_path);
        nk_label(nkCtx, "FBX version", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT, "%u", importer->fbx->version);
        nk_label(nkCtx, "Created at", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT,
                  "%4u/%02u/%02u %02u:%02u:%02u:%04u",
                  importer->fbx->timestamp.year,
                  importer->fbx->timestamp.month,
                  importer->fbx->timestamp.day,
                  importer->fbx->timestamp.hour,
                  importer->fbx->timestamp.minute,
                  importer->fbx->timestamp.second,
                  importer->fbx->timestamp.millisecond);
        nk_label(nkCtx, "Created with", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT, "%s (%s) by %s",
                  importer->fbx->tool.name,
                  importer->fbx->tool.version,
                  importer->fbx->tool.vendor);
        nk_label(nkCtx, "Used bytes for import", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT, "%u B", memory_used_for_import);
        nk_label(nkCtx, "Up", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT, "<%1.0f, %1.0f, %1.0f>",
                  importer->fbx->basis.up.x,
                  importer->fbx->basis.up.y,
                  importer->fbx->basis.up.z);
        nk_label(nkCtx, "Forward", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT, "<%1.0f, %1.0f, %1.0f>",
                  importer->fbx->basis.forward.x,
                  importer->fbx->basis.forward.y,
                  importer->fbx->basis.forward.z);
        nk_label(nkCtx, "Right", NK_TEXT_LEFT);
        nk_labelf(nkCtx, NK_TEXT_RIGHT, "<%1.0f, %1.0f, %1.0f>",
                  importer->fbx->basis.right.x,
                  importer->fbx->basis.right.y,
                  importer->fbx->basis.right.z);
      }
      nk_end(nkCtx);

      nk_sdl_render(NK_ANTI_ALIASING_ON, NK_MAX_VERTEX_MEMORY, NK_MAX_ELEMENT_MEMORY);
    }

    SDL_GL_SwapWindow(window);

    SDL_Delay(10);

    /*
    // TODO(mc): add sleep for the rest of fixed frame time
    if (there is more time in this frame computing) {
      sleep
    }
    else {
      drop the target framerate
    }
    */

  }   // <---- END OF running WHILE

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDeleteBuffers(1, &glAtom.vboVertices);
  glDeleteBuffers(1, &glAtom.eboIndices);

  SDL_Quit();
  return 0;
}
