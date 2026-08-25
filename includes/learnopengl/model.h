#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <learnopengl/mesh.h>
#include <learnopengl/shader.h>
#include <learnopengl/ktx2_image_provider.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

unsigned int TextureFromFile(const char *path, const string &directory, bool gamma = false);

class Model
{
public:
    // model data
    vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;

    // constructor, expects a filepath to a 3D model.
    Model(string const &path, bool gamma = false) : gammaCorrection(gamma)
    {
        loadModel(path);
    }

    // draws the model, and thus all its meshes
    void Draw(Shader &shader)
    {
        for(unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

private:
    // loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string const &path)
    {
        // read file via ASSIMP
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        // check for errors
        if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // retrieve the directory path of the filepath
        const size_t lastSlash = path.find_last_of("/\\");
        directory = (lastSlash != string::npos) ? path.substr(0, lastSlash) : "";
        for (char& c : directory)
        {
            if (c == '\\')
                c = '/';
        }
        std::cout << "Model loaded from: " << path << std::endl;
        std::cout << "Model texture directory: " << directory << std::endl;

        // process ASSIMP's root node recursively
        processNode(scene->mRootNode, scene);
    }

    // processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene)
    {
        // process each mesh located at the current node
        for(unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            // the node object only contains indices to index the actual objects in the scene.
            // the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // after we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for(unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }

    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        // data to fill
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<Texture> textures;

        // walk through each of the mesh's vertices
        for(unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            // positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            // normals
            if (mesh->HasNormals())
            {
                vector.x = mesh->mNormals[i].x;
                vector.y = mesh->mNormals[i].y;
                vector.z = mesh->mNormals[i].z;
                vertex.Normal = vector;
            }
            // texture coordinates
            if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // a vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
                // tangent
                vector.x = mesh->mTangents[i].x;
                vector.y = mesh->mTangents[i].y;
                vector.z = mesh->mTangents[i].z;
                vertex.Tangent = vector;
                // bitangent
                vector.x = mesh->mBitangents[i].x;
                vector.y = mesh->mBitangents[i].y;
                vector.z = mesh->mBitangents[i].z;
                vertex.Bitangent = vector;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);

            vertices.push_back(vertex);
        }
        // now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for(unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            // retrieve all indices of the face and store them in the indices vector
            for(unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
        // process materials
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        // we assume a convention for sampler names in the shaders. Each diffuse texture should be named
        // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
        // Same applies to other texture as the following list summarizes:
        // diffuse: texture_diffuseN
        // specular: texture_specularN
        // normal: texture_normalN

        // 1. diffuse maps
        vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        // 2. specular maps
        vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        // 3. normal maps
        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
        textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
        // 4. height maps
        std::vector<Texture> heightMaps = loadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
        textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

        // return a mesh object created from the extracted mesh data
        return Mesh(vertices, indices, textures);
    }

    // checks all material textures of a given type and loads the textures if they're not loaded yet.
    // the required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName)
    {
        vector<Texture> textures;
        for(unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            // check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            bool skip = false;
            for(unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if(std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
                    break;
                }
            }
            if(!skip)
            {   // if texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
            }
        }
        return textures;
    }
};


unsigned int TextureFromFile(const char *path, const string &directory, bool gamma)
{
    auto normalizePath = [](string p) {
        for (char& c : p)
        {
            if (c == '\\')
                c = '/';
        }
        return p;
    };
    auto baseName = [](const string& p) {
        const size_t pos = p.find_last_of('/');
        return (pos == string::npos) ? p : p.substr(pos + 1);
    };
    auto fileReadable = [](const string& p) {
        std::ifstream f(p.c_str(), std::ios::binary);
        return f.good();
    };

    string raw = normalizePath(path ? path : "");
    // Assimp/FBX often embeds Windows absolute paths (C:/... or //server/...)
    if (raw.size() >= 2 && ((raw[0] >= 'A' && raw[0] <= 'Z') || (raw[0] >= 'a' && raw[0] <= 'z')) && raw[1] == ':')
        raw = baseName(raw);
    while (raw.size() >= 2 && raw[0] == '/' && raw[1] == '/')
        raw = raw.substr(1);

    string dir = normalizePath(directory);
    vector<string> candidates;
    auto addCandidate = [&](const string& p) {
        if (p.empty())
            return;
        for (const auto& existing : candidates)
        {
            if (existing == p)
                return;
        }
        candidates.push_back(p);
    };

    const bool rawIsAbs = !raw.empty() && raw[0] == '/';
    if (!dir.empty() && !rawIsAbs)
        addCandidate(dir + "/" + raw);
    if (!dir.empty())
        addCandidate(dir + "/" + baseName(raw));
    addCandidate(raw);
    addCandidate(baseName(raw));

    // If Assimp ABI still truncates the name (e.g. LOW_WEPON -> WEPON), find a
    // file in the model directory whose name ends with the (possibly truncated) name.
    // Also try common alternate extensions (.ktx2 / .png / .jpg).
    auto addSuffixMatchesInDir = [&](const string& needle) {
        if (dir.empty() || needle.empty() || !fs::is_directory(dir))
            return;
        std::error_code ec;
        for (const auto& ent : fs::directory_iterator(dir, ec))
        {
            if (ec || !ent.is_regular_file())
                continue;
            const string name = ent.path().filename().string();
            if (name.size() >= needle.size() &&
                name.compare(name.size() - needle.size(), needle.size(), needle) == 0)
            {
                addCandidate(ent.path().string());
            }
        }
    };

    const string needle = baseName(raw);
    addSuffixMatchesInDir(needle);
    // Prefer compressed sibling if mtl points at png but only ktx2 exists (and vice versa)
    const char* altExts[] = { ".ktx2", ".png", ".jpg", ".jpeg", ".tga", ".ktx" };
    const size_t dot = needle.find_last_of('.');
    if (dot != string::npos)
    {
        const string stem = needle.substr(0, dot);
        for (const char* ext : altExts)
            addSuffixMatchesInDir(stem + ext);
    }

    string filename;
    for (const auto& c : candidates)
    {
        if (fileReadable(c))
        {
            filename = c;
            break;
        }
    }
    if (filename.empty())
        filename = candidates.empty() ? raw : candidates.front();

    unsigned int textureID = 0;
    glGenTextures(1, &textureID);

    auto uploadRaster = [&](unsigned char* data, int width, int height, int nrComponents) -> bool {
        if (!data)
            return false;

        GLenum format = GL_RGB;
        GLenum internalFormat = GL_RGB;
        if (nrComponents == 1)
        {
            format = GL_RED;
            internalFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            format = GL_RGB;
            internalFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            format = GL_RGBA;
            internalFormat = GL_RGBA;
        }
        else
        {
            std::cout << "Texture unsupported channel count " << nrComponents << " at " << filename << std::endl;
            return false;
        }

#if defined(SS_GL_USE_ES) && SS_GL_USE_ES
        if (nrComponents == 1)
            internalFormat = GL_R8;
        else if (nrComponents == 3)
            internalFormat = GL_RGB8;
        else if (nrComponents == 4)
            internalFormat = GL_RGBA8;
#endif

        while (glGetError() != GL_NO_ERROR) {}

        glBindTexture(GL_TEXTURE_2D, textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internalFormat), width, height, 0, format, GL_UNSIGNED_BYTE, data);
        const GLenum err = glGetError();
        if (err != GL_NO_ERROR)
        {
            std::cout << "glTexImage2D failed for " << filename
                      << " err=0x" << std::hex << err << std::dec
                      << " size=" << width << "x" << height
                      << " channels=" << nrComponents << std::endl;
            return false;
        }

        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return true;
    };

    if (isKtx2File(filename))
    {
        Ktx2Texture ktx;
        if (loadKtx2FromFile(filename, ktx))
        {
            if (uploadKtx2ToTexture(textureID, ktx))
            {
                std::cout << "Texture loaded KTX2: " << filename << std::endl;
                return textureID;
            }
            std::cout << "uploadKtx2ToTexture fail: " << filename << std::endl;
        }
        else
        {
            std::cout << "loadKtx2FromFile fail: " << filename << std::endl;
        }

        // Fallback: same basename with common raster extensions
        const string stem = filename.substr(0, filename.size() - 5); // strip .ktx2
        const char* exts[] = { ".png", ".jpg", ".jpeg", ".tga", ".bmp" };
        for (const char* ext : exts)
        {
            const string alt = stem + ext;
            if (!fileReadable(alt))
                continue;
            int width = 0, height = 0, nrComponents = 0;
            unsigned char* data = stbi_load(alt.c_str(), &width, &height, &nrComponents, 0);
            if (data && uploadRaster(data, width, height, nrComponents))
            {
                stbi_image_free(data);
                std::cout << "Texture loaded fallback raster: " << alt << std::endl;
                return textureID;
            }
            if (data)
                stbi_image_free(data);
        }

        std::cout << "Texture failed to load KTX2: " << filename
                  << " (modelDir=" << dir << ", assimpPath=" << (path ? path : "") << ")" << std::endl;
        return textureID;
    }

    int width = 0, height = 0, nrComponents = 0;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data && uploadRaster(data, width, height, nrComponents))
    {
        stbi_image_free(data);
        std::cout << "Texture loaded: " << filename << std::endl;
        return textureID;
    }

    std::cout << "Texture failed to load: " << filename
              << " reason: " << (stbi_failure_reason() ? stbi_failure_reason() : "unknown")
              << " (modelDir=" << dir << ", assimpPath=" << (path ? path : "") << ")"
              << " tried:";
    for (const auto& c : candidates)
        std::cout << " [" << c << "]";
    std::cout << std::endl;

    if (data)
        stbi_image_free(data);
    return textureID;
}
#endif
