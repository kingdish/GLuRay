// #include <optixu/optixpp_namespace.h>
// #include <optixu/optixu_math_namespace.h>
// #include <optixu/optixu_aabb_namespace.h>
// #include <sutil.h>
// #include <GLUTDisplay.h>
// #include <PlyLoader.h>
// #include <ObjLoader.h>
// #include "commonStructs.h"
// #include <string>
// #include <iostream>
// #include <fstream>
// #include <cstdlib>
// #include <cstring>
// #include "random.h"
// #include "MeshScene.h"




#include <UseMPI.h>
#ifdef USE_MPI
#include <Engine/Display/NullDisplay.h>
#include <Engine/LoadBalancers/MPI_LoadBalancer.h>
#include <Engine/ImageTraversers/MPI_ImageTraverser.h>
#include <mpi.h>
#endif

#include "defines.h"
#include "OptiXManager.h"

#include <sutil.h>
#include <glm.h>
#include <ImageLoader.h>
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_aabb_namespace.h>
#include <optixu/optixu_matrix_namespace.h>
#include <sample6/random.h>

//#include <X11/Xlib.h>
//#include <X11/Xutil.h>

//#include "../gl_functions.h"
#include "CDTimer.h"
#include "OScene.h"
#include "ORenderable.h"
#include "common.h"
#include <Modules/Manta/AccelWork.h>
#include <OBJScene.h>

#include <Core/Util/Logger.h>

#include <Model/Primitives/KenslerShirleyTriangle.h>
#include <Engine/PixelSamplers/RegularSampler.h>
#include <Engine/Display/FileDisplay.h>
#include <Image/SimpleImage.h>
#include <Model/Materials/Dielectric.h>
#include <Model/Materials/ThinDielectric.h>
#include <Model/Materials/OrenNayar.h>
#include <Model/Materials/Transparent.h>
#include <Model/AmbientLights/AmbientOcclusionBackground.h>
#include <Model/AmbientLights/AmbientOcclusion.h>
#include <Model/Textures/TexCoordTexture.h>
#include <Model/Textures/Constant.h>
#include <Model/Primitives/Plane.h>
#include <Model/Primitives/Parallelogram.h>
#include <Model/Primitives/Cube.h>
#include <Model/Primitives/Disk.h>



#include <stdio.h>
#include <cassert>
#include <float.h>
#include <stack>
#include <algorithm>

#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>



#include <arpa/inet.h>

#include <GL/gl.h>

#define USE_AO 0

using namespace optix;

std::string ptxPath( const std::string& base )
{
  return std::string("/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/") + std::string("/sample5pp_generated_") + base + std::string(".ptx");
}

std::string ptxpath(std::string base, std::string subset)
{
  return std::string("/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/") + base + std::string("_generated_") + subset + std::string(".ptx");
}

OptiXManager* OptiXManager::_singleton = NULL;

OptiXManager* OptiXManager::singleton()
{
  if (!_singleton)
    _singleton = new OptiXManager();
  return _singleton;
}


OptiXManager::OptiXManager()
:RenderManager(), current_scene(NULL), next_scene(NULL),
_nid_counter(0), _depth(false), _width(0), _height(0), _frameNumber(0), _realFrameNumber(0)
{
  initialized=false;
  printf("%s::%s\n",typeid(*this).name(),__FUNCTION__);
  _format = "RGBA8";

  _gVoid = new OGeometryGeneratorVoid();
  _gTriangle = new OGeometryGeneratorTriangles();
  _gTriangleStrip = new OGeometryGeneratorTriangleStrip();
  _gQuads = new OGeometryGeneratorQuads();
  _gQuadStrip = new OGeometryGeneratorQuadStrip();
  _gLines = new OGeometryGeneratorLines();
  _gLineStrip= new OGeometryGeneratorLineStrip();

}

OptiXManager::~OptiXManager()
{
}

void OptiXManager::updateLights()
{

}

Renderable* OptiXManager::createRenderable(GeometryGenerator* gen)
{
  OGeometryGenerator* mg = dynamic_cast<OGeometryGenerator*>(gen);
  assert(mg);
  return new ORenderable(mg);
}

void  OptiXManager::updateMaterial()
{
  if (!initialized)
    return;
  GLMaterial m = gl_material;
  //TODO: DEBUG: hardcoding mat for debugging
  // m.diffuse = Color(RGB(1.0, 0.713726, .21569));
  //m.diffuse = Color(RGB(0.8, 0.8, 0.8));
  // m.specular = Manta::Color(RGB(.1, .1, .1));
  // m.ambient = Manta::Color(RGB(0, 0, 0));
  // m.shiny = 100;

}

void OptiXManager::useShadows(bool st)
{
}

void OptiXManager::setSize(int w, int h)
{

  printf("setSize %d %d\n", w,h);
  if (initialized && (w != _width || h != _height))
  {
    _width = w; _height = h;
    updateCamera();
    #if USE_AO
      if( m_rnd_seeds.get() == 0 ) {
    m_rnd_seeds = context->createBuffer( RT_BUFFER_INPUT_OUTPUT | RT_BUFFER_GPU_LOCAL, RT_FORMAT_UNSIGNED_INT,
                                         w, h);
    context["rnd_seeds"]->setBuffer(m_rnd_seeds);
  }

  unsigned int* seeds = static_cast<unsigned int*>( m_rnd_seeds->map() );
  fillRandBuffer(seeds, w*h);
  m_rnd_seeds->unmap();
    #endif

  }
}



