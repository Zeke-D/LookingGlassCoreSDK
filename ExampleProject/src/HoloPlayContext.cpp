/**
 * HoloPlayContext.cpp
 * Contributors:
 *      * Arthur Sonzogni (author), Looking Glass Factory Inc.
 * Licence:
 *      * MIT
 */

#include <initializer_list>
#ifdef WIN32
#pragma warning(disable : 4464 4820 4514 5045 4201 5039 4061 4710)
#endif

#include "HoloPlayContext.hpp"
#include "HoloPlayShaders.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_operation.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "Shader.hpp"
#include "glError.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

#define UNUSED(x) [&x]{}()

HoloPlayContext *currentApplication = NULL;

HoloPlayContext &HoloPlayContext::getInstance()
{
  if (currentApplication)
    return *currentApplication;
  else
    throw std::runtime_error("There is no current Application");
}

// Mouse and scroll function wrapper
// ========================================================
// callback functions must be static
HoloPlayContext &getInstance()
{
  if (currentApplication)
    return *currentApplication;
  else
    throw std::runtime_error("There is no current Application");
}

static void external_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  getInstance().key_callback(window, key, scancode, action, mods);
}


void HoloPlayContext::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) exit();
  if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
    cout << "Recomputing hit buffers" << endl;
    glCheckError(__FILE__, __LINE__);
    const char* shaderPaths[2] = { "../sdf_shader.glsl", "../color.glsl" };
    ShaderProgram** shaders[2] = { &sdfShader, &colorShader };
    for (int i = 0; i < 2; i++) {
      const char* shaderPath = shaderPaths[i];
      Shader vertShader(GL_VERTEX_SHADER, (opengl_version_header + hpc_LightfieldVertShaderGLSL).c_str());
      Shader fragShader(shaderPath, GL_FRAGMENT_SHADER);
      if (!fragShader.checkCompileError(shaderPath)) {
        // if we did not get an error, re-assign the shader
        delete *shaders[i];
        *shaders[i] = new ShaderProgram({vertShader, fragShader});
      }
    }
    renderHitBuffers();
    cout << "Done hit buffers" << endl;
    glCheckError(__FILE__, __LINE__);

    cout << "We made it..." << endl;
    
    // cout << "Entering recompile mode" << std::endl;
    // autoRecompile = !autoRecompile;
  }
  if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
    renderSwitch = (renderSwitch + 1) % 3;
  }
}

// wrapper for getting mouse movement callback
static void external_mouse_callback(GLFWwindow *window,
                                    double xpos,
                                    double ypos)
{
  // here we access the instance via the singleton pattern and forward the
  // callback to the instance method
  getInstance().mouse_callback(window, xpos, ypos);
}

// wrapper for getting mouse scroll callback
static void external_scroll_callback(GLFWwindow *window,
                                     double xpos,
                                     double ypos)
{
  // here we access the instance via the singleton pattern and forward the
  // callback to the instance method
  getInstance().scroll_callback(window, xpos, ypos);
}

HoloPlayContext::HoloPlayContext(bool capture_mouse)
    : state(State::Ready),
      title("Application"),
      opengl_version_major(3),
      opengl_version_minor(3)
{
  currentApplication = this;

  timeSinceCompiled = 0;

  // get device info via holoplay core
  if (!GetLookingGlassInfo())
  {
    cout << "[Info] HoloplayCore Message Pipe tear down" << endl;
    state = State::Exit;
    // must tear down the message pipe before shut down the app
    hpc_TeardownMessagePipe();
    throw std::runtime_error("Couldn't find looking glass");
  }
  // get the viewcone here, which is used as a const
  viewCone = hpc_GetDevicePropertyFloat(DEV_INDEX, "/calibration/viewCone/value");

  cout << "[Info] GLFW initialisation" << endl;

  opengl_version_header = "#version ";
  opengl_version_header += to_string(opengl_version_major);
  opengl_version_header += to_string(opengl_version_minor);
  opengl_version_header += "0 core\n";

  // initialize the GLFW library
  if (!glfwInit())
  {
    throw std::runtime_error("Couldn't init GLFW");
  }
  
  // set opengl version (will also be used for lightfield and blit shaders)

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, opengl_version_major);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, opengl_version_minor);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // create window on the first looking glass device
  window = openWindowOnLKG();
  if (!window)
  {
    glfwTerminate();
    throw std::runtime_error(
        "Couldn't create a window on looking glass device");
  }
  cout << "[Info] Window opened on lkg" << endl;
  glfwMakeContextCurrent(window);
  glCheckError(__FILE__, __LINE__);

  // set up the cursor callback
  glfwSetCursorPosCallback(window, external_mouse_callback);
  glfwSetScrollCallback(window, external_scroll_callback);
  glfwSetKeyCallback(window, external_key_callback);

  if (capture_mouse)
  {
      // tell GLFW to capture our mouse
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }

  glCheckError(__FILE__, __LINE__);

  glewExperimental = GL_TRUE;
  GLenum err = glewInit();
  glCheckError(__FILE__, __LINE__);

  if (err != GLEW_OK)
  {
    cout << "terminiated" << endl;
    glfwTerminate();
    throw std::runtime_error(string("Could initialize GLEW, error = ") +
                             (const char *)glewGetErrorString(err));
  }

  // get OpenGL version info
  const GLubyte *renderer = glGetString(GL_RENDERER);
  const GLubyte *version = glGetString(GL_VERSION);
  cout << "Renderer: " << renderer << endl;
  cout << "[Info] OpenGL version supported " << version << endl;

  // opengl configuration
  glEnable(GL_DEPTH_TEST); // enable depth-testing
  glDepthFunc(GL_LESS);    // depth-testing interprets a smaller value as "closer"

  // initialize the holoplay context
  initialize();
  renderHitBuffers();
}

