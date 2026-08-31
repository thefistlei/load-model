#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/version.h>
#include <iostream>
int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "usage: dump_mats <model>\n"; return 1; }
  Assimp::Importer imp;
  const aiScene* sc = imp.ReadFile(argv[1], aiProcess_Triangulate | aiProcess_FlipUVs);
  if (!sc) { std::cout << "FAIL: " << imp.GetErrorString() << "\n"; return 1; }
  std::cout << "OK meshes=" << sc->mNumMeshes << " mats=" << sc->mNumMaterials
            << " assimp=" << aiGetVersionMajor() << "." << aiGetVersionMinor() << "\n";
  for (unsigned i=0;i<sc->mNumMeshes;i++) {
    aiMesh* m = sc->mMeshes[i];
    std::cout << "mesh["<<i<<"] verts="<<m->mNumVertices<<" faces="<<m->mNumFaces
              << " uv0="<<(m->mTextureCoords[0]?"yes":"no")<<" mat="<<m->mMaterialIndex<<"\n";
  }
  for (unsigned i=0;i<sc->mNumMaterials;i++) {
    aiMaterial* m = sc->mMaterials[i];
    aiString name; m->Get(AI_MATKEY_NAME, name);
    std::cout << "mat["<<i<<"] name=" << name.C_Str() << "\n";
    for (int t=1;t<=(int)AI_TEXTURE_TYPE_MAX;t++) {
      unsigned c = m->GetTextureCount((aiTextureType)t);
      if (!c) continue;
      for (unsigned j=0;j<c;j++) {
        aiString p; m->GetTexture((aiTextureType)t,j,&p);
        std::cout << "  type="<<t<<" path=["<<p.C_Str()<<"]\n";
      }
    }
  }
  return 0;
}