void OptiXManager::init()
{
  if (initialized)
    return;
  initialized=true;
  printf("%s::%s\n",typeid(*this).name(),__FUNCTION__);

  updateBackground();
  updateCamera();
  updateMaterial();
  if (!current_scene)
    current_scene = new OScene();
  if (!next_scene)
    next_scene = new OScene();


  int width, height;
width=_width;
height=_height;
 bool m_accum_enabled = false;
  enum CameraMode
  {
    CM_PINHOLE=0,
    CM_ORTHO
  }; 
 int m_camera_mode  = CM_PINHOLE;

   // Set up context
  context = optix::Context::create();
  // context->setRayTypeCount( 1 );
  // context->setEntryPointCount( 1 );
    context->setRayTypeCount( 3 );
  context->setEntryPointCount( 1 );
  context->setStackSize( 1180 );

    context[ "scene_epsilon"      ]->setFloat( .01 );
  context[ "occlusion_distance" ]->setFloat( .01 );

  context[ "radiance_ray_type"   ]->setUint( 0u );
  context[ "shadow_ray_type"     ]->setUint( 1u );
  context[ "max_depth"           ]->setInt( 5 );
  context[ "ambient_light_color" ]->setFloat( 0.2f, 0.2f, 0.2f );
  // context[ "output_buffer"       ]->set( createOutputBuffer(RT_FORMAT_UNSIGNED_BYTE4, width, height) );
  context[ "jitter_factor"       ]->setFloat( m_accum_enabled ? 1.0f : 0.0f );


  const std::string camera_name = m_camera_mode == CM_PINHOLE ? "pinhole_camera" : "orthographic_camera"; 
  const std::string camera_file = m_accum_enabled             ? "accum_camera.cu" :
                                  m_camera_mode == CM_PINHOLE ? "pinhole_camera.cu"  :
                                                               "orthographic_camera.cu";

    const std::string camera_ptx  = ptxpath( "sample6", camera_file );
  Program ray_gen_program = context->createProgramFromPTXFile( camera_ptx, camera_name );
  context->setRayGenerationProgram( 0, ray_gen_program );


    // Exception program
  const std::string except_ptx  = ptxpath( "sample6", camera_file );
  context->setExceptionProgram( 0, context->createProgramFromPTXFile( except_ptx, "exception" ) );
  context[ "bad_color" ]->setFloat( 0.0f, 1.0f, 0.0f );


  // Miss program 
  const std::string miss_ptx = ptxpath( "sample6", "constantbg.cu" );
  context->setMissProgram( 0, context->createProgramFromPTXFile( miss_ptx, "miss" ) );
  context[ "bg_color" ]->setFloat(  0.34f, 0.55f, 0.85f );


typedef struct struct_BasicLight
{
#if defined(__cplusplus)
  typedef optix::float3 float3;
#endif
  float3 pos;
  float3 color;
  int    casts_shadow; 
  int    padding;      // make this structure 32 bytes -- powers of two are your friend!
} BasicLight;

int m_light_scale = 1.5;
    // Lights buffer
  BasicLight lights[] = {
    { make_float3( -60.0f,  30.0f, -120.0f ), make_float3( 0.2f, 0.2f, 0.25f )*m_light_scale, 0, 0 },
    { make_float3( -60.0f,   0.0f,  120.0f ), make_float3( 0.1f, 0.1f, 0.10f )*m_light_scale, 0, 0 },
    { make_float3(  60.0f,  60.0f,   60.0f ), make_float3( 0.7f, 0.7f, 0.65f )*m_light_scale, 1, 0 }
  };

  Buffer light_buffer = context->createBuffer(RT_BUFFER_INPUT);
  light_buffer->setFormat(RT_FORMAT_USER);
  light_buffer->setElementSize(sizeof( BasicLight ) );
  light_buffer->setSize( sizeof(lights)/sizeof(lights[0]) );
  memcpy(light_buffer->map(), lights, sizeof(lights));
  light_buffer->unmap();

  context[ "lights" ]->set( light_buffer );

    // RT_CHECK_ERROR( rtBufferCreate( context, RT_BUFFER_OUTPUT, &buffer ) );
    // RT_CHECK_ERROR( rtBufferSetFormat( buffer, RT_FORMAT_FLOAT4 ) );
    // RT_CHECK_ERROR( rtBufferSetSize2D( buffer, width, height ) );
    // RT_CHECK_ERROR( rtContextDeclareVariable( context, "result_buffer", &result_buffer ) );
    // RT_CHECK_ERROR( rtVariableSetObject( result_buffer, buffer ) );

    // sprintf( path_to_ptx, "%s/%s", sutilSamplesPtxDir(), "sample1_generated_draw_color.cu.ptx" );
    // printf("path_to_ptx: %s\n", path_to_ptx);
    // RT_CHECK_ERROR( rtProgramCreateFromPTXFile( context, "/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/sample1_generated_draw_color.cu.ptx", "draw_solid_color", &ray_gen_program ) );
    // RT_CHECK_ERROR( rtProgramDeclareVariable( ray_gen_program, "draw_color", &draw_color ) );
    // RT_CHECK_ERROR( rtVariableSet3f( draw_color, 0.462f, 0.725f, 0.0f ) );
    // RT_CHECK_ERROR( rtContextSetRayGenerationProgram( context, 0, ray_gen_program ) );

    /* Run */
    // RT_CHECK_ERROR( rtContextValidate( context ) );
    // RT_CHECK_ERROR( rtContextCompile( context ) );
    // RT_CHECK_ERROR( rtContextLaunch2D( context, 0 /* entry point */, width, height ) );





}

//TODO: updating pixelsampler mid flight crashes manta
void OptiXManager::setNumSamples(int,int,int samples)
{
}

void OptiXManager::setNumThreads(int t)
{
}

size_t OptiXManager::generateNID()
{
  return 0;
  // return ++_nid_counter;
}

Renderable* OptiXManager::getRenderable(size_t nid)
{
  return _map_renderables[nid];
}

void* OptiXManager::renderLoop(void* t)
{
}

void OptiXManager::internalRender()
{
}


