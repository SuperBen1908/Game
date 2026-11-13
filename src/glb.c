
typedef struct {
  u32 len;
  u8 *data;
} glb_chunk_t;

typedef enum {
  GLB_NUM_Z   = 5120,
  GLB_NUM_S8  = 5120,
  GLB_NUM_U8  = 5121,
  GLB_NUM_S16 = 5122,
  GLB_NUM_U16 = 5123,
  GLB_NUM_S32 = 5124,
  GLB_NUM_U32 = 5125,
  GLB_NUM_R32 = 5126,
} glb_num_type_e;

LOCAL_PERSIST u32 num_type_size_gp[] = {
  1, // GLB_NUM_S8 
  1, // GLB_NUM_U8 
  2, // GLB_NUM_S16
  2, // GLB_NUM_U16
  4, // GLB_NUM_S32
  4, // GLB_NUM_U32
  4, // GLB_NUM_R32
};

typedef enum {
  GLB_VEC_SCALAR = 0,
  GLB_VEC_VEC2   = 1,
  GLB_VEC_VEC3   = 2,
  GLB_VEC_VEC4   = 3,
  GLB_VEC_MAT2   = 4,
  GLB_VEC_MAT3   = 5,
  GLB_VEC_MAT4   = 6,
} glb_vec_type_e;

LOCAL_PERSIST u8 *vec_type_gp[] = {
  (u8*)"SCALAR",
  (u8*)"VEC2",
  (u8*)"VEC3",
  (u8*)"VEC4",
  (u8*)"MAT2",
  (u8*)"MAT3",
  (u8*)"MAT4",
};

LOCAL_PERSIST u32 vec_type_size_gp[] = {
  1,  // GLB_VEC_SCALAR
  2,  // GLB_VEC_VEC2  
  3,  // GLB_VEC_VEC3  
  4,  // GLB_VEC_VEC4  
  4,  // GLB_VEC_MAT2  
  9,  // GLB_VEC_MAT3  
  16, // GLB_VEC_MAT4  
};

typedef struct {
  u32 id;
} glb_scene_t;

typedef struct {
  s32 parent;
  u32 mesh;
  vec3_t translation;
  vec3_t rotation;
} glb_node_t;

typedef struct {
  // all accessors
  u32 position;
  u32 normal;
  u32 texcoord;

  u32 indices;
  u32 material;
} glb_mesh_t;

typedef struct {
  u32 bufferview;
  glb_num_type_e num_type;
  u32 count;
  glb_vec_type_e vec_type;

  // sparse data
  b32 sparse;
  u32 indices_bufferview;
  u32 indices_count;
  glb_num_type_e indices_num_type;
} glb_accessor_t;

typedef struct {
  u32 length;
  u32 offset;
  u32 target;
} glb_bufferview_t;


typedef struct {
  arena_t arena;

  json_node_t *root;

  u32 n_scenes;
  glb_scene_t *scenes;

  u32 n_nodes;
  glb_node_t *nodes;

  u32 n_meshes;
  glb_mesh_t *meshes;

  u32 n_accessors;
  glb_accessor_t *accessors;

  u32 n_bufferviews;
  glb_bufferview_t *bufferviews;

  glb_chunk_t bin_chunk;

  u32 n_triangles;
  triangle_t *triangles;

} glb_data_t;


