#include "model.h"
#include "graphic.h"
#include "texture.h"
#include "sglthing.h"
#include <string.h>
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct model models[256] = {0};
int models_loaded = 0;

static void model_load_textures(struct mesh* mesh, struct aiMesh* mesh_2, const struct aiScene* scene)
{
    if(mesh_2->mMaterialIndex >= 0)
    {
        struct aiMaterial* material = scene->mMaterials[mesh_2->mMaterialIndex];
        struct aiString path;
        for(unsigned int i = 0; i < MIN(3,aiGetMaterialTextureCount(material, aiTextureType_DIFFUSE)); i++)
        {
            aiGetMaterialTexture(material, aiTextureType_DIFFUSE, i, &path, NULL, NULL, NULL, NULL, NULL, NULL);
            if(!get_texture(&path.data))
            {
                if(path.data[0] == '*')
                    printf("sglthing: embedded textures not supported\n");
                else
                {
                    char fpath[64];
                    snprintf(fpath,64,"../resources/%s",&path.data);
                    load_texture(fpath);
                    mesh->diffuse_texture[mesh->diffuse_textures] = get_texture(fpath);
                    mesh->diffuse_textures++;
                }
            }
            else
            {
                mesh->diffuse_texture[mesh->diffuse_textures] = get_texture(path.data);
                mesh->diffuse_textures++;
            }
        }
        for(unsigned int i = 0; i < MIN(3,aiGetMaterialTextureCount(material, aiTextureType_SPECULAR)); i++)
        {
            aiGetMaterialTexture(material, aiTextureType_SPECULAR, i, &path, NULL, NULL, NULL, NULL, NULL, NULL);
            if(!get_texture(&path.data))
            {
                if(path.data[0] == '*')
                    printf("sglthing: embedded textures not supported\n");
                else
                {
                    char fpath[64];
                    snprintf(fpath,64,"../%s",&path.data);
                    load_texture(fpath);
                    mesh->specular_texture[mesh->specular_textures] = get_texture(fpath);
                    mesh->specular_textures++;
                }
            }
            else
            {
                mesh->specular_texture[mesh->specular_textures] = get_texture(path.data);
                mesh->specular_textures++;
            }    
        }
    }
}

static void model_reset_vertex_bone_data(struct model_vertex* vtx_array, int vertex)
{
    for(int i = 0; i < 4; i++)
    {
        if(vtx_array[vertex].bone_ids[i] < 0)
        {
            vtx_array[vertex].bone_ids[i] = -1;
            vtx_array[vertex].bone_ids[i] = 0.f;
        }
    }
}

struct model_bone_info* model_find_bone_data(struct mesh* mesh, char* name, int* id_out)
{
    for(int i = 0; i < mesh->bone_infos; i++)
    {
        if(strncmp(mesh->bone_info[i].name, name, 64) == 0)
        {
            if(id_out)
                *id_out = i;
            return &mesh->bone_info[i];
        }
    }
    return 0;
}

static void model_set_vertex_bone_data(struct model_vertex* vtx_array, int vertex, int bone_id, float weight)
{
    for(int i = 0; i < 4; i++)
    {
        if(vtx_array[vertex].bone_ids[i] < 0)
        {
            vtx_array[vertex].bone_ids[i] = bone_id;
            vtx_array[vertex].weights[i] = weight;
            break;
        }
    }
}

static void assimp_convert_mat4(struct aiMatrix4x4 from, mat4 to)
{
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
}

