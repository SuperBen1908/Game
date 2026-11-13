

typedef enum {

  JSON_INVALID   =  0,

  JSON_NULL      =  1,
  JSON_TRUE      =  2,
  JSON_FALSE     =  3,

  JSON_NUM       =  4,
  JSON_STRING    =  5,
  JSON_ARRAY     =  6,
  JSON_OBJECT    =  7,

  JSON_ARRAYEND  =  8,
  JSON_OBJECTEND =  9,
  JSON_KEY       = 10,

} json_type_e;


typedef struct json_node_t {

  json_type_e type;

  union {
    r64 num;

    struct {
      u32  str_size;
      u8  *str;
    };
  };

  struct json_node_t *value;
  struct json_node_t *next;
  struct json_node_t *parent;

} json_node_t;


s32 json_next(u8 *buf, u32 len, u8 **next, json_node_t *content)
{
  json_type_e type = JSON_INVALID;
  u8 *p = buf;

  // skip white space
  while (len) {
    if (*p != ' '  &&
        *p != '\t' &&
        *p != '\n' &&
        *p != '\r')
      break;
    ++p; --len;
  }

  if (!len)
    return -1;

  // check each case untill a match
  do {
    if ((*p >= '0' && *p <= '9') || *p == '-') {
      type = JSON_NUM;

      r64 sign = 1.0;
      r64 num  = 0;
      r64 exp  = 1.0;

      // minus
      if (*p == '-') {
        sign = -1.0;
        ++p; --len;
        if (!len || *p < '0' || *p > '9')
          return -1;
      }

      // whole part
      do {
        u8 dig = *p - '0';
        num = num * 10 + (r64)dig;
        ++p; --len;
      } while (len && *p >= '0' && *p <= '9');

      if (len && *p == '.')
        ++p; --len;

      // frac part
      r64 step = 1.0;
      while (len && *p >= '0' && *p <= '9') {
        step *= 0.1;
        u8 dig = *p - '0';
        num += step * (r64)dig;
        ++p; --len;
      }

      // exp part
      if (len && *p == 'e' || *p == 'E') {
        ++p; --len;

        r64 exp_n = 0.0;
        r64 exp_step = 10.0;
        if (*p == '-') {
          exp_step = 0.1;
          ++p; --len;
          if (!len || *p < '0' || *p > '9')
            return -1;
        }

        while (len && *p >= '0' && *p <= '9') {
          u8 dig = *p - '0';
          exp_n += exp_n * 10 + (r64)dig;
          ++p; --len;
        }
        ASSERT(exp_n >= 0 && exp_n < 100);

        while (exp_n > 0.5) {
          exp_n -= 1.0;
          exp *= exp_step;
        }
      }

      content->num = sign * num * exp;

      break;
    }

    LOCAL_PERSIST struct {
      json_type_e type;
      u8 str[8];
    } literals[] = {
      { .str  = "true",
        .type = JSON_TRUE },
      { .str  = "false",
        .type = JSON_FALSE },
      { .str  = "null",
        .type = JSON_NULL },
    };

    for (u32 i = 0; i < arr_size(literals); ++i) {
      b32 found = TRUE;
      u32 s;
      for (s = 0; s < len && literals[i].str[s]; ++s) {
        if (p[s] != literals[i].str[s]) {
          found = FALSE;
          break;
        }
      }

      if (found) {
        type = literals[i].type;
        p += s; len -= s;
        break;
      }
    }

    if (type != JSON_INVALID)
      break;

    switch (*p) {
      case '[':  { type = JSON_ARRAY;     } break;
      case ']':  { type = JSON_ARRAYEND;  } break;
      case '{':  { type = JSON_OBJECT;    } break;
      case '}':  { type = JSON_OBJECTEND; } break;
      case '\"': {
        type = JSON_STRING;
        ++p; --len;

        u8 *str = p;
        u32 str_size = 0;

        b32 escape = FALSE;
        for (;;) {
          if (!len)
            return -1;
            
          if (*p == '\\')
            escape = !escape;
          else
            escape = FALSE;

          if (!escape && *p == '\"') {
            break;
          }

          ++p; --len; ++str_size;

        }

        content->str      = str;
        content->str_size = str_size;

      } break;
      default: { } break;
    }

    ++p; --len;

  } while (0);


  switch (type) {
    case JSON_ARRAY:
    case JSON_OBJECT: {
      if (next)
        *next = p;
      content->type = type;
      return 0;
    } break;
  }
  
  // skip white space
  while (len) {
    if (*p != ' '  &&
        *p != '\t' &&
        *p != '\n' &&
        *p != '\r')
      break;
    ++p; --len;
  }

  if (!len) {
    if (next)
      *next = p;
    content->type = type;
    return 0;
  }

  switch (*p) {
    case ']':
    case '}':
    break;

    case ':':
      if (type != JSON_STRING)
        return -1;
      type = JSON_KEY;
    case ',':
      ++p; --len;
    break;

    default: {
      return -1;
    } break;
  }

  if (next)
    *next = p;

  content->type = type;
  return 0;
}