s32 glb_parse_json(glb_chunk_t *json_chunk, glb_data_t *glb_data)
{
  s32 err = 0;

  glb_data->root = arena_alloc(&glb_data->arena, sizeof(json_node_t));
  err = json_parse(json_chunk->data, json_chunk->len, glb_data->root, &glb_data->arena);
  if (err)
    return err;

  // all properties
  // json_node_t *asset       = json_dict_find(glb_data->root, (u8 *)"asset");
  // json_node_t *scene       = json_dict_find(glb_data->root, (u8 *)"scene");
  // json_node_t *scenes      = json_dict_find(glb_data->root, (u8 *)"scenes");
  // json_node_t *materials   = json_dict_find(glb_data->root, (u8 *)"materials");
  // json_node_t *buffers     = json_dict_find(glb_data->root, (u8 *)"buffers");

  // parse nodes
  json_node_t *nodes = json_dict_find(glb_data->root, (u8 *)"nodes");
  glb_data->nodes = arena_alloc(&glb_data->arena, 0);
  if (nodes) {
    if (nodes->type != JSON_ARRAY)
      return -1;

    for (json_node_t *node = nodes->value;
         node && node->type == JSON_OBJECT;
         node = node->next) {

      glb_node_t *node_data = arena_alloc(&glb_data->arena, sizeof(glb_node_t));

      node_data->parent = -1;
      json_node_t *children = json_dict_find(node, (u8 *)"children");
      if (children) {
        if (children->type != JSON_ARRAY)
          return -1;
        json_node_t *field = children->value;
        for (u32 i = 0;
             field && field->type == JSON_NUM;
             field = field->next, ++i) {
          u32 child_node = (u32)field->num;
          if (glb_data->nodes[child_node].parent != -1)
            return -1;
          glb_data->nodes[child_node].parent = glb_data->n_nodes;
        }
      }

      json_node_t *mesh = json_dict_find(node, (u8 *)"mesh");
      if (!mesh || mesh->type != JSON_NUM)
        return -1;
      node_data->mesh = (u32)mesh->num;

      node_data->translation = (vec3_t){ 0 };
      json_node_t *translation = json_dict_find(node, (u8 *)"translation");
      if (translation) {
        if (translation->type != JSON_ARRAY)
          return -1;
        json_node_t *field = translation->value;
        for (u32 i = 0;
             field && field->type == JSON_NUM;
             field = field->next, ++i) {
          node_data->translation.v[i] = (r32)field->num;
        }
      }

      node_data->rotation = (vec3_t){ 0 };
      json_node_t *rotation = json_dict_find(node, (u8 *)"rotation");
      if (rotation) {
        if (rotation->type != JSON_ARRAY)
          return -1;
        json_node_t *field = rotation->value;
        for (u32 i = 0;
             field && field->type == JSON_NUM;
             field = field->next, ++i) {
          node_data->rotation.v[i] = (r32)field->num;
        }
      }

      ++glb_data->n_nodes;
    }
  }

  // parse meshes
  glb_data->meshes = arena_alloc(&glb_data->arena, 0);

  json_node_t *meshes = json_dict_find(glb_data->root, (u8 *)"meshes");
  if (meshes) {
    if (meshes->type != JSON_ARRAY)
      return -1;

    for (json_node_t *mesh = meshes->value;
         mesh && mesh->type == JSON_OBJECT;
         mesh = mesh->next) {

      glb_mesh_t *mesh_data = arena_alloc(&glb_data->arena, sizeof(glb_mesh_t));

      json_node_t *primitives = json_dict_find(mesh, (u8 *)"primitives");
      if (!primitives || primitives->type != JSON_ARRAY)
        return -1;

      for (json_node_t *primitive = primitives->value;
           primitive && primitive->type == JSON_OBJECT;
           primitive = primitive->next) {

        json_node_t *attributes = json_dict_find(primitive, (u8 *)"attributes");
        if (attributes && attributes->type == JSON_OBJECT) {
          json_node_t *position = json_dict_find(attributes, (u8 *)"POSITION");
          if (position && position->type == JSON_NUM)
            mesh_data->position = (u32)position->num;

          json_node_t *normal = json_dict_find(attributes, (u8 *)"NORMAL");
          if (normal && normal->type == JSON_NUM)
            mesh_data->normal  = (u32)normal->num;

          json_node_t *texcoord = json_dict_find(attributes, (u8 *)"TEXCOORD_0");
          if (texcoord && texcoord->type == JSON_NUM)
            mesh_data->texcoord = (u32)texcoord->num;
        }

        json_node_t *indices = json_dict_find(primitive, (u8 *)"indices");
        if (indices && indices->type == JSON_NUM)
          mesh_data->indices = (u32)indices->num;

        json_node_t *material = json_dict_find(primitive, (u8 *)"material");
        if (material && material->type == JSON_NUM)
          mesh_data->material = (u32)material->num;
      }

      ++glb_data->n_meshes;
    }
  }


  // parse accessors
  glb_data->accessors = arena_alloc(&glb_data->arena, 0);

  json_node_t *accessors = json_dict_find(glb_data->root, (u8 *)"accessors");
  if (accessors) {
    if (accessors->type != JSON_ARRAY)
      return -1;

    for (json_node_t *accessor = accessors->value;
         accessor && accessor->type == JSON_OBJECT;
         accessor = accessor->next) {

      glb_accessor_t *accessor_data = arena_alloc(&glb_data->arena, sizeof(glb_accessor_t));

      json_node_t *bufferview = json_dict_find(accessor, (u8 *)"bufferView");
      if (bufferview) {
        if (bufferview->type != JSON_NUM)
          return -1;

        accessor_data->bufferview = (u32)bufferview->num;
      }
      else {
        json_node_t *sparse = json_dict_find(accessor, (u8 *)"sparse");
        if (!sparse)
          return -1;
        accessor_data->sparse = TRUE;

        json_node_t *indices_count = json_dict_find(sparse, (u8 *)"count");
        if (!indices_count || indices_count->type != JSON_NUM)
          return -1;
        accessor_data->indices_count = (u32)indices_count->num;

        json_node_t *indices = json_dict_find(sparse, (u8 *)"indices");
        if (!indices || indices ->type != JSON_OBJECT)
          return -1;

        json_node_t *indices_bufferview = json_dict_find(indices, (u8 *)"bufferView");
        if (!indices_bufferview || indices_bufferview->type != JSON_NUM)
          return -1;
        accessor_data->indices_bufferview = (u32)indices_bufferview->num;

        json_node_t *component_type = json_dict_find(indices, (u8 *)"componentType");
        if (!component_type || component_type->type != JSON_NUM)
          return -1;

        accessor_data->indices_num_type = (u32)component_type->num;
        switch (accessor_data->indices_num_type) {
          case GLB_NUM_S8:  case GLB_NUM_U8:
          case GLB_NUM_S16: case GLB_NUM_U16:
          case GLB_NUM_S32: case GLB_NUM_U32: case GLB_NUM_R32: break;
          default: return -1; break;
        }
      }

      json_node_t *component_type = json_dict_find(accessor, (u8 *)"componentType");
      if (!component_type || component_type->type != JSON_NUM)
        return -1;

      accessor_data->num_type = (u32)component_type->num;
      switch (accessor_data->num_type) {
        case GLB_NUM_S8:  case GLB_NUM_U8:
        case GLB_NUM_S16: case GLB_NUM_U16:
        case GLB_NUM_S32: case GLB_NUM_U32: case GLB_NUM_R32: break;
        default: return -1; break;
      }
      // accessor_data->num_type_size = num_type_size_gp[accessor_data->num_type-GLB_NUM_Z];

      json_node_t *count = json_dict_find(accessor, (u8 *)"count");
      if (!count || count->type != JSON_NUM)
        return -1;
      accessor_data->count = (u32)count->num;

      json_node_t *type = json_dict_find(accessor, (u8 *)"type");
      if (!type || type->type != JSON_STRING)
        return -1;

      for (u32 vec_type = 0; vec_type < arr_size(vec_type_gp); ++vec_type) {
        b32 str_eq = TRUE;
        for (u32 i = 0; i < type->str_size && vec_type_gp[vec_type][i]; ++i) {
          if (type->str[i] != vec_type_gp[vec_type][i]) {
            str_eq = FALSE;
            break;
          }
        }
        if (str_eq) {
          accessor_data->vec_type = vec_type;
          // accessor_data->vec_type_size = vec_type_size_gp[vec_type];
          break;
        }
      }

      ++glb_data->n_accessors;
    }
  }


  // parse bufferviews
  glb_data->bufferviews = arena_alloc(&glb_data->arena, 0);
  
  json_node_t *bufferviews = json_dict_find(glb_data->root, (u8 *)"bufferViews");
  if (bufferviews) {
    if (bufferviews->type != JSON_ARRAY)
      return -1;

    for (json_node_t *bufferview = bufferviews->value;
         bufferview && bufferview->type == JSON_OBJECT;
         bufferview = bufferview->next) {

      glb_bufferview_t *bufferview_data = arena_alloc(&glb_data->arena, sizeof(glb_bufferview_t));

      json_node_t *buffer = json_dict_find(bufferview, (u8 *)"buffer");
      if (!buffer || buffer->type != JSON_NUM || buffer->num)
        return -1;

      json_node_t *length = json_dict_find(bufferview, (u8 *)"byteLength");
      if (!length || length->type != JSON_NUM)
        return -1;
      bufferview_data->length = (u32)length->num;

      json_node_t *offset = json_dict_find(bufferview, (u8 *)"byteOffset");
      if (!offset || offset->type != JSON_NUM)
        return -1;
      bufferview_data->offset = (u32)offset->num;

      json_node_t *target = json_dict_find(bufferview, (u8 *)"target");
      if (target) {
        if (target->type != JSON_NUM)
          return -1;
        bufferview_data->target = (u32)target->num;
      }

      ++glb_data->n_bufferviews;
    }
  }

  return err;
}