static void model_extract_bone_weights(struct mesh* i_mesh, struct model_vertex* vtx_array, struct aiMesh* mesh, const struct aiScene* scene)
{
    if(mesh->mNumBones == 0)
        printf("sglthing: mesh has no bones\n");
    for(int bone_index = 0; bone_index < mesh->mNumBones; ++bone_index)
    {
        int bone_id = -1;
        int bone_data_id = 0;
        struct model_bone_info* bone_data;
        char* bone_name = mesh->mBones[bone_index]->mName.data;
        bone_data = model_find_bone_data(i_mesh, bone_name, &bone_data_id);
        if(bone_data_id == i_mesh->bone_infos)
        {
            struct model_bone_info new_bone_info;
            new_bone_info.id = i_mesh->bone_counter;
            assimp_convert_mat4(mesh->mBones[bone_index]->mOffsetMatrix,new_bone_info.offset);
            i_mesh->bone_info[i_mesh->bone_infos++] = new_bone_info;
            bone_id = i_mesh->bone_counter;
            i_mesh->bone_counter++;
        }
        else
        {
            bone_id = bone_data->id;
        }

        ASSERT(bone_id != -1);

        struct aiVertexWeight* weights = mesh->mBones[bone_index]->mWeights;
        int num_weights = mesh->mBones[bone_index]->mNumWeights;

        for(int weight_index = 0; weight_index < num_weights; ++weight_index)
        {
            int vertex_id = weights[weight_index].mVertexId;
            float weight =  weights[weight_index].mWeight;
            model_set_vertex_bone_data(i_mesh->vtx_data, vertex_id, bone_id, weight);
        }
    }
}

static void model_parse_mesh(struct model_vertex* vtx_array, int* vtx_count, unsigned int* idx_array, int* idx_count, struct aiMesh* mesh, const struct aiScene* scene)
{
    for(unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        struct model_vertex* curr_vtx = &vtx_array[i];
        (*vtx_count)++;

        model_reset_vertex_bone_data(vtx_array, i);

        curr_vtx->position[0] = mesh->mVertices[i].x;
        curr_vtx->position[1] = mesh->mVertices[i].y;
        curr_vtx->position[2] = mesh->mVertices[i].z; 

        curr_vtx->normal[0] = mesh->mNormals[i].x;
        curr_vtx->normal[1] = mesh->mNormals[i].y;
        curr_vtx->normal[2] = mesh->mNormals[i].z;        

        if(mesh->mColors[0])
        {
            curr_vtx->color[0] = mesh->mColors[0][i].r;
            curr_vtx->color[1] = mesh->mColors[0][i].g;
            curr_vtx->color[2] = mesh->mColors[0][i].b;
            curr_vtx->color[3] = mesh->mColors[0][i].a;
        }
        else
        {
            curr_vtx->color[0] = atan2(mesh->mVertices[i].z,mesh->mVertices[i].x);
            curr_vtx->color[1] = atan2(mesh->mVertices[i].x,mesh->mVertices[i].y);
            curr_vtx->color[2] = atan2(mesh->mVertices[i].y,mesh->mVertices[i].z);
            curr_vtx->color[3] = 1.f;
        }

        if(mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
        {
            curr_vtx->tex_coords[0] = mesh->mTextureCoords[0][i].x; 
            curr_vtx->tex_coords[1] = mesh->mTextureCoords[0][i].y;
        }
    }

    for(unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        struct aiFace face = mesh->mFaces[i];
        for(unsigned int j = 0; j < face.mNumIndices; j++)
        {
            idx_array[*idx_count] = face.mIndices[j];
            (*idx_count)++;
        }
    }

    printf("parsed mesh with %i vertices %i faces... \n", mesh->mNumVertices, mesh->mNumFaces);
}

// FIXME: only until recently i had figured out that vertex array switching is an expensive operation
int create_model_vao(struct model_vertex* vtx_array, int vtx_count, int* idx_array, int idx_count, int* vertex_buffer, int* element_buffer)
{
    int vertex_array;

    sglc(glGenVertexArrays(1, &vertex_array));
    sglc(glGenBuffers(1, vertex_buffer));
    sglc(glGenBuffers(1, element_buffer));

    sglc(glBindVertexArray(vertex_array));

    sglc(glBindBuffer(GL_ARRAY_BUFFER, *vertex_buffer));
    sglc(glBufferData(GL_ARRAY_BUFFER, vtx_count * sizeof(struct model_vertex), &vtx_array[0], GL_STATIC_DRAW));

    sglc(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *element_buffer));
    sglc(glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx_count * sizeof(unsigned int), &idx_array[0], GL_STATIC_DRAW));
    
    // vec3: position
    sglc(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct model_vertex), (void*)0));
    sglc(glEnableVertexAttribArray(0));
    // vec3: normal
    sglc(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct model_vertex), (void*)offsetof(struct model_vertex, normal)));
    sglc(glEnableVertexAttribArray(1));
    // vec4: color
    sglc(glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(struct model_vertex), (void*)offsetof(struct model_vertex, color)));
    sglc(glEnableVertexAttribArray(2));
    // vec2: uv
    sglc(glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(struct model_vertex), (void*)offsetof(struct model_vertex, tex_coords)));
    sglc(glEnableVertexAttribArray(3));
    // ivec4: bone_ids
    sglc(glVertexAttribIPointer(4, 4, GL_INT, sizeof(struct model_vertex), (void*)offsetof(struct model_vertex, bone_ids)));
    sglc(glEnableVertexAttribArray(3));
    // vec4: weights
    sglc(glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(struct model_vertex), (void*)offsetof(struct model_vertex, weights)));
    sglc(glEnableVertexAttribArray(3));

    sglc(glBindVertexArray(0));

    return vertex_array;
}

