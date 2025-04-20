#include "storage.h"

#include "data/hash_map.h"
#include "tbx/hash/hash.h"

#define RYML_SINGLE_HDR_DEFINE_NOW
#include "libs/rapidyaml.hpp"

using namespace cellar;

void Storage::save() const
{
  path vaultPath = path("vault/vault.yml");

  ryml::Tree tree;
  ryml::NodeRef node = tree.rootref();

  node |= ryml::MAP;

  for (const auto& file : _files)
  {
    std::string hash = std::string(file.first.literal());
    auto child = node.append_child();
    child << ryml::key(hash);
    child << file.second.path().c_str();
  }

  std::string output = ryml::emitrs_yaml<std::string>(tree);

  FILE* out = fopen(vaultPath.c_str(), "wb+");
  fwrite(output.c_str(), output.size(), 1, out);
  fclose(out);
}

bool Storage::isOwned(const DatRom* rom) const
{
  // TODO: check if sha1 is actually used for the rom
  auto it = _files.find(rom->hash->sha1());
  return it != _files.end();
}