void OptiXManager::render()
{
  printf("optix render\n");
  if (!initialized)
    return;
  if (next_scene->instances.size() == 0)
    return;


  //JOAO: put render call here

  displayFrame();


// int width = _width;
// int height = _height;


  int width, height;
width=_width;
height=_height;
 bool m_accum_enabled = false;
  enum CameraMode
  {
    CM_PINHOLE=0,
    CM_ORTHO
  }; 
 int m_camera_mode  = CM_PINHOLE;


optix::Variable output_buffer = context["output_buffer"];
  buffer = context->createBuffer( RT_BUFFER_OUTPUT, RT_FORMAT_UNSIGNED_BYTE4, width, height );
  output_buffer->set(buffer);

  // Ray generation program
  // std::string ptx_path( ptxPath( "pinhole_camera.cu" ) );
  // std::cout << "ptx_path: " << ptx_path << std::endl;
  // Program ray_gen_program = context->createProgramFromPTXFile( ptx_path, "pinhole_camera" );
  // context->setRayGenerationProgram( 0, ray_gen_program );


 GLuRayRenderParameters& p = params;
  optix::float3 cam_eye = { p.camera_eye.x(), p.camera_eye.y(), p.camera_eye.z() };
  optix::float3 lookat  = { p.camera_dir.x()+p.camera_eye.x(), p.camera_dir.y()+p.camera_eye.y(), p.camera_dir.z()+p.camera_eye.z() };
  optix::float3 up      = { p.camera_up.x(), p.camera_up.y(), p.camera_up.z() };
  float  hfov    = p.camera_vfov;
  float  aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
  optix::float3 camera_u, camera_v, camera_w;
  sutilCalculateCameraVariables( &cam_eye.x, &lookat.x, &up.x, hfov, aspect_ratio,
                                 &camera_u.x, &camera_v.x, &camera_w.x );

  context["eye"]->setFloat( cam_eye );
  context["U"]->setFloat( camera_u );
  context["V"]->setFloat( camera_v );
  context["W"]->setFloat( camera_w );


  // Exception program
  // optix::Program exception_program = context->createProgramFromPTXFile( ptx_path, "exception" );
  // context->setExceptionProgram( 0, exception_program );
  // context["bad_color"]->setFloat( 1.0f, 1.0f, 0.0f );

  // // Miss program
  // ptx_path = ptxPath( "constantbg.cu" );
  // context->setMissProgram( 0, context->createProgramFromPTXFile( ptx_path, "miss" ) );
  // context["bg_color"]->setFloat( 0.3f, 0.1f, 0.2f );

    optix::Geometry sphere = context->createGeometry();
  sphere->setPrimitiveCount( 1u );
  sphere->setBoundingBoxProgram( context->createProgramFromPTXFile( ptxPath( "sphere.cu" ), "bounds" ) );
  sphere->setIntersectionProgram( context->createProgramFromPTXFile( ptxPath( "sphere.cu" ), "intersect" ) );
  sphere["sphere"]->setFloat( 0, 0, 0, 1.5 );

//create material
    optix::Program chp = context->createProgramFromPTXFile( ptxPath( "normal_shader.cu" ), "closest_hit_radiance" );

  optix::Material material = context->createMaterial();
  material->setClosestHitProgram( 0, chp );

    // Create geometry instance
  optix::GeometryInstance gi = context->createGeometryInstance();
  gi->setMaterialCount( 1 );
  gi->setGeometry( sphere );
  gi->setMaterial( 0, material );

  // Create geometry group
  optix::Group top_group = context->createGroup();
  int validChildren=0;
  int childIndex=0;
    for(vector<GRInstance>::iterator itr = next_scene->instances.begin(); itr != next_scene->instances.end(); itr++)
  {
    Manta::AffineTransform mt = itr->transform;
    Renderable* ren = itr->renderable;
    ORenderable* er = dynamic_cast<ORenderable*>(ren);
    if (er->isBuilt())
    {
      validChildren++;
  // geometrygroup->setChild( childIndex++, *(er->gi) );
        //JOAO: add instances to optix scene here
    }
  }
  top_group->setChildCount( validChildren );
      for(vector<GRInstance>::iterator itr = next_scene->instances.begin(); itr != next_scene->instances.end(); itr++)
  {
    Manta::AffineTransform mt = itr->transform;
    Renderable* ren = itr->renderable;
    ORenderable* er = dynamic_cast<ORenderable*>(ren);
    if (er->isBuilt())
    {
// validChildren++;
          optix::Transform transform = context->createTransform();
    // optix::GeometryGroup gg = context->createGeometryGroup();
    // gg->setChildCount(1);
    // gg->setChild(0, *(er->gi));
    transform->setChild(er->gg);
    // gg->setAcceleration( context->createAcceleration("NoAccel","NoAccel") );
    // group->setChild( i, transform );
    // float m[16] = {mt(0,0),mt(0,1),mt(0,2),mt(0,3),
    //                mt(1,0),mt(1,1),mt(1,2),mt(1,3),
    //                mt(2,0),mt(2,1),mt(2,2),mt(2,3),
    //                mt(3,0),mt(3,1),mt(3,2),mt(3,3) };
    float m[16] = {mt(0,0),mt(0,1),mt(0,2),mt(0,3),
                   mt(1,0),mt(1,1),mt(1,2),mt(1,3),
                   mt(2,0),mt(2,1),mt(2,2),mt(2,3),
                   0,0,0,1 };
    // float m[16] = {1,0,0,0,
    //                0,1,0,0,
    //                0,0,1,0,
    //                0,0,0,1 };

    transform->setMatrix( false, m, NULL );

      top_group->setChild( childIndex++, transform);
        //JOAO: add instances to optix scene here
    }
  }
  next_scene->instances.resize(0);

  top_group->setAcceleration( context->createAcceleration("NoAccel","NoAccel") );

  context["top_object"]->set( top_group );
  context[ "top_shadower" ]->set( top_group );
  
  context->validate();
  context->compile();

  //launch

    // Run
  context->launch( 0, width, height );


  buffer = context["output_buffer"]->getBuffer();

GLvoid* imageData = buffer->map();
  RTformat buffer_format = buffer->getFormat();
    assert( imageData );

    GLenum gl_data_type = GL_FALSE;
    GLenum gl_format = GL_FALSE;

    switch (buffer_format) {
          case RT_FORMAT_UNSIGNED_BYTE4:
            gl_data_type = GL_UNSIGNED_BYTE;
            gl_format    = GL_BGRA;
            break;

          case RT_FORMAT_FLOAT:
            gl_data_type = GL_FLOAT;
            gl_format    = GL_LUMINANCE;
            break;

          case RT_FORMAT_FLOAT3:
            gl_data_type = GL_FLOAT;
            gl_format    = GL_RGB;
            break;

          case RT_FORMAT_FLOAT4:
            gl_data_type = GL_FLOAT;
            gl_format    = GL_RGBA;
            break;

          default:
            fprintf(stderr, "Unrecognized buffer data type or format.\n");
            exit(2);
            break;
    }

    RTsize elementSize = buffer->getElementSize();
    int align = 1;
    if      ((elementSize % 8) == 0) align = 8; 
    else if ((elementSize % 4) == 0) align = 4;
    else if ((elementSize % 2) == 0) align = 2;
    glPixelStorei(GL_UNPACK_ALIGNMENT, align);

    // NVTX_RangePushA("glDrawPixels");
    glDrawPixels( static_cast<GLsizei>( width ), static_cast<GLsizei>( height ),
      gl_format, gl_data_type, imageData);
    // NVTX_RangePop();

    buffer->unmap();


  // Ray generation program
  // std::string ptx_path( ptxPath( "pinhole_camera.cu" ) );
  // std::string ptx_path("/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/pinhole_camera.cu");
  // optix::Program ray_gen_program = context->createProgramFromPTXFile( ptx_path, "pinhole_camera" );
  // context->setRayGenerationProgram( 0, ray_gen_program );

  // optix::float3 cam_eye = { 0.0f, 0.0f, 5.0f };
  // optix::float3 lookat  = { 0.0f, 0.0f, 0.0f };
  // optix::float3 up      = { 0.0f, 1.0f, 0.0f };
  // float  hfov    = 60.0f;
  // float  aspect_ratio = static_cast<float>(width) / static_cast<float>(height);
  // optix::float3 camera_u, camera_v, camera_w;
  // // sutilCalculateCameraVariables( &cam_eye.x, &lookat.x, &up.x, hfov, aspect_ratio,
  // //                                &camera_u.x, &camera_v.x, &camera_w.x );

  // context["eye"]->setFloat( cam_eye );
  // context["U"]->setFloat( camera_u );
  // context["V"]->setFloat( camera_v );
  // context["W"]->setFloat( camera_w );

  // // Exception program
  // optix::Program exception_program = context->createProgramFromPTXFile( ptx_path, "exception" );
  // context->setExceptionProgram( 0, exception_program );
  // context["bad_color"]->setFloat( 1.0f, 1.0f, 0.0f );

  // // Miss program
  // ptx_path = string( "/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/constantbg.cu" );
  // context->setMissProgram( 0, context->createProgramFromPTXFile( ptx_path, "miss" ) );
  // context["bg_color"]->setFloat( 0.3f, 0.1f, 0.2f );

  #if 0


      /* Primary RTAPI objects */

    /* Parameters */
    RTvariable result_buffer;
    RTvariable draw_color;

    char path_to_ptx[512];
    char outfile[512];

    unsigned int width  = 512u;
    unsigned int height = 384u;
    int i;
    int use_glut = 1;

    outfile[0] = '\0';

    /* If "--file" is specified, don't do any GL stuff */
    // for( i = 1; i < argc; ++i ) {
    //   if( strcmp( argv[i], "--file" ) == 0 || strcmp( argv[i], "-f" ) == 0 )
    //     use_glut = 0;
    // }

    // /* Process command line args */
    // if( use_glut )
    //   RT_CHECK_ERROR_NO_CONTEXT( sutilInitGlut( &argc, argv ) );
    // for( i = 1; i < argc; ++i ) {
    //   if( strcmp( argv[i], "--help" ) == 0 || strcmp( argv[i], "-h" ) == 0 ) {
    //     printUsageAndExit( argv[0] );
    //   } else if( strcmp( argv[i], "--file" ) == 0 || strcmp( argv[i], "-f" ) == 0 ) {
    //     if( i < argc-1 ) {
    //       strcpy( outfile, argv[++i] );
    //     } else {
    //       printUsageAndExit( argv[0] );
    //     }
    //   } else if ( strncmp( argv[i], "--dim=", 6 ) == 0 ) {
    //     const char *dims_arg = &argv[i][6];
    //     if ( sutilParseImageDimensions( dims_arg, &width, &height ) != RT_SUCCESS ) {
    //       fprintf( stderr, "Invalid window dimensions: '%s'\n", dims_arg );
    //       printUsageAndExit( argv[0] );
    //     }
    //   } else {
    //     fprintf( stderr, "Unknown option '%s'\n", argv[i] );
    //     printUsageAndExit( argv[0] );
    //   }
    // }

    /* Create our objects and set state */
    RT_CHECK_ERROR( rtContextCreate( &context ) );
    RT_CHECK_ERROR( rtContextSetRayTypeCount( context, 1 ) );
    RT_CHECK_ERROR( rtContextSetEntryPointCount( context, 1 ) );

    RT_CHECK_ERROR( rtBufferCreate( context, RT_BUFFER_OUTPUT, &buffer ) );
    RT_CHECK_ERROR( rtBufferSetFormat( buffer, RT_FORMAT_FLOAT4 ) );
    RT_CHECK_ERROR( rtBufferSetSize2D( buffer, width, height ) );
    RT_CHECK_ERROR( rtContextDeclareVariable( context, "result_buffer", &result_buffer ) );
    RT_CHECK_ERROR( rtVariableSetObject( result_buffer, buffer ) );

    sprintf( path_to_ptx, "%s/%s", sutilSamplesPtxDir(), "sample1_generated_draw_color.cu.ptx" );
    printf("path_to_ptx: %s\n", path_to_ptx);
    RT_CHECK_ERROR( rtProgramCreateFromPTXFile( context, "/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/sample1_generated_draw_color.cu.ptx", "draw_solid_color", &ray_gen_program ) );
    RT_CHECK_ERROR( rtProgramDeclareVariable( ray_gen_program, "draw_color", &draw_color ) );
    RT_CHECK_ERROR( rtVariableSet3f( draw_color, 0.462f, 0.725f, 0.0f ) );
    RT_CHECK_ERROR( rtContextSetRayGenerationProgram( context, 0, ray_gen_program ) );

    /* Run */
    RT_CHECK_ERROR( rtContextValidate( context ) );
    RT_CHECK_ERROR( rtContextCompile( context ) );
    RT_CHECK_ERROR( rtContextLaunch2D( context, 0 /* entry point */, width, height ) );

    // /* Display image */
    // if( strlen( outfile ) == 0 ) {
    //   RT_CHECK_ERROR( sutilDisplayBufferInGlutWindow( argv[0], buffer ) );
    // } else {
    //   RT_CHECK_ERROR( sutilDisplayFilePPM( outfile, buffer ) );
    // }

      RTresult result;
  RTsize buffer_width, buffer_height;
  // int width, height;

  // Set the global RTcontext so we can destroy it at exit
  // if ( context != NULL ) {
  //   fprintf(stderr, "displayGlutWindow called, while another displayGlut is active.  Not supported.");
  //   // return RT_ERROR_UNKNOWN;
  //   return;
  // }
  // rtBufferGetContext( buffer, &context );
  
  // if ( !g_glut_initialized ) {
  //   fprintf(stderr, "displayGlutWindow called before initGlut.");
  //   return RT_ERROR_UNKNOWN;
  // }

  // result = checkBuffer(buffer);
  // if (result != RT_SUCCESS) {
  //   fprintf(stderr, "checkBuffer didn't pass\n");
  //   // return result;
  //   return;
  // }
  RTbuffer imageBuffer = buffer;

  result = rtBufferGetSize2D(buffer, &buffer_width, &buffer_height);
  if (result != RT_SUCCESS) {
    fprintf(stderr, "Error getting dimensions of buffer\n");
    // return result;
    return;
  }
  width  = static_cast<int>(buffer_width);
  height = static_cast<int>(buffer_height);

  //
  // display
  //
  GLvoid* imageData;
  // GLsizei width, height;
  // RTsize buffer_width, buffer_height;
  // RTsize buffer_height;
  RTformat buffer_format;

  result = rtBufferMap(imageBuffer, &imageData);
  if (result != RT_SUCCESS) {
    // Get error from context
    RTcontext context;
    const char* error;
    rtBufferGetContext(imageBuffer, &context);
    rtContextGetErrorString(context, result, &error);
    fprintf(stderr, "Error mapping image buffer: %s\n", error);
    exit(2);
  }
  if (0 == imageData) {
    fprintf(stderr, "data in buffer is null.\n");
    exit(2);
  }

  result = rtBufferGetSize2D(imageBuffer, &buffer_width, &buffer_height);
  if (result != RT_SUCCESS) {
    // Get error from context
    RTcontext context;
    const char* error;
    rtBufferGetContext(imageBuffer, &context);
    rtContextGetErrorString(context, result, &error);
    fprintf(stderr, "Error getting dimensions of buffer: %s\n", error);
    exit(2);
  }
  width  = static_cast<GLsizei>(buffer_width);
  height = static_cast<GLsizei>(buffer_height);

  result = rtBufferGetFormat(imageBuffer, &buffer_format);
  GLenum gl_data_type = GL_FALSE;
  GLenum gl_format = GL_FALSE;
  switch (buffer_format) {
    case RT_FORMAT_UNSIGNED_BYTE4:
      gl_data_type = GL_UNSIGNED_BYTE;
      gl_format    = GL_BGRA;
      break;

    case RT_FORMAT_FLOAT:
      gl_data_type = GL_FLOAT;
      gl_format    = GL_LUMINANCE;
      break;

    case RT_FORMAT_FLOAT3:
      gl_data_type = GL_FLOAT;
      gl_format    = GL_RGB;
      break;

    case RT_FORMAT_FLOAT4:
      gl_data_type = GL_FLOAT;
      gl_format    = GL_RGBA;
      break;

    default:
      fprintf(stderr, "Unrecognized buffer data type or format.\n");
      exit(2);
      break;
  }

  glDrawPixels(width, height, gl_format, gl_data_type, imageData);
  // glutSwapBuffers();

  // Now unmap the buffer
  result = rtBufferUnmap(imageBuffer);
  if (result != RT_SUCCESS) {
    // Get error from context
    RTcontext context;
    const char* error;
    rtBufferGetContext(imageBuffer, &context);
    rtContextGetErrorString(context, result, &error);
    fprintf(stderr, "Error unmapping image buffer: %s\n", error);
    exit(2);
  }


    /* Clean up */
    RT_CHECK_ERROR( rtBufferDestroy( buffer ) );
    RT_CHECK_ERROR( rtProgramDestroy( ray_gen_program ) );
    RT_CHECK_ERROR( rtContextDestroy( context ) );
    #endif

}