s32 json_parse(u8 *buf, u32 len, json_node_t *root, arena_t *arena)
{
  s32 err = 0;

  json_node_t *node = root;
  *node = (json_node_t) { 0 };

  while (len) {

    u8 *next = buf;

    err = json_next(buf, len, &next, node);
    if (err)
      return err;

    if (node->type == JSON_KEY &&
       (!node->parent || node->parent->type != JSON_OBJECT))
      return -1;

    if (node->parent && node->parent->type == JSON_OBJECT &&
       (node->type != JSON_KEY && node->type != JSON_OBJECTEND))
      return -1;

    if (node->type == JSON_ARRAYEND &&
       (!node->parent || node->parent->type != JSON_ARRAY))
      return -1;

    if (node->type == JSON_OBJECTEND &&
       (!node->parent || node->parent->type != JSON_OBJECT))
      return -1;

    if (node->type == JSON_ARRAY  ||
        node->type == JSON_OBJECT ||
        node->type == JSON_KEY) {

      node->value = arena_alloc(arena, sizeof(json_node_t));
      *node->value = (json_node_t) { 0 };

      node->value->parent = node;
      node = node->value;
    }
    else {
      if (node->type == JSON_ARRAYEND || node->type == JSON_OBJECTEND) {
        node = node->parent;
        if (!node)
          return -1;
      }

      if (!node->parent)
        break;

      if (node->parent->type == JSON_KEY) {
        node = node->parent;
        if (!node->parent || node->parent->type != JSON_OBJECT)
          return -1;
      }

      node->next = arena_alloc(arena, sizeof(json_node_t));
      *node->next = (json_node_t) { 0 };
      node->next->parent = node->parent;
      node = node->next;
    }

    len -= (u32)(next - buf); 
    buf = next;
  }

  return 0;
}


json_node_t *json_dict_find(json_node_t *dict, u8 *str)
{
  ASSERT(dict);
  ASSERT(dict->type == JSON_OBJECT);
  u32 str_size;
  for (str_size = 0; str[str_size]; ++str_size);
  
  for (json_node_t *node = dict->value;
       node && node->type == JSON_KEY;
       node = node->next) {
    b32 str_eq = TRUE;
    if (node->str_size != str_size)
      continue;

    for (u32 i = 0; i < str_size; ++i) {
      if (node->str[i] != str[i]) {
        str_eq = FALSE;
        break;
      }
    }

    if (str_eq)
      return node->value;
  }

  return NULL;
}


u32 json_arr_len(json_node_t *arr)
{
  ASSERT(arr);
  ASSERT(arr->type == JSON_ARRAY);
  u32 arr_len = 0;
  
  for (json_node_t *node = arr->value;
       node && node->type != JSON_ARRAYEND;
       node = node->next)
    ++arr_len;

  return arr_len;
}