HoloPlayContext::~HoloPlayContext()
{
}

void HoloPlayContext::onExit()
{
  cout << "[INFO] : on exit" << endl;
}

void HoloPlayContext::exit()
{
  state = State::Exit;
  cout << "[Info] Informing Holoplay Core to close app" << endl;
  hpc_CloseApp();
  // release all the objects created for setting up the HoloPlay Context
  release();
}

// main loop
void HoloPlayContext::run()
{
  state = State::Run;

  // Make the window's context current
  glfwMakeContextCurrent(window);

  time = float(glfwGetTime());

  while (state == State::Run)
  {
    // compute new time and delta time
    float t = float(glfwGetTime());
    deltaTime = t - time;
    time = t;

    // detech window related changes
    detectWindowChange();
    glCheckError(__FILE__, __LINE__);

    // press esc to quit
    if (!processInput(window))
    {
      exit();
      onExit();
      continue;
    }

    // decide how camera updates here, override in SampleScene.cpp
    // glm::mat4 currentViewMatrix = getViewMatrixOfCurrentFrame();
    glCheckError(__FILE__, __LINE__);

    // do the update
    update();

    // clear backbuffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.0, 0.0, 0.0, 1.0);

    // bind quilt texture to frame buffer
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    // save the viewport for the total quilt
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    glViewport(0, 0, qs_width, qs_height);

    renderScene();
    
    glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);

    // reset framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // draw the light field image
    drawLightField();

    // Swap Front and Back buffers (double buffering)
    glfwSwapBuffers(window);

    // Poll and process events
    glfwPollEvents();


  //   // recompile sdf shader if it needs to be (do it every second)
  //   if (autoRecompile && timeSinceCompiled > 1) {
  //     for (auto &s : sdfShader->getShaders()) {
  //       glDetachShader(sdfShader->getHandle(), s.getHandle());
  //       glDeleteShader(s.getHandle());
  //     }
  //     Shader vertShader(GL_VERTEX_SHADER, (opengl_version_header + hpc_LightfieldVertShaderGLSL).c_str());
  //     Shader fragShader("../sdf_shader.glsl", GL_FRAGMENT_SHADER);
  //     if (!fragShader.checkCompileError("../sdf_shader.glsl")) {
  //       // if we did not get an error, re-assign the shader
  //       delete sdfShader;
  //       sdfShader = new ShaderProgram({vertShader, fragShader});
  //       renderHitBuffers();
  //     }
  //     timeSinceCompiled = 0;
  //   }
   
  //   timeSinceCompiled += deltaTime;
  }

  glfwTerminate();
}

// window coordinates may be changed when the main display is scaled and the
// looking glass display is not, so we make this function here to detect the
// window change and force our window to be full-screen again
void HoloPlayContext::detectWindowChange()
{
  int w, h;
  int x, y;
  glfwGetWindowSize(getWindow(), &w, &h);
  glfwGetWindowPos(getWindow(), &x, &y);

  windowChanged = (w != win_w) || (h != win_h) || (x != win_x) || (y != win_y);
  if (windowChanged)
  {
    cout << "[Info] Dimension changed: (" << w << "," << h << ")" << endl;
    glfwSetWindowPos(getWindow(), win_x, win_y);
    glfwSetWindowSize(getWindow(), win_w, win_h);
    cout << "[Info] force window to be full-screen again" << endl;
  }
}