s32 glb_render(glb_data_t *glb_data)
{
  s32 err = 0;

  glb_data->triangles = arena_alloc(&glb_data->arena, 0);
  
  // go in opposite order, from top nodes to bottom nodes
  for (s32 node_idx = glb_data->n_nodes; node_idx >= 0; --node_idx) {
    glb_node_t *node = &glb_data->nodes[node_idx];
    glb_mesh_t *mesh = &glb_data->meshes[node->mesh];
    glb_accessor_t *pos_accessor = &glb_data->accessors[mesh->position];
    glb_bufferview_t *pos_bufferview = &glb_data->bufferviews[pos_accessor->bufferview];

    vec3_t translation = { 0 };
    // dont go for more than 1000 times
    glb_node_t *trans_node = node;
    for (u32 i = 0; i < 1000; ++i) {
      translation = math_add_v3(translation, trans_node->translation);

      if (trans_node->parent == -1)
        break;
        
      trans_node = &glb_data->nodes[trans_node->parent];
    }

    if (pos_accessor->num_type != GLB_NUM_R32)
      return -1;
    if (pos_accessor->vec_type != GLB_VEC_VEC3)
      return -1;
    if (pos_bufferview->length !=
        pos_accessor->count                      *
        vec_type_size_gp[pos_accessor->vec_type] *
        num_type_size_gp[pos_accessor->num_type-GLB_NUM_Z])
      return -1;
    vec3_t *positions = (vec3_t *)&glb_data->bin_chunk.data[pos_bufferview->offset];

    if (pos_accessor->count < 3)
      return -1;

    glb_accessor_t *indices_accessor = &glb_data->accessors[mesh->indices];
    glb_bufferview_t *indices_bufferview = &glb_data->bufferviews[indices_accessor->bufferview];
    if (indices_accessor->num_type != GLB_NUM_U16)
      return -1;
    if (indices_accessor->vec_type != GLB_VEC_SCALAR)
      return -1;
    if (indices_bufferview->length !=
        indices_accessor->count                      *
        vec_type_size_gp[indices_accessor->vec_type] *
        num_type_size_gp[indices_accessor->num_type-GLB_NUM_Z])
      return -1;
    u16 *indices = (u16 *)&glb_data->bin_chunk.data[indices_bufferview->offset];

    u32 n_triangles = indices_accessor->count / 3;
    arena_alloc(&glb_data->arena, n_triangles*sizeof(triangle_t));
    triangle_t *dst_triangle = &glb_data->triangles[glb_data->n_triangles];
    glb_data->n_triangles += n_triangles;

    triangle_t triangle = { 0 };

    for (u32 i = 0; i < indices_accessor->count; i += 3) {
      dst_triangle->p1 = positions[indices[i]];
      dst_triangle->p2 = positions[indices[i+1]];
      dst_triangle->p3 = positions[indices[i+2]];

      dst_triangle->p1 = math_add_v3(dst_triangle->p1, translation);
      dst_triangle->p2 = math_add_v3(dst_triangle->p2, translation);
      dst_triangle->p3 = math_add_v3(dst_triangle->p3, translation);

      ++dst_triangle;
    }
  }

  return err;
}


