#include "Material.h"
#include "Application.h"
#include "Shader.h"
#include "Matrix4x4.h"
#include "Texture.h"
#include "Texture2d.h"
#include "Resources.h"
#include "Debug.h"
#include "Exception.h"

#include <memory>
#include <functional>

#include <iostream>
#include <fstream>

namespace mutiny
{

namespace engine
{

ref<Material> Material::load(std::string path)
{
  std::string vertPath = path + ".vert";
  std::string fragPath = path + ".frag";

  std::string vertContents;
  std::string fragContents;

  std::ifstream file;
  std::string line;
  file.open(vertPath.c_str());

  if(file.is_open() == false)
  {
    throw Exception("Failed to read vertex shader file '" + path + "'");
  }

  while(file.eof() == false)
  {
    getline(file, line);
    vertContents += line + '\n';
  }

  file.close();
  file.open(fragPath.c_str());

  if(file.is_open() == false)
  {
    Exception("Failed to read fragment shader file");
  }

  while(file.eof() == false)
  {
    getline(file, line);
    fragContents += line + '\n';
  }

  ref<Material> material = new Material(vertContents, fragContents);

  return material;
}

Material::Material(std::string vertContents, std::string fragContents)
{
  managedShader = Shader::create(vertContents, fragContents);
  indexesDirty = true;
  refreshIds();
}

Material::Material()
{
}

shared<Material> Material::create(std::string vertContents, std::string fragContents)
{
  shared<Material> rtn(new Material());
  rtn->managedShader = Shader::create(vertContents, fragContents);
  rtn->indexesDirty = true;
  rtn->refreshIds();

  return rtn;
}

shared<Material> Material::create(ref<Shader> shader)
{
  shared<Material> rtn(new Material());
  rtn->shader = shader;
  rtn->indexesDirty = true;
  rtn->refreshIds();

  return rtn;
}

Material::Material(ref<Material> material)
{
  shader = material->getShader();
  indexesDirty = true;
  refreshIds();
}

Material::Material(ref<Shader> shader)
{
  this->shader = shader;
  indexesDirty = true;
  refreshIds();
}

ref<Texture> Material::getTexture(std::string propertyName)
{
  for(size_t i = 0; i < textureNames.size(); i++)
  {
    if(textureNames.at(i) == propertyName)
    {
      return textures.at(i).get();
    }
  }

  return NULL;
}

void Material::setTexture(std::string propertyName, ref<Texture2d> texture)
{
  ref<Texture> tex = texture.get();
  setTexture(propertyName, tex);
}

void Material::setTexture(std::string propertyName, ref<Texture> texture)
{
  for(size_t i = 0; i < textureNames.size(); i++)
  {
    if(textureNames.at(i) == propertyName)
    {
      textures.at(i) = texture;
      return;
    }
  }

  textures.push_back(texture);
  textureIndexes.push_back(-1);
  textureNames.push_back(propertyName);
  indexesDirty = true;
}

void Material::setVector(std::string propertyName, Vector2 value)
{
  for(size_t i = 0; i < vector2Names.size(); i++)
  {
    if(vector2Names.at(i) == propertyName)
    {
      vector2s[i] = value;
      return;
    }
  }

  vector2s.push_back(value);
  vector2Indexes.push_back(-1);
  vector2Names.push_back(propertyName);
  indexesDirty = true;
}

void Material::setFloat(std::string propertyName, float value)
{
  for(size_t i = 0; i < floatNames.size(); i++)
  {
    if(floatNames.at(i) == propertyName)
    {
      floats[i] = value;
      return;
    }
  }

  floats.push_back(value);
  floatIndexes.push_back(-1);
  floatNames.push_back(propertyName);
  indexesDirty = true;
}

void Material::setMatrix(std::string propertyName, Matrix4x4 matrix)
{
  for(size_t i = 0; i < matrixNames.size(); i++)
  {
    if(matrixNames.at(i) == propertyName)
    {
      matrices[i] = matrix;
      return;
    }
  }

  matrices.push_back(matrix);
  matrixIndexes.push_back(-1);
  matrixNames.push_back(propertyName);
  indexesDirty = true;
}

Matrix4x4 Material::getMatrix(std::string propertyName)
{
  for(size_t i = 0; i < matrixNames.size(); i++)
  {
    if(matrixNames.at(i) == propertyName)
    {
      return matrices[i];
    }
  }

  //Debug::logWarning("Matrix with specified name '"+propertyName+"' does not exist in shader");
  return Matrix4x4::getIdentity();
}

void Material::refreshIndexes()
{
  for(size_t i = 0; i < matrixNames.size(); i++)
  {
    GLuint uniformId = glGetUniformLocation(getShader()->programId->getGLuint(), matrixNames.at(i).c_str());

    if(uniformId == -1)
    {
      //Debug::logWarning("The specified matrix name '" + matrixNames.at(i) + "' was not found in the shader");
      continue;
    }

    matrixIndexes[i] = uniformId;
  }

  for(size_t i = 0; i < vector2Names.size(); i++)
  {
    GLuint uniformId = glGetUniformLocation(getShader()->programId->getGLuint(), vector2Names.at(i).c_str());

    if(uniformId == -1)
    {
      //Debug::logWarning("The specified vector2 name was not found in the shader");
      continue;
    }

    vector2Indexes[i] = uniformId;
  }

  for(size_t i = 0; i < floatNames.size(); i++)
  {
    GLuint uniformId = glGetUniformLocation(getShader()->programId->getGLuint(), floatNames.at(i).c_str());

    if(uniformId == -1)
    {
      //Debug::logWarning("The specified float name was not found in the shader");
      continue;
    }

    floatIndexes[i] = uniformId;
  }

  for(size_t i = 0; i < textureNames.size(); i++)
  {
    GLint uniformId = glGetUniformLocation(getShader()->programId->getGLuint(), textureNames.at(i).c_str());

    if(uniformId == -1)
    {
      //Debug::logWarning("The specified sampler name was not found in the shader");
      continue;
    }

    textureIndexes[i] = uniformId;
  }

  refreshIds();
}

ref<Texture> Material::getMainTexture()
{
  return getTexture("in_Texture");
}

void Material::setMainTexture(ref<Texture> texture)
{
  setTexture("in_Texture", texture);
}

void Material::setMainTexture(ref<Texture2d> texture)
{
  setTexture("in_Texture", texture);
}

ref<Shader> Material::getShader()
{
  ref<Shader> rtn = shader;

  if(rtn.expired())
  {
    rtn = managedShader;
  }

  return rtn;
}

void Material::setShader(ref<Shader> shader)
{
  this->shader = shader;
  indexesDirty = true;
  refreshIds();
}

void Material::refreshIds()
{
  positionId = glGetAttribLocation(getShader()->programId->getGLuint(), "in_Position");
  uvId = glGetAttribLocation(getShader()->programId->getGLuint(), "in_Uv");
  normalId = glGetAttribLocation(getShader()->programId->getGLuint(), "in_Normal");
  modelUniformId = glGetUniformLocation(getShader()->programId->getGLuint(), "in_Model");
}

int Material::getPassCount()
{
  return 1;
}

void Material::setPass(int pass, ref<Material> _this)
{
  Application::context->currentMaterial = _this;
  glUseProgram(getShader()->programId->getGLuint());

  if(indexesDirty == true)
  {
    refreshIndexes();
  }

  for(size_t i = 0; i < matrixNames.size(); i++)
  {
    glUniformMatrix4fv(matrixIndexes[i], 1, GL_FALSE, matrices[i].getValue());
  }

  for(size_t i = 0; i < vector2Names.size(); i++)
  {
    glUniform2f(vector2Indexes[i], vector2s[i].x, vector2s[i].y);
  }

  for(size_t i = 0; i < floatNames.size(); i++)
  {
    glUniform1f(floatIndexes[i], floats[i]);
  }

  for(size_t i = 0; i < textureNames.size(); i++)
  {
    glUniform1i(textureIndexes[i], i);
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, textures.at(i)->getNativeTexture());
  }
}

}

}

