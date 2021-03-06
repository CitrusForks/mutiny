#include "SceneManager.h"

#include <mutiny/mutiny.h>

#include <ctime>

using namespace mutiny::engine;

void mutiny_main()
{
  srand(time(NULL));
  Application::setTitle("Baastud - The quadrupedal, copulation game");

  ref<GameObject> smGo = GameObject::create();
  smGo->addComponent<SceneManager>();

  Application::loadLevel("mutiny");
}