void OptiXManager::displayFrame()
{
  char* data = NULL;

  //JOAO: map data to buffer

  if (!data)
    return;
  if (_format == "RGBA8")
    glDrawPixels(_width, _height, GL_RGBA, GL_UNSIGNED_BYTE, data);
  else
    return;

   if (params.write_to_file != "")
  {
    char* rgba_data = (char*)data;
    DEBUG("writing image\n");
    string filename = params.write_to_file;
      // if (params.write_to_file == "generated")
    {
      char cfilename[256];
#if USE_MPI
      sprintf(cfilename, "render_%04d_%dx%d_%d.rgb", _realFrameNumber, _width, _height, _rank);
#else
      sprintf(cfilename, "render_%04d_%dx%d.rgb", _realFrameNumber, _width, _height);
#endif
      filename = string(cfilename);
    }

    printf("writing filename: %s\n", filename.c_str());

      //unsigned char* test = new unsigned char[xres*yres*3];
      //glReadPixels(0,0,xres,yres,GL_RGB, GL_UNSIGNED_BYTE, test);
    FILE* pFile = fopen(filename.c_str(), "w");
    assert(pFile);
    if (_format == "RGBA8")
    {
      fwrite((void*)&rgba_data[0], 1, _width*_height*4, pFile);
      fclose(pFile);
      stringstream s("");
        //TODO: this fudge factor on teh sizes makes no sense... I'm assuming it's because they have row padding in the data but it doesn't show up in drawpixels... perplexing.  It can also crash just a hack for now
      s  << "convert -flip -size " << _width << "x" << _height << " -depth 8 rgba:" << filename << " " << filename << ".png && rm " << filename ;
        /*printf("calling system call \"%s\"\n", s.str().c_str());*/
      system(s.str().c_str());
        //delete []test;

    }
    else
    {
      fwrite(data, 1, _width*_height*3, pFile);
      fclose(pFile);
      stringstream s("");
        //TODO: this fudge factor on teh sizes makes no sense... I'm assuming it's because they have row padding in the data but it doesn't show up in drawpixels... perplexing.  It can also crash just a hack for now
      s << "convert -flip -size " << _width << "x" << _height << " -depth 8 rgb:" << filename << " " << filename << ".png && rm " << filename;
      system(s.str().c_str());
    }
      //delete []test;
    _realFrameNumber++;
  }
}