// virtual functions
// =========================================================
void HoloPlayContext::update()
{
  // cout << "[INFO] : update" << endl;
  // implement update function in the child class
}


void HoloPlayContext::renderHitBuffers() {
  // GLint curFBO;
  // glGetIntegerv(GL_FRAMEBUFFER_BINDING, &curFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, hitFBO);
  // glClearColor(0.0, 0.0, 0.0, 0.0);
  // glClear(GL_COLOR_BUFFER_BIT);
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  glViewport(0, 0, qs_width, qs_height);
  sdfShader->use();
  sdfShader->setUniform("qs_columns", qs_columns);
  sdfShader->setUniform("qs_rows", qs_rows);
  sdfShader->setUniform("qs_width", qs_width);
  sdfShader->setUniform("qs_height", qs_height);
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  sdfShader->unuse();
  glCheckError(__FILE__, __LINE__);
  glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  // glBindFramebuffer(GL_FRAMEBUFFER, curFBO);
}

void HoloPlayContext::renderScene()
{

  glCheckError(__FILE__, __LINE__);
  glBindVertexArray(VAO);
  glBindFramebuffer(GL_FRAMEBUFFER, FBO);
  for (GLuint i = 0; i < 2; i++) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, hitAttachments[i]);
  }
  colorShader->use();
  colorShader->setUniform("iTime", time);
  // uniform layout locations not supported in 3.3, set manually
  colorShader->setUniform("posMatTex", 0);
  colorShader->setUniform("normalTex", 1);
  colorShader->setUniform("renderSwitch", renderSwitch);
  glActiveTexture(GL_TEXTURE0 + 2);
  glCheckError(__FILE__, __LINE__);
  glBindTexture(GL_TEXTURE_2D, texture);
  glCheckError(__FILE__, __LINE__);
  colorShader->setUniform("customTex", 2);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glCheckError(__FILE__, __LINE__);
  colorShader->unuse();
  
  /*
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glBlitFramebuffer(0, 0, qs_width, qs_height, 
    0, 0, qs_width / qs_columns *3., qs_height / qs_rows * 3., 
    GL_COLOR_BUFFER_BIT, GL_LINEAR);
  */
}

glm::mat4 HoloPlayContext::getViewMatrixOfCurrentFrame()
{
  // cout << "[INFO] : update camera" << endl;
  return glm::mat4(1.0);
}

// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly. return false to close application
// ---------------------------------------------------------------------------------------------------------
bool HoloPlayContext::processInput(GLFWwindow*)
{
  // cout << "[INFO] : process input" << endl;
  // exit on escape
  // if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) return false;
  
  int new_debug = 0;

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    new_debug = 1;

  if (debug != new_debug)
  {
    debug = new_debug;
    lightFieldShader->use();
    lightFieldShader->setUniform("debug", debug);
    lightFieldShader->unuse();
  }
  return true;
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void HoloPlayContext::mouse_callback(GLFWwindow*,
                                     double xpos,
                                     double ypos) 
{
    UNUSED(xpos);
    UNUSED(ypos);
}

void HoloPlayContext::scroll_callback(GLFWwindow*,
                                      double xoffset,
                                      double yoffset) 
{
    UNUSED(xoffset);
    UNUSED(yoffset);
}

// get functions
// =========================================================
int HoloPlayContext::getWidth()
{
  return win_w;
}

int HoloPlayContext::getHeight()
{
  return win_h;
}

float HoloPlayContext::getWindowRatio()
{
  return float(win_w) / float(win_h);
}

GLFWwindow *HoloPlayContext::getWindow() const
{
  return window;
}

bool HoloPlayContext::windowDimensionChanged()
{
  return windowChanged;
}

float HoloPlayContext::getFrameDeltaTime() const
{
  return deltaTime;
}

float HoloPlayContext::getTime() const
{
  return time;
}

// Sample code for creating HoloPlay context

// Holoplay Core related functions
// =========================================================
// Register and initialize the app through HoloPlay Core
// And print information about connected Looking Glass devices
bool HoloPlayContext::GetLookingGlassInfo()
{
  hpc_client_error errco =
      hpc_InitializeApp("Holoplay Core Example App", hpc_LICENSE_NONCOMMERCIAL);
  if (errco)
  {
    string errstr;
    switch (errco)
    {
    case hpc_CLIERR_NOSERVICE:
      errstr = "HoloPlay Service not running";
      break;
    case hpc_CLIERR_SERIALIZEERR:
      errstr = "Client message could not be serialized";
      break;
    case hpc_CLIERR_VERSIONERR:
      errstr = "Incompatible version of HoloPlay Service";
      break;
    case hpc_CLIERR_PIPEERROR:
      errstr = "Interprocess pipe broken";
      break;
    case hpc_CLIERR_SENDTIMEOUT:
      errstr = "Interprocess pipe send timeout";
      break;
    case hpc_CLIERR_RECVTIMEOUT:
      errstr = "Interprocess pipe receive timeout";
      break;
    default:
      errstr = "Unknown error";
      break;
    }
    cout << "HoloPlay Service access error (code " << errco << "): " << errstr
         << "!" << endl;
    return false;
  }
  char buf[1000];
  hpc_GetHoloPlayCoreVersion(buf, 1000);
  cout << "HoloPlay Core version " << buf << "." << endl;
  hpc_GetHoloPlayServiceVersion(buf, 1000);
  cout << "HoloPlay Service version " << buf << "." << endl;
  int num_displays = hpc_GetNumDevices();
  cout << num_displays << " devices connected." << endl;
  if (num_displays < 1)
  {
    return false;
  }
  for (int i = 0; i < num_displays; ++i)
  {
    cout << "Device information for display " << i << ":" << endl;
    hpc_GetDeviceHDMIName(i, buf, 1000);
    cout << "\tDevice name: " << buf << endl;
    hpc_GetDeviceType(i, buf, 1000);
    cout << "\tDevice type: " << buf << endl;
    hpc_GetDeviceType(i, buf, 1000);
    cout << "\nWindow parameters for display " << i << ":" << endl;
    cout << "\tPosition: (" << hpc_GetDevicePropertyWinX(i) << ", "
         << hpc_GetDevicePropertyWinY(i) << ")" << endl;
    cout << "\tSize: (" << hpc_GetDevicePropertyScreenW(i) << ", "
         << hpc_GetDevicePropertyScreenH(i) << ")" << endl;
    cout << "\tAspect ratio: " << hpc_GetDevicePropertyDisplayAspect(i) << endl;
    cout << "\nShader uniforms for display " << i << ":" << endl;
    cout << "\tPitch: " << hpc_GetDevicePropertyPitch(i) << endl;
    cout << "\tTilt: " << hpc_GetDevicePropertyTilt(i) << endl;
    cout << "\tCenter: " << hpc_GetDevicePropertyCenter(i) << endl;
    cout << "\tSubpixel width: " << hpc_GetDevicePropertySubp(i) << endl;
    cout << "\tView cone: " << hpc_GetDevicePropertyFloat(i, "/calibration/viewCone/value") << endl;
    cout << "\tFringe: " << hpc_GetDevicePropertyFringe(i) << endl;
    cout << "\tRI: " << hpc_GetDevicePropertyRi(i)
         << "\n\tBI: " << hpc_GetDevicePropertyBi(i)
         << "\n\tinvView: " << hpc_GetDevicePropertyInvView(i) << endl;
  }

  return true;
}

GLuint loadTextureByPath(const char* filePath) {
  int width, height, nrChannels;
  unsigned char* imageData = stbi_load(filePath, &width, &height, &nrChannels, 0);
  if (!imageData) {
    cout << "Failed to load rocky image." << endl;
    exit(1);
  }
  
  GLuint texID;
  glGenTextures(1, &texID);
  glBindTexture(GL_TEXTURE_2D, texID);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, imageData);
  stbi_image_free(imageData);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);
  return texID;
}


