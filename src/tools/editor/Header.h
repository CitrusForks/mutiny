#ifndef HEADER_H
#define HEADER_H

#include "EditorWindow.h"

class ProjectScreen;

class Header : public EditorWindow
{
public:
  Header(ProjectScreen* parent);

  void onGui();

};

#endif