void OptiXManager::syncInstances()
{}

void OptiXManager::updateCamera()
{
  //JOAO: put camera update here
}

void OptiXManager::updateBackground()
{
}

void OptiXManager::addInstance(Renderable* ren)
{
  if (!ren->isBuilt())
  {
    std::cerr << "addInstance: renderable not build by rendermanager\n";
    return;
  }
  next_scene->instances.push_back(GRInstance(ren, current_transform));
}

void OptiXManager::addRenderable(Renderable* ren)
{
  ORenderable* oren = dynamic_cast<ORenderable*>(ren);
  if (!oren)
  {
    printf("error: OptiXManager::addRenderable wrong renderable type\n");
    return;
  }

  // updateMaterial();
      // msgModel = new miniSG::Model;
      // msgModel->material.push_back(new miniSG::Material);
  // OSPMaterial ospMat = ospNewMaterial(renderer,"OBJMaterial");
  // float Kd[] = {1.f,1.f,1.f};
  // float Ks[] = {1,1,1};

  Manta::Mesh* mesh = oren->_data->mesh;
  // mesh->vertices.resize(0);
  // mesh->vertices.push_back(Manta::Vector(-10,0,0));
  // mesh->vertices.push_back(Manta::Vector(10,0,0));
  // mesh->vertices.push_back(Manta::Vector(0,10,0));
  // mesh->vertices.push_back(Manta::Vector(10,0,0));
  // mesh->vertices.push_back(Manta::Vector(30,0,0));
  // mesh->vertices.push_back(Manta::Vector(20,10,0));
  //   mesh->vertices.push_back(Manta::Vector(30,0,0));
  // mesh->vertices.push_back(Manta::Vector(50,0,0));
  // mesh->vertices.push_back(Manta::Vector(40,10,0));
  // mesh->vertex_indices.resize(0);
  // mesh->vertex_indices.push_back(0);
  // mesh->vertex_indices.push_back(1);
  // mesh->vertex_indices.push_back(2);
  //   mesh->vertex_indices.push_back(3);
  // mesh->vertex_indices.push_back(4);
  // mesh->vertex_indices.push_back(5);
  //     mesh->vertex_indices.push_back(6);
  // mesh->vertex_indices.push_back(7);
  // mesh->vertex_indices.push_back(8);
  assert(mesh);
  size_t numTexCoords = mesh->texCoords.size();
  size_t numPositions = mesh->vertices.size();
  printf("addrenderable called mesh indices/3 vertices normals texcoords: %d %d %d %d \n", mesh->vertex_indices.size()/3, mesh->vertices.size(), mesh->vertexNormals.size(),
    mesh->texCoords.size());
  size_t numTriangles = mesh->vertex_indices.size()/3;
  //
  // hack! 
  //
  if (!mesh->vertexNormals.size())
  {
    for(int i =0; i < numTriangles;i++)
  {
    Manta::Vector v1 = mesh->vertices[mesh->vertex_indices[i*3] ];
    Manta::Vector v2 = mesh->vertices[mesh->vertex_indices[i*3+1] ];
    Manta::Vector v3 = mesh->vertices[mesh->vertex_indices[i*3+2] ];
    Manta::Vector n = Manta::Cross(v2-v1,v3-v1);
    n.normalize();
    mesh->vertexNormals.push_back(n);
    mesh->vertexNormals.push_back(n);
    mesh->vertexNormals.push_back(n);
  }
}
  size_t numNormals = mesh->vertexNormals.size();
  // assert(mesh->vertices.size() == numTriangles*3);


      optix::Geometry sphere = context->createGeometry();
  sphere->setPrimitiveCount( 1u );
  sphere->setBoundingBoxProgram( context->createProgramFromPTXFile( ptxPath( "sphere.cu" ), "bounds" ) );
  sphere->setIntersectionProgram( context->createProgramFromPTXFile( ptxPath( "sphere.cu" ), "intersect" ) );
  sphere["sphere"]->setFloat( 0, 0, 0, 1.5 );


optix::Matrix4x4 transform = optix::Matrix4x4::identity();
  //
  // vertex data
  //
  //   unsigned int num_vertices  = model->numvertices;
  // unsigned int num_texcoords = model->numtexcoords;
  // unsigned int num_normals   = model->numnormals;
  unsigned int num_vertices = numPositions;
  unsigned int num_texcoords = 0;
  unsigned int num_normals = numNormals;

  // Create vertex buffer
  optix::Buffer m_vbuffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, numPositions );
  optix::float3* vbuffer_data = static_cast<optix::float3*>( m_vbuffer->map() );

  // Create normal buffer
  optix::Buffer m_nbuffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, num_normals );
  optix::float3* nbuffer_data = static_cast<optix::float3*>( m_nbuffer->map() );

  // Create texcoord buffer
  optix::Buffer m_tbuffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, num_texcoords );
  optix::float2* tbuffer_data = static_cast<optix::float2*>( m_tbuffer->map() );

  // Transform and copy vertices.  
  for ( unsigned int i = 0; i < num_vertices; ++i )
  {
    // const optix::float3 v3 = *((optix::float3*)&mesh->vertices[(i)*3]);
    const optix::float3 v3 = make_float3(mesh->vertices[(i)].x(), mesh->vertices[(i)].y(), mesh->vertices[(i)].z());
    optix::float4 v4 = make_float4( v3, 1.0f );
    vbuffer_data[i] = optix::make_float3( transform*v4 );
  }

  // Transform and copy normals.
  // const optix::Matrix4x4 norm_transform = transform.inverse().transpose();
  for( unsigned int i = 0; i < num_normals; ++i )
  {
    // const optix::float3 v3 = *((optix::float3*)&mesh->vertexNormals[(i)*3]);
    mesh->vertexNormals[i].normalize();
    const optix::float3 v3 = make_float3(mesh->vertexNormals[(i)].x(), mesh->vertexNormals[(i)].y(),mesh->vertexNormals[(i)].z());
    optix::float4 v4 = make_float4( v3, 1.0f );
    nbuffer_data[i] = make_float3( transform*v4 );
  }

  // Copy texture coordinates.
  // memcpy( static_cast<void*>( tbuffer_data ),
  //         static_cast<void*>( &(model->texcoords[2]) ),
  //         sizeof( float )*num_texcoords*2 );   

  // Calculate bbox of model
  // for( unsigned int i = 0; i < num_vertices; ++i )
    // m_aabb.include( vbuffer_data[i] );

  // Unmap buffers.
  m_vbuffer->unmap();
  m_nbuffer->unmap();
  m_tbuffer->unmap();

  //
  // create instance
  // 

    // Load triangle_mesh programs
  // if( !m_intersect_program.get() ) {
    std::string path = std::string("/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/") + std::string("/cuda_compile_ptx_generated_triangle_mesh.cu.ptx");
    optix::Program m_intersect_program = context->createProgramFromPTXFile( path, "mesh_intersect" );
  // }

  // if( !m_bbox_program.get() ) {
    path = std::string("/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/") + std::string("/cuda_compile_ptx_generated_triangle_mesh.cu.ptx");
    optix::Program m_bbox_program = context->createProgramFromPTXFile( path, "mesh_bounds" );
  // }

  std::vector<GeometryInstance> instances;

  // Loop over all groups -- grab the triangles and material props from each group
  unsigned int triangle_count = 0u;
  unsigned int group_count = 0u;
  // for ( GLMgroup* obj_group = model->groups;
        // obj_group != 0;
        // obj_group = obj_group->next, group_count++ ) {

  unsigned int num_triangles = numTriangles;
    // unsigned int num_triangles = obj_group->numtriangles;
    // if ( num_triangles == 0 ) continue; 

    // Create vertex index buffers
    optix::Buffer vindex_buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT3, num_triangles );
    optix::int3* vindex_buffer_data = static_cast<optix::int3*>( vindex_buffer->map() );

    optix::Buffer tindex_buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT3, num_triangles );
    optix::int3* tindex_buffer_data = static_cast<optix::int3*>( tindex_buffer->map() );

    optix::Buffer nindex_buffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_INT3, num_triangles );
    optix::int3* nindex_buffer_data = static_cast<optix::int3*>( nindex_buffer->map() );

    Buffer mbuffer = context->createBuffer( RT_BUFFER_INPUT, RT_FORMAT_UNSIGNED_INT, num_triangles );
    unsigned int* mbuffer_data = static_cast<unsigned int*>( mbuffer->map() );

    // optix::Geometry oMesh;

    for ( unsigned int i = 0; i < numTriangles; ++i ) {

      // unsigned int tindex = mesh->indices[i];
      int3 vindices;
      // vindices.x = mesh->indices[ tindex ].vindices[0] - 1; 
      // vindices.y = model->triangles[ tindex ].vindices[1] - 1; 
      // vindices.z = model->triangles[ tindex ].vindices[2] - 1; 
      vindices.x = mesh->vertex_indices[i*3+0];
      vindices.y = mesh->vertex_indices[i*3+1];
      vindices.z = mesh->vertex_indices[i*3+2];
      assert( vindices.x <= static_cast<int>(numPositions) );
      assert( vindices.y <= static_cast<int>(numPositions) );
      assert( vindices.z <= static_cast<int>(numPositions) );
      
      vindex_buffer_data[ i ] = vindices;

      optix::int3 nindices;
      nindices.x = mesh->vertex_indices[i*3+0];
      nindices.y = mesh->vertex_indices[i*3+1];
      nindices.z = mesh->vertex_indices[i*3+2];

      int3 tindices;
      tindices.x = -1;//model->triangles[ tindex ].tindices[0] - 1; 
      tindices.y = -1;//model->triangles[ tindex ].tindices[1] - 1; 
      tindices.z = -1;//model->triangles[ tindex ].tindices[2] - 1; 

      nindex_buffer_data[ i ] = nindices;
      tindex_buffer_data[ i ] = tindices;
      mbuffer_data[ i ] = 0; // See above TODO

    }
    vindex_buffer->unmap();
    tindex_buffer->unmap();
    nindex_buffer->unmap();
    mbuffer->unmap();

    std::vector<int> tri_reindex;

    // if (m_large_geom) {
    //   if( m_ASBuilder == std::string("Sbvh") || m_ASBuilder == std::string("KdTree")) {
    //     m_ASBuilder = "MedianBvh";
    //     m_ASTraverser = "Bvh";
    //   }

    //   float* vertex_buffer_data = static_cast<float*>( m_vbuffer->map() );
    //   RTsize nverts;
    //   m_vbuffer->getSize(nverts);

    //   tri_reindex.resize(num_triangles);
    //   RTgeometry geometry;

    //   unsigned int usePTX32InHost64 = 0;
    //   rtuCreateClusteredMeshExt( context->get(), usePTX32InHost64, &geometry, 
    //                             (unsigned int)nverts, vertex_buffer_data, 
    //                              num_triangles, (const unsigned int*)vindex_buffer_data,
    //                              mbuffer_data,
    //                              m_nbuffer->get(), 
    //                             (const unsigned int*)nindex_buffer_data,
    //                             m_tbuffer->get(), 
    //                             (const unsigned int*)tindex_buffer_data);
    //   mesh = optix::Geometry::take(geometry);

    //   m_vbuffer->unmap();
    //   rtBufferDestroy( vindex_buffer->get() );
    // } else 
    // {
      // Create the mesh object
      optix::Geometry oMesh = context->createGeometry();
      oMesh->setPrimitiveCount( num_triangles );
      oMesh->setIntersectionProgram( m_intersect_program);
      oMesh->setBoundingBoxProgram( m_bbox_program );
      oMesh[ "vertex_buffer" ]->setBuffer( m_vbuffer );
      oMesh[ "vindex_buffer" ]->setBuffer( vindex_buffer );
      oMesh[ "normal_buffer" ]->setBuffer( m_nbuffer );
      oMesh[ "texcoord_buffer" ]->setBuffer( m_tbuffer );
      oMesh[ "tindex_buffer" ]->setBuffer( tindex_buffer );
      oMesh[ "nindex_buffer" ]->setBuffer( nindex_buffer );
      oMesh[ "material_buffer" ]->setBuffer( mbuffer );
    // }

    // Create the geom instance to hold mesh and material params
    // loadMaterialParams( instance, obj_group->material );
    // instances.push_back( instance );
  // }



