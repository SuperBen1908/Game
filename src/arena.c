
#define arena_init(_pool, _size) \
        ((arena_t) {             \
          .pool = (u8*)(_pool),  \
          .top  = 0,             \
          .size = (_size),       \
        })


#define arena_reset(_arena) ((_arena)->top = 0) 

void *arena_alloc(arena_t *arena, u32 size)
{
  u8 *ret = &arena->pool[arena->top];

  arena->top += size;
  ASSERT(arena->top <= arena->size);

  return ret;
}

arena_t arena_sub(arena_t *arena, u32 size)
{
  arena_t ret = {
    .pool = &arena->pool[arena->top],
    .top  = 0,
    .size = size,
  };

  arena->top += size;
  ASSERT(arena->top < arena->size);

  return ret;
}