// setup fuctions
// =========================================================
void HoloPlayContext::initialize()
{
  autoRecompile = false;
  renderSwitch = 0;

  cout << "[Info] initializing" << endl;
  glfwMakeContextCurrent(window);
  
  setupQuiltSettings(1);

  loadLightFieldShaders();
  glCheckError(__FILE__, __LINE__);

  // load my custom shader
  cout << "loading quilt shader" << endl;
  Shader vertShader(GL_VERTEX_SHADER, (opengl_version_header + hpc_LightfieldVertShaderGLSL).c_str());
  Shader fragShader("../sdf_shader.glsl", GL_FRAGMENT_SHADER);
  sdfShader = new ShaderProgram({vertShader, fragShader});
  glCheckError(__FILE__, __LINE__);
  
  Shader colShader("../color.glsl", GL_FRAGMENT_SHADER);
  colorShader = new ShaderProgram( { vertShader, colShader } );
  glCheckError(__FILE__, __LINE__);


  // WORKING
  // inspired by https://ogldev.org/www/tutorial35/tutorial35.html
  // setup custom precomputation shader to precompute sdf hits
  glGenFramebuffers(1, &hitFBO);
  glBindFramebuffer(GL_FRAMEBUFFER, hitFBO);
  glGenTextures(2, hitAttachments);
  
  for (GLuint i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, hitAttachments[i]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCheckError(__FILE__, __LINE__);
    cout << GL_RGBA32F << " " << qs_width << " " << qs_height << endl;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, qs_width, qs_height, 0, GL_RGBA, GL_FLOAT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, 
      hitAttachments[i], 0);
  }
  
  // enable writing to the buffers
  GLenum writeableAttachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
  glDrawBuffers(2, writeableAttachments );

  // make sure it went well
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
      cout << "Finished setting up framebuffers!" << endl;
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      cout << "Incomplete attachment." << endl;
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      cout << "Missing attachment." << endl;
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
      cout << "Incomplete drawbuffer." << endl;
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
      cout << "Incomplete read buffer." << endl;
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      cout << "Format not support." << endl;
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      cout << "Incomplete multisample." << endl;
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
      cout << "Incomplete layer targets." << endl;
      break;
    default:
      cout << "Unknown error setting up hit framebuffers." << endl;
      break;
  }
  
  // unbind FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);


  texture = loadTextureByPath("../images/rocky-small.jpg");

  /*
  TODO: get cubemap working
  // load cubemap
  vector<std::string> faces = {
    "images/skybox/right.jpg",
    "images/skybox/left.jpg",
    "images/skybox/top.jpg",
    "images/skybox/bottom.jpg",
    "images/skybox/front.jpg",
    "images/skybox/back.jpg"
  };
  
  skyMap = loadCubemap(faces);
  */
  
  loadCalibrationIntoShader();
  glCheckError(__FILE__, __LINE__);

  passQuiltSettingsToShader();
  glCheckError(__FILE__, __LINE__);

  setupQuilt();
  glCheckError(__FILE__, __LINE__);
}

GLuint HoloPlayContext::loadCubemap(vector<std::string> faces) {
  
  GLuint cubemapID;
  glGenTextures(1, &cubemapID);
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapID);

  int width, height, nrChannels;
  for (GLuint i = 0; i < faces.size(); i++) {
    unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
    if (data) {
      glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    else {
      std::cout << "Failed to load cubemap with path \"" << faces[i] << "\"" << std::endl;
    }
    stbi_image_free(data);
  }

  return cubemapID;
}

// set up the quilt settings
void HoloPlayContext::setupQuiltSettings(int preset)
{
  // there are 3 presets:
  switch (preset)
  {
  case 0: // standard
    qs_width = 2048;
    qs_height = 2048;
    qs_columns = 4;
    qs_rows = 8;
    qs_totalViews = 32;
    break;
  default:
  case 1: // hires
    qs_width = 4096;
    qs_height = 4096;
    qs_columns = 8;
    qs_rows = 6;
    qs_totalViews = 48;
    break;
  case 2: // 8k
    qs_width = 4096 * 2;
    qs_height = 4096 * 2;
    qs_columns = 5;
    qs_rows = 9;
    qs_totalViews = 45;
    break;
  }
}
// pass quilt values to shader
void HoloPlayContext::passQuiltSettingsToShader()
{
  lightFieldShader->use();
  lightFieldShader->setUniform("overscan", 0);
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("tile",
                               glm::vec3(qs_columns, qs_rows, qs_totalViews));
  glCheckError(__FILE__, __LINE__);

  int qs_viewWidth = qs_width / qs_columns;
  int qs_viewHeight = qs_height / qs_rows;

  lightFieldShader->setUniform(
      "viewPortion", glm::vec2(float(qs_viewWidth * qs_columns) / float(qs_width),
                               float(qs_viewHeight * qs_rows) / float(qs_height)));
  glCheckError(__FILE__, __LINE__);
  lightFieldShader->unuse();
}

