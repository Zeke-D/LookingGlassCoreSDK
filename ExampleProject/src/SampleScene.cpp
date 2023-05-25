/**
 * SampleScene.cpp
 * Contributors:
 *      * Arthur Sonzogni (author), Looking Glass Factory Inc.
 * Licence:
 *      * MIT
 */

#ifdef WIN32
#pragma warning(disable : 4464 4820 4514 5045 4201 5039 4061 4710 4458 4626 5027)
#endif

#include "SampleScene.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_operation.hpp>
#include <iostream>
#include <vector>

#include "glError.hpp"

#ifdef _DEBUG
static const bool capture_mouse = false;
#else
static const bool capture_mouse = true;
#endif

struct VertexType
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec4 color;
};

SampleScene::SampleScene() : HoloPlayContext(capture_mouse)
{
  glCheckError(__FILE__, __LINE__);

  // creation of the mesh ------------------------------------------------------
  std::vector<VertexType> vertices;
  std::vector<GLuint> indices;

  VertexType v;
  v.normal = glm::vec3(0., 0., 1.);
  v.color  = glm::vec4(0., 0., 1., 0.);

  for (float x = -1.; x <= 1.; x += 2) {
    for (float y = -1.; y <= 1.; y += 2) {
      v.position = glm::vec3(x,  y, -1.);
      vertices.push_back(v);
    }
  }

  // two triangles
  indices.push_back(0);
  indices.push_back(1);
  indices.push_back(2);
    
  indices.push_back(1);
  indices.push_back(2);
  indices.push_back(3);
  
  std::cout << "vertices=" << vertices.size() << std::endl;
  std::cout << "index=" << indices.size() << std::endl;

  // creation of the vertex array buffer----------------------------------------

  // vbo
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(vertices.size() * sizeof(VertexType)),
               vertices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // ibo
  glGenBuffers(1, &ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(indices.size() * sizeof(GLuint)),
               indices.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  // vao
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  // bind vbo
  glBindBuffer(GL_ARRAY_BUFFER, vbo);

  const char *fragmentShaderSource = R"--(
    #version 150

    in vec4 fPosition;
    in vec4 fColor;
    in vec4 fLightPosition;
    in vec3 fNormal;

    // output
    out vec4 color;

    void main(void)
    {       
        vec3 o =-normalize(fPosition.xyz);
        vec3 n = normalize(fNormal);
        vec3 r = reflect(o,n);
        vec3 l = normalize(fLightPosition.xyz-fPosition.xyz);

        float ambient = 0.1;
        float diffus = 0.7*max(0.0,dot(n,l));
        float specular = 0.6*pow(max(0.0,-dot(r,l)),4.0);

        color = vec4(ambient + fColor.xyz * diffus + specular, fColor.w);
    }
  )--";
  const char *vertexShaderSource = R"--(
    #version 150

    in vec3 position;
    in vec3 normal;
    in vec4 color;

    uniform mat4 projection;
    uniform mat4 view;

    out vec4 fPosition;
    out vec4 fColor;
    out vec4 fLightPosition;
    out vec3 fNormal;

    void main(void)
    {
        fPosition = view * vec4(position,1.0);
        fLightPosition = view * vec4(0.0,0.0,1.0,1.0);

        fColor = color;
        fNormal = vec3(view * vec4(normal,0.0));

        // gl_Position = projection * fPosition;
    }
  )--";
  Shader vertexShader(GL_VERTEX_SHADER, vertexShaderSource);
  Shader fragmentShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

  shaderProgram = new ShaderProgram({vertexShader, fragmentShader});

  // map vbo to shader attributes
  shaderProgram->setAttribute("position", 3, sizeof(VertexType),
                              offsetof(VertexType, position));
  shaderProgram->setAttribute("normal", 3, sizeof(VertexType),
                              offsetof(VertexType, normal));
  shaderProgram->setAttribute("color", 4, sizeof(VertexType),
                              offsetof(VertexType, color));

  // bind the ibo
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

  // vao end
  glBindVertexArray(0);
}

// process input: query GLFW if relevant keys are pressed/released 
// if ESC pressed, return false
// ---------------------------------------------------------------------------------------------------------
bool SampleScene::processInput(GLFWwindow *window)
{
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    return false;

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

  // Here add your code to control the camera by keys
  float cameraSpeed = 5 * deltaTime;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    cameraPos += cameraSpeed * cameraFront;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    cameraPos -= cameraSpeed * cameraFront;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    cameraPos +=
        glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    cameraPos -=
        glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;

  return true;
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void SampleScene::mouse_callback(GLFWwindow*, double xpos, double ypos)
{
  if (firstMouse)
  {
    lastX = float(xpos);
    lastY = float(ypos);
    firstMouse = false;
  }

  float xoffset = float(xpos) - lastX;
  float yoffset =
      lastY - float(ypos); // reversed since y-coordinates go from bottom to top

  lastX = float(xpos);
  lastY = float(ypos);

  float sensitivity = 0.01f; // change this value to your liking
  xoffset *= -sensitivity;
  yoffset *= -sensitivity;

  yaw += xoffset;
  pitch += yoffset;

  // make sure that when pitch is out of bounds, screen doesn't get flipped
  if (pitch > 89.0f)
    pitch = 89.0f;
  if (pitch < -89.0f)
    pitch = -89.0f;

  glm::vec3 front;
  front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
  front.y = sin(glm::radians(pitch));
  front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
  cameraFront = glm::normalize(front);
}

// mouse scroll feed back
void SampleScene::scroll_callback(GLFWwindow*,
                                  double,
                                  double yoffset)
{
  // here we implement zoom in and zoom out control
  const float MAX_SIZE = 10; // for example
  const float MIN_SIZE = 1;  // for example
  if (cameraSize >= MIN_SIZE && cameraSize <= MAX_SIZE)
    cameraSize -= float(yoffset);
  if (cameraSize <= MIN_SIZE)
    cameraSize = MIN_SIZE;
  if (cameraSize >= MAX_SIZE)
    cameraSize = MAX_SIZE;
}

void SampleScene::update()
{
  // add your updates for each frame here
}

void SampleScene::onExit()
{
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ibo);
  delete shaderProgram;
}

glm::mat4 SampleScene::getViewMatrixOfCurrentFrame()
{
  return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
}

void SampleScene::renderScene()
{
  glCheckError(__FILE__, __LINE__);

  // clear
  glClear(GL_COLOR_BUFFER_BIT);
  glClearColor(0.0, 0.0, 0.0, 1.0);

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glCheckError(__FILE__, __LINE__);

  shaderProgram->use();
  glCheckError(__FILE__, __LINE__);

  // holoplay special camera setup for each view, don't delete
  shaderProgram->setUniform("view", GetViewMatrixOfCurrentView());
  shaderProgram->setUniform("projection", GetProjectionMatrixOfCurrentView());
  glCheckError(__FILE__, __LINE__);

  // render your scene here as usual
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

  glCheckError(__FILE__, __LINE__);
  glDrawElements(GL_TRIANGLES,        // mode
                 GLsizei(6), // count
                 GL_UNSIGNED_INT,     // type
                 NULL                 // element array buffer offset
  );

  glBindVertexArray(0);

  shaderProgram->unuse();
}