//create material
        // std::string path = std::string(sutilSamplesPtxDir()) + "/cuda_compile_ptx_generated_obj_material.cu.ptx";
#if !USE_AO
  optix::Program closest_hit = context->createProgramFromPTXFile( "/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/cuda_compile_ptx_generated_obj_material.cu.ptx", "closest_hit_radiance" );
  optix::Program any_hit     = context->createProgramFromPTXFile( "/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/cuda_compile_ptx_generated_obj_material.cu.ptx", "any_hit_shadow" );
#else 
  float m_ao_sample_mult = 0.4;
  context["sqrt_occlusion_samples"]->setInt( 3 * m_ao_sample_mult );
  context["sqrt_diffuse_samples"]->setInt( 3 );
          optix::Program closest_hit = context->createProgramFromPTXFile( "/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/sample6_generated_ambocc.cu.ptx", "closest_hit_radiance" );
          optix::Program any_hit = context->createProgramFromPTXFile( "/work/01336/carson/opt/optix/SDK-precompiled-samples/ptx/sample6_generated_ambocc.cu.ptx", "any_hit_occlusion" );
          #endif


    // optix::Program chp = context->createProgramFromPTXFile( ptxPath( "normal_shader.cu" ), "closest_hit_radiance" );
        // const std::string ptx_path = ptxpath("sample6", "ambocc.cu");

  optix::Material material           = context->createMaterial();
  material->setClosestHitProgram( 0u, closest_hit );
  material->setAnyHitProgram( 1u, any_hit );
  // material->setClosestHitProgram( 0, chp );




  // optix::Material material = context->createMaterial();
    GeometryInstance instance = context->createGeometryInstance( oMesh, &material, &material+1 );
    instance[ "emissive" ]->setFloat( 0.0f, 0.0f, 0.0f );
    instance[ "phong_exp" ]->setFloat( 32.0f );
    instance[ "reflectivity" ]->setFloat( 0.3f, 0.3f, 0.3f );
    instance[ "illum" ]->setInt( 2 );

    instance["ambient_map"]->setTextureSampler( loadTexture( context, "", make_float3( 0.2f, 0.2f, 0.2f ) ) );
    instance["diffuse_map"]->setTextureSampler( loadTexture( context, "", make_float3( 0.8f, 0.8f, 0.8f ) ) );
    instance["specular_map"]->setTextureSampler( loadTexture( context, "", make_float3( 0.0f, 0.0f, 0.0f ) ) );



      // instance->setMaterialCount( 1 );
    // instance->setGeometry(mesh);
    // instance->setMaterial(material);


      optix:GeometryGroup geometrygroup = context->createGeometryGroup();
      geometrygroup->setChildCount( 1 );
  optix::Acceleration acceleration = context->createAcceleration("Sbvh", "Bvh");

      acceleration->setProperty( "vertex_buffer_name", "vertex_buffer" );
      acceleration->setProperty( "index_buffer_name", "vindex_buffer" );
  // acceleration->setProperty( "refine", m_ASRefine );

  geometrygroup->setAcceleration( acceleration );
  acceleration->markDirty();
  geometrygroup->setChild(0, instance);

      oren->gi = new optix::GeometryInstance;
    *(oren->gi) = instance;
    oren->gg = geometrygroup;

    // Create geometry instance
  // optix::GeometryInstance gi = context->createGeometryInstance();
  // gi->setMaterialCount( 1 );
  // gi->setGeometry( sphere );
  // gi->setMaterial( 0, material );
  oren->setBuilt(true);

}