void HoloPlayContext::setupQuilt()
{
  cout << "setting up quilt texture and framebuffer" << endl;
  glGenTextures(1, &quiltTexture);
  glBindTexture(GL_TEXTURE_2D, quiltTexture);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, qs_width, qs_height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_2D, 0);

  // framebuffer
  glGenFramebuffers(1, &FBO);
  glBindFramebuffer(GL_FRAMEBUFFER, FBO);

  // bind the quilt texture as the color attachment of the framebuffer
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, quiltTexture, 0);

  // vbo and vao
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  // set up the vertex array object
  glBindVertexArray(VAO);

  // fullscreen quad vertices
  const float fsquadVerts[] = {
      -1.0f,
      -1.0f,
      -1.0f,
      1.0f,
      1.0f,
      1.0f,
      1.0f,
      1.0f,
      1.0f,
      -1.0f,
      -1.0f,
      -1.0f,
  };

  // create vbo
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(fsquadVerts), fsquadVerts,
               GL_STATIC_DRAW);

  // setup the attribute pointers
  // note: using only 2 floats per vert, not 3
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // unbind stuff
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void HoloPlayContext::loadLightFieldShaders()
{
  cout << "loading quilt shader" << endl;
  Shader lightFieldVertexShader(
      GL_VERTEX_SHADER,
      (opengl_version_header + hpc_LightfieldVertShaderGLSL).c_str());
  Shader lightFieldFragmentShader(
      GL_FRAGMENT_SHADER,
      (opengl_version_header + hpc_LightfieldFragShaderGLSL).c_str());
  lightFieldShader =
      new ShaderProgram({lightFieldVertexShader, lightFieldFragmentShader});
}

void HoloPlayContext::loadCalibrationIntoShader()
{
  cout << "begin assigning calibration uniforms" << endl;
  lightFieldShader->use();
  lightFieldShader->setUniform("pitch", hpc_GetDevicePropertyPitch(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("tilt", hpc_GetDevicePropertyTilt(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("center",
                               hpc_GetDevicePropertyCenter(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("invView",
                               hpc_GetDevicePropertyInvView(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("quiltInvert", 0);
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("subp", hpc_GetDevicePropertySubp(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("ri", hpc_GetDevicePropertyRi(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("bi", hpc_GetDevicePropertyBi(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);

  lightFieldShader->setUniform("displayAspect",
                               hpc_GetDevicePropertyDisplayAspect(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);
  lightFieldShader->setUniform("quiltAspect",
                               hpc_GetDevicePropertyDisplayAspect(DEV_INDEX));
  glCheckError(__FILE__, __LINE__);
  lightFieldShader->unuse();
  glCheckError(__FILE__, __LINE__);
}

// release function
// =========================================================
void HoloPlayContext::release()
{
  cout << "[Info] HoloPlay Context releasing" << endl;
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteFramebuffers(1, &FBO);
  glDeleteTextures(1, &quiltTexture);
  delete lightFieldShader;
  delete blitShader;

  glDeleteFramebuffers(1, &hitFBO);
  glDeleteTextures(2, hitAttachments);
  delete sdfShader;
}

void HoloPlayContext::drawLightField()
{
  // bind quilt texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, quiltTexture);

  // bind vao
  glBindVertexArray(VAO);

  // use the shader and draw
  lightFieldShader->use();
  glDrawArrays(GL_TRIANGLES, 0, 6);

  // clean up
  glBindVertexArray(0);
  lightFieldShader->unuse();
}

// Other helper functions
// =======================================================================
// open window at looking glass monitor
GLFWwindow* HoloPlayContext::openWindowOnLKG()
{
  // Load GLFW and Create a Window
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
  glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

  // open the window
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_CENTER_CURSOR, false);
  glfwWindowHint(GLFW_DECORATED, false);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);

  // get the window size / coordinates
  win_w = hpc_GetDevicePropertyScreenW(DEV_INDEX);
  win_h = hpc_GetDevicePropertyScreenH(DEV_INDEX);
  win_x = hpc_GetDevicePropertyWinX(DEV_INDEX);
  win_y = hpc_GetDevicePropertyWinY(DEV_INDEX);
  cout << "[Info] window opened at (" << win_x << ", " << win_y << "), size: ("
       << win_w << ", " << win_h << ")" << endl;
  // open the window
  auto mWindow =
      glfwCreateWindow(win_w, win_h, "Looking Glass Output", NULL, NULL);

  glfwSetWindowPos(mWindow, win_x, win_y);

  return mWindow;
}
