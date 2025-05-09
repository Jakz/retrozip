#include "cataloguer.h"

using namespace cellar;

void Cataloguer::catalogue(Game* game)
{
  std::string_view name = game->name;
  for (size_t k = 0; k < name.size(); ++k)
  {
    char closing = '\0';
    /* search for opening tag */
    if (name[k] == '(')
      closing = ')';
    else if (name[k] == '[')
      closing = ']';

    if (closing != '\0')
    {
      auto it = name.find(closing, k);

      if (it == std::string_view::npos)
        break;
      else
      {
        std::string tag = std::string(name.substr(k + 1, it - k - 1));
        //tags.insert(tag);

        game->tags.add(kernel()->tags()->registerTag(tag));

        k = it;
      }
    }
  }
}