void OptiXManager::deleteRenderable(Renderable* ren)
{
  //TODO: DELETE RENDERABLES
  ORenderable* r = dynamic_cast<ORenderable*>(ren);
  // printf("deleting renderable of size: %d\n", er->_data->mesh->vertex_indices.size()/3);
  /*if (er->isBuilt())*/
  /*g_device->rtClear(er->_data->d_mesh);*/
  r->setBuilt(false);
  /*er->_data->mesh->vertexNormals.resize(0);*/
  // delete er->_data->mesh;  //embree handles clearing the data... not sure how to get it to not do that with rtclear yet
}

void OptiXManager::addTexture(int handle, int target, int level, int internalFormat, int width, int height, int border, int format, int type, void* data)
{
}

void OptiXManager::deleteTexture(int handle)
{

}

GeometryGenerator* OptiXManager::getGeometryGenerator(int type)
{
  switch(type)
  {
    case GL_TRIANGLES:
    {
      return _gTriangle;
    }
    case GL_TRIANGLE_STRIP:
    {
      return _gTriangleStrip;
    }
    case GL_QUADS:
    {
      return _gQuads;
    }
    case GL_QUAD_STRIP:
    {
      return _gQuadStrip;
    }
    //case GL_LINES:
    //{
        ////			gen = rm->GLines;
        ////break;
    //}
    //case GL_LINE_STRIP:
    //{
        ////			gen = rm->GLineStrip;
        ////break;
    //}
    //case GL_POLYGON:
    //{
        ////this is temporary for visit, need to support other than quads
        ////break;
    //}
    default:
    {
      return _gVoid;
    }
  }
}

RenderManager* createOptiXManager(){ return OptiXManager::singleton(); }