static void model_parse_node(struct model* model, struct aiNode* node, const struct aiScene* scene)
{
    printf("node %s > ", &node->mName.data[0]);
    for(int i = 0; i < node->mNumMeshes; i++)
    {
        struct aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

        struct model_vertex* vtx_array = (struct model_vertex*)malloc(mesh->mNumVertices*sizeof(struct model_vertex));
        int* idx_array = (int*)malloc(mesh->mNumFaces*3*sizeof(int)); // safe to assume we'll have 3 vertices per face since we use Triangulate, do fix in future
        int idx_count = 0, vtx_count = 0;

        model_parse_mesh(vtx_array, &vtx_count, idx_array, &idx_count, mesh, scene);

        int element_buffer, vertex_buffer;
        int new_mesh = create_model_vao(vtx_array, vtx_count, idx_array, idx_count, &vertex_buffer, &element_buffer);
        model->meshes[model->mesh_count].vertex_array = new_mesh;
        model->meshes[model->mesh_count].element_buffer = element_buffer;
        model->meshes[model->mesh_count].vertex_buffer = vertex_buffer;
        model->meshes[model->mesh_count].element_count = idx_count;
        model->meshes[model->mesh_count].vtx_data = vtx_array;
        model->meshes[model->mesh_count].vtx_data_count = vtx_count;
        model_load_textures(&model->meshes[model->mesh_count], mesh, scene);
        model->mesh_count++;

        model_extract_bone_weights(&model->meshes[model->mesh_count], vtx_array, mesh, scene);

        free(idx_array);
    }
    for(int i = 0; i < node->mNumChildren; i++)
    {
        printf("new_node \nsglthing: start > ");
        model_parse_node(model, node->mChildren[i], scene);
    }
}


void load_model(char* file)
{
    struct model* sel_model = &models[models_loaded];
    sel_model->mesh_count = 0;
    
    const struct aiScene *scene = aiImportFile(file, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);
    if(!scene)
    {
        printf("sglthing: model %s not found\n", file);
        return;
    }

    printf("sglthing: start > ");
    model_parse_node(sel_model, scene->mRootNode, scene);
    printf("\n");

    strncpy(&sel_model->name[0], file, 64);

    models_loaded++;
}

struct model* get_model(char* file)
{
    for(int i = 0; i < 256; i++)
    {
        if(strncmp(file,models[i].name,64)==0)
            return &models[i];
    }
    return 0;
}