s32 glb_parse(u8 *glb_file_buf, u32 glb_file_size, glb_data_t *glb_data)
{
  s32 err = 0;

  u8 *buf_p = glb_file_buf;
  u32 magic = *(u32*)buf_p;
  if (magic != 'FTlg') return -1; // glTF
  buf_p += sizeof(u32);

  u32 version = *(u32*)buf_p;
  buf_p += sizeof(u32);

  u32 total_len = *(u32*)buf_p;
  if (total_len != glb_file_size) return -1;
  buf_p += sizeof(u32);

  glb_chunk_t json_chunk = { 0 };
  while (buf_p - glb_file_buf < total_len - 8) // -8 to ensure space for headers, dont allow empty data
  {
    glb_chunk_t chunk;
    chunk.len = *(u32*)buf_p;
    buf_p += sizeof(u32);

    u32 chunk_type = *(u32*)buf_p;
    buf_p += sizeof(u32);
    if (buf_p - glb_file_buf + chunk.len > total_len) return -1;

    chunk.data = buf_p;
    switch (chunk_type) {
      // JSON
      case 'NOSJ': {
        json_chunk = chunk;
      } break;

      // BIN
      case 'NIB': {
        glb_data->bin_chunk = chunk;
      } break;

      default: {
      } break;
    }
    if (err)
      return err;

    buf_p += chunk.len;
  }

  err = glb_parse_json(&json_chunk, glb_data);
  if (err)
    return err;

  err = glb_render(glb_data);
  if (err)
    return err;

  return err;
}



