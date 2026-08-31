#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: fbx2obj <input.fbx> <output.obj>\n";
        return 1;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        argv[1],
        aiProcess_Triangulate
            | aiProcess_GenSmoothNormals
            | aiProcess_FlipUVs
            | aiProcess_CalcTangentSpace);

    if (!scene)
    {
        std::cerr << "Import failed: " << importer.GetErrorString() << "\n";
        return 2;
    }

    Assimp::Exporter exporter;
    if (exporter.Export(scene, "obj", argv[2]) != AI_SUCCESS)
    {
        std::cerr << "Export failed: " << exporter.GetErrorString() << "\n";
        return 3;
    }

    return 0;
}
