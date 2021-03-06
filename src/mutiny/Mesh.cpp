#include "Mesh.h"
#include "Application.h"
#include "Color.h"
#include "Debug.h"
#include "Exception.h"

#include "internal/WavefrontParser.h"

#include <iostream>
#include <memory>
#include <functional>

namespace mutiny
{

namespace engine
{

ref<Mesh> Mesh::load(std::string path)
{
  internal::WavefrontParser parser(path + ".obj");
  ref<internal::ModelData> modelData = parser.getModelData();
  std::vector<Vector3> vertices;
  std::vector<Vector3> normals;
  std::vector<Vector2> uv;
  std::vector<Color> colors;
  std::vector<std::vector<int> > triangles;
  int currentSubmesh = 0;

  for(size_t p = 0; p < modelData->parts.size(); p++)
  {
    ref<internal::PartData> part = modelData->parts.at(p);

    for(size_t m = 0; m < part->materialGroups.size(); m++)
    {
      ref<internal::MaterialGroupData> materialGroup = part->materialGroups.at(m);
      triangles.push_back(std::vector<int>());

      for(size_t f = 0; f < materialGroup->faces.size(); f++)
      {
        ref<internal::FaceData> face = materialGroup->faces.at(f);

        triangles.at(currentSubmesh).push_back(vertices.size());
        vertices.push_back(Vector3(face->a.position.x, face->a.position.y, face->a.position.z));
        triangles.at(currentSubmesh).push_back(vertices.size());
        vertices.push_back(Vector3(face->b.position.x, face->b.position.y, face->b.position.z));
        triangles.at(currentSubmesh).push_back(vertices.size());
        vertices.push_back(Vector3(face->c.position.x, face->c.position.y, face->c.position.z));

        normals.push_back(Vector3(face->a.normal.x, face->a.normal.y, face->a.normal.z));
        normals.push_back(Vector3(face->b.normal.x, face->b.normal.y, face->b.normal.z));
        normals.push_back(Vector3(face->c.normal.x, face->c.normal.y, face->c.normal.z));

        uv.push_back(Vector2(face->a.coord.x, face->a.coord.y));
        uv.push_back(Vector2(face->b.coord.x, face->b.coord.y));
        uv.push_back(Vector2(face->c.coord.x, face->c.coord.y));

        colors.push_back(Color(materialGroup->material->color.x,
                               materialGroup->material->color.y,
                               materialGroup->material->color.z));

        colors.push_back(Color(materialGroup->material->color.x,
                               materialGroup->material->color.y,
                               materialGroup->material->color.z));

        colors.push_back(Color(materialGroup->material->color.x,
                               materialGroup->material->color.y,
                               materialGroup->material->color.z));
      }

      currentSubmesh++;
    }
  }


  ref<Mesh> mesh = new Mesh();
  mesh->setVertices(vertices);
  mesh->setNormals(normals);
  mesh->setUv(uv);
  mesh->setColors(colors);

  for(size_t i = 0; i < triangles.size(); i++)
  {
    mesh->setTriangles(triangles.at(i), i);
  }

  Debug::log("Loading mesh");

  return mesh;
}

void Mesh::setVertices(std::vector<Vector3> vertices)
{
  this->vertices = vertices;
}

void Mesh::setColors(std::vector<Color> colors)
{
  this->colors = colors;
}

void Mesh::setTriangles(std::vector<int> triangles, int submesh)
{
  bool insert = false;

  if(submesh > positionBufferIds.size())
  {
    throw Exception("Submesh index out of bounds");
  }
  else if(submesh == positionBufferIds.size())
  {
    this->triangles.push_back(triangles);
    insert = true;
  }
  else
  {
    this->triangles.at(submesh) = triangles;
  }

  recalculateBounds();

  std::vector<float> values;

  for(size_t i = 0; i < triangles.size(); i++)
  {
    values.push_back(vertices.at(triangles.at(i)).x);
    values.push_back(vertices.at(triangles.at(i)).y);
    values.push_back(vertices.at(triangles.at(i)).z);
  }

  shared<gl::Uint> positionBufferId;

  if(insert == true)
  {
    positionBufferId = gl::Uint::genBuffer();
    positionBufferIds.push_back(positionBufferId);
  }
  else
  {
    positionBufferId = positionBufferIds.at(submesh);
  }

  glBindBuffer(GL_ARRAY_BUFFER, positionBufferId->getGLuint());
  //glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(values[0]), &values[0], GL_STATIC_DRAW);
  glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(values[0]), &values[0], GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

// Normals

  if(normals.size() > 0)
  {
    values.clear();

    for(size_t i = 0; i < triangles.size(); i++)
    {
      values.push_back(normals.at(triangles.at(i)).x);
      values.push_back(normals.at(triangles.at(i)).y);
      values.push_back(normals.at(triangles.at(i)).z);
    }

    shared<gl::Uint> normalBufferId;

    if(insert == true)
    {
      normalBufferId = gl::Uint::genBuffer();
      normalBufferIds.push_back(normalBufferId);
    }
    else
    {
      normalBufferId = normalBufferIds.at(submesh);
    }

    glBindBuffer(GL_ARRAY_BUFFER, normalBufferId->getGLuint());
    //glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(values[0]), &values[0], GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(values[0]), &values[0], GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

// UVs

  if(uv.size() > 0)
  {
    values.clear();

    for(size_t i = 0; i < triangles.size(); i++)
    {
      values.push_back(uv.at(triangles.at(i)).x);
      values.push_back(uv.at(triangles.at(i)).y);
    }

    shared<gl::Uint> uvBufferId;

    if(insert == true)
    {
      uvBufferId = gl::Uint::genBuffer();
      uvBufferIds.push_back(uvBufferId);
    }
    else
    {
      uvBufferId = uvBufferIds.at(submesh);
    }

    glBindBuffer(GL_ARRAY_BUFFER, uvBufferId->getGLuint());
    //glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(values[0]), &values[0], GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, values.size() * sizeof(values[0]), &values[0], GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
  }
}

void Mesh::setUv(std::vector<Vector2> uv)
{
  this->uv = uv;
}

void Mesh::setNormals(std::vector<Vector3> normals)
{
  this->normals = normals;
}

std::vector<Vector3>& Mesh::getVertices()
{
  return vertices;
}

std::vector<int>& Mesh::getTriangles(int submesh)
{
  return triangles.at(submesh);
}

std::vector<Vector2>& Mesh::getUv()
{
  return uv;
}

std::vector<Vector3>& Mesh::getNormals()
{
  return normals;
}

std::vector<Color>& Mesh::getColors()
{
  return colors;
}

void Mesh::recalculateBounds()
{
  if(vertices.size() < 1)
  {
    bounds = Bounds(Vector3(), Vector3());
  }

  float minX = vertices.at(0).x; float maxX = vertices.at(0).x;
  float minY = vertices.at(0).y; float maxY = vertices.at(0).y;
  float minZ = vertices.at(0).z; float maxZ = vertices.at(0).z;

  for(size_t i = 0; i < vertices.size(); i++)
  {
    if(vertices.at(i).x < minX)
    {
      minX = vertices.at(i).x;
    }

    if(vertices.at(i).x > maxX)
    {
      maxX = vertices.at(i).x;
    }

    if(vertices.at(i).y < minY)
    {
      minY = vertices.at(i).y;
    }

    if(vertices.at(i).y > maxY)
    {
      maxY = vertices.at(i).y;
    }

    if(vertices.at(i).z < minZ)
    {
      minZ = vertices.at(i).z;
    }

    if(vertices.at(i).z > maxZ)
    {
      maxZ = vertices.at(i).z;
    }
  }

  float midX = (maxX + minX) / 2.0f;
  float midY = (maxY + minY) / 2.0f;
  float midZ = (maxZ + minZ) / 2.0f;

  float sizeX = maxX - minX;
  float sizeY = maxY - minY;
  float sizeZ = maxZ - minZ;

  bounds = Bounds(Vector3(midX, midY, midZ), Vector3(sizeX, sizeY, sizeZ));
}

Bounds Mesh::getBounds()
{
  return bounds;
}

int Mesh::getSubmeshCount()
{
  return triangles.size();
}

}

}