b32 glb_test(game_sim_region_t *sim_region)
{
  s32 err = 0;
  u32 glb_file_size = 0;
  u8 *glb_file_buf = NULL;
  glb_data_t glb_data = { 0 };
  glb_file_buf = platform_DEBUG_read_entire_file_f("W:\\game\\data\\test.glb", &glb_file_size);
  ASSERT(glb_file_buf != NULL);

#define GLB_MAX_SIZE MB(64)
  LOCAL_PERSIST u8 glb_buf[GLB_MAX_SIZE];
  glb_data.arena = arena_init(glb_buf, GLB_MAX_SIZE);

  err = glb_parse(glb_file_buf, glb_file_size, &glb_data);
  ASSERT(!err);

  triangle_t *dst_triangle = &sim_region->triangles[sim_region->triangle_count];
  triangle_t *src_triangle = glb_data.triangles;
  for (u32 i = 0; i < glb_data.n_triangles; ++i) {
    *dst_triangle = *(src_triangle++);
    dst_triangle->p1 = math_mul_c_v3(1000, dst_triangle->p1);
    dst_triangle->p2 = math_mul_c_v3(1000, dst_triangle->p2);
    dst_triangle->p3 = math_mul_c_v3(1000, dst_triangle->p3);
    ++dst_triangle;
  }

  sim_region->triangle_count += glb_data.n_triangles;
  
  return FALSE;
}



