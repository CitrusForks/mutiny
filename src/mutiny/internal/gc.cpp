#include "gc.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define UNMARK 0
#define MARK 1
#define REMARK 2
#define SWEEP 3

struct GcBlock
{
  void *ptr;
  size_t size;
  void (*deleter)(void*);
  int generation;
  struct GcBlock *next;
  struct GcBlock *prev;
};

struct GcContext
{
  int state;
  struct GcBlock **processList;
  int processListCount;

  struct GcBlock *root;
  struct GcBlock *last;
  int needsCollect;
};

struct GcContext *gc_context()
{
  struct GcContext *rtn = NULL;

  rtn = (struct GcContext*)calloc(1, sizeof(*rtn));
  rtn->processList = (struct GcBlock**)calloc(1024, sizeof(*rtn->processList));

  return rtn;
}

/******************************************************************************
 * gc_addblock
 *
 * Reallocate the blocks array to make room for a copy of the new block.
 ******************************************************************************/
int gc_addblock(struct GcContext *ctx, struct GcBlock *block)
{
  if(ctx->last == NULL)
  {
    ctx->root = block;
    ctx->last = block;

    return 0;
  }

  ctx->last->next = block;
  block->prev = ctx->last;
  ctx->last = block;

  ctx->needsCollect = 1;

  return 0;
}

/******************************************************************************
 * gc_purgeblocks
 *
 * Iterate backwards through the blocks array to see if each one is marked
 * as to be purged. If so, execute its delete function on its contained pointer
 * and unless it is the last one of the array, move all the other blocks up to
 * fill up the space and replace it.
 ******************************************************************************/
void gc_destroy(struct GcContext *ctx)
{
  struct GcBlock *tofree = NULL;
  struct GcBlock *block = NULL;
  int destroyCount = 0;

  block = ctx->last;

  if(block != NULL)
  {
    while(block != NULL)
    {
      if(block->deleter != NULL)
      {
        block->deleter(block->ptr);
      }

      if(block->size > 0)
      {
        free(block->ptr); block->ptr = NULL;
      }

      tofree = block;
      block = block->prev;
      free(tofree); tofree = NULL;
      destroyCount ++;
    }
  }

  if(destroyCount > 0)
  {
    printf("Garbage Collector: %i destroys\n", destroyCount);
  }

  free(ctx->processList);
  free(ctx);
}

void gc_purgeblocks(struct GcContext *ctx)
{
  struct GcBlock *tofree = NULL;
  struct GcBlock *block = NULL;
  int purgeCount = 0;

  block = ctx->last;

  if(block == NULL)
  {
    return;
  }

  while(block != NULL)
  {
    if(block->generation == -1)
    {
      if(block->deleter != NULL)
      {
        block->deleter(block->ptr);
      }

      if(block->size > 0)
      {
        free(block->ptr); block->ptr = NULL;
      }

      block->prev->next = block->next;

      if(block->next != NULL)
      {
        block->next->prev = block->prev;
      }

      if(ctx->last == block)
      {
        ctx->last = block->prev;
      }

      tofree = block;
      block = block->prev;
      free(tofree); tofree = NULL;
      purgeCount ++;
    }
    else
    {
      block = block->prev;
    }
  }

  if(purgeCount > 0)
  {
    printf("Garbage Collector: %i purges\n", purgeCount);
  }
}

/******************************************************************************
 * gc_alloc
 *
 * Allocate the space specified by size and add block to be later scanned and
 * evaluated for deletion.
 ******************************************************************************/
void *gc_alloc(struct GcContext *ctx, size_t size)
{
  void *rtn = NULL;
  struct GcBlock *newBlock = NULL;

  rtn = calloc(1, size);

  if(rtn == NULL)
  {
    return NULL;
  }

  newBlock = (struct GcBlock*)calloc(1, sizeof(*newBlock));

  if(newBlock == NULL)
  {
    free(rtn); rtn = NULL;
    return NULL;
  }

  newBlock->ptr = rtn;
  newBlock->size = size;

  if(gc_addblock(ctx, newBlock) != 0)
  {
    free(newBlock); newBlock = NULL;
    free(rtn); rtn = NULL;
    return NULL;
  }

  return rtn;
}

int gc_finalizer(struct GcContext *ctx, void *ptr, void (*deleter)(void*))
{
  struct GcBlock *block = NULL;
  struct GcBlock *newBlock = NULL;

  if(ptr == NULL)
  {
    return 1;
  }

  if(ctx->last == NULL)
  {
    return 1;
  }

  block = ctx->last;

  while(block != NULL)
  {
    if(block->ptr == ptr)
    {
      block->deleter = deleter;
      return 0;
    }

    block = block->prev;
  }

  newBlock = (struct GcBlock*)calloc(1, sizeof(*newBlock));

  if(newBlock == NULL)
  {
    return 1;
  }

  newBlock->ptr = ptr;
  newBlock->deleter = deleter;

  if(gc_addblock(ctx, newBlock) != 0)
  {
    free(newBlock); newBlock = NULL;
    return 1;
  }

  return 0;
}

void gc_scan_block(struct GcContext *ctx, struct GcBlock *block)
{
  struct GcBlock *currentBlock = NULL;
  uintptr_t current = 0;
  uintptr_t end = 0;

  block->generation = 1;

  if(block->size <= 0)
  {
    return;
  }

  current = (uintptr_t)block->ptr;
  end = current + block->size;
  end -= sizeof(block->ptr);

  while(current <= end)
  {
    void **test = (void**)current;
    currentBlock = ctx->root;

    while(currentBlock != NULL)
    {
      // We will let self references pass
      //if(currentBlock == block)
      //{
      //  currentBlock = currentBlock->next;
      //  continue;
      //}

      if(*test == currentBlock->ptr)
      {
        if(currentBlock->generation == -1)
        {
          gc_scan_block(ctx, currentBlock);
        }

        break;
      }

      currentBlock = currentBlock->next;
    }

    current++;
  }
}

int gc_rescan_block(struct GcContext *ctx, struct GcBlock *block)
{
  struct GcBlock *currentBlock = NULL;
  uintptr_t current = 0;
  uintptr_t end = 0;

  currentBlock = ctx->root;

  while(currentBlock != NULL)
  {
    if(currentBlock->size <= 0)
    {
      currentBlock = currentBlock->next;
      continue;
    }

    if(currentBlock->generation == -1)
    {
      currentBlock = currentBlock->next;
      continue;
    }

    current = (uintptr_t)currentBlock->ptr;
    end = current + currentBlock->size;
    end -= sizeof(currentBlock->ptr);

    while(current <= end)
    {
      void **test = (void**)current;

      if(*test == block->ptr)
      {
        block->generation = 1;

        return 1;
      }

      current++;
    }

    currentBlock = currentBlock->next;
  }

  return 0;
}

void gc_scan_block_incr(struct GcContext *ctx, struct GcBlock *block)
{
  struct GcBlock *currentBlock = NULL;
  uintptr_t current = 0;
  uintptr_t end = 0;

  block->generation = 1;

  if(block->size <= 0)
  {
    return;
  }

  current = (uintptr_t)block->ptr;
  end = current + block->size;
  end -= sizeof(block->ptr);

  while(current <= end)
  {
    void **test = (void**)current;
    currentBlock = ctx->root;

    while(currentBlock != NULL)
    {
      // We will let self references pass
      //if(currentBlock == block)
      //{
      //  currentBlock = currentBlock->next;
      //  continue;
      //}

      if(*test == currentBlock->ptr)
      {
        if(currentBlock->generation != 1)
        {
          ctx->processList[ctx->processListCount] = currentBlock;
          ctx->processListCount++;
        }

        break;
      }

      currentBlock = currentBlock->next;
    }

    current++;
  }
}

/******************************************************************************
 * gc_collect
 *
 * For each block (other than the root), go through all the other blocks and
 * scan through the data pointed to by ptr to look for references to the prior
 * block. If nothing is found, mark it as eligible for purge and then trigger
 * a purge of any blocks.
 ******************************************************************************/
void gc_collect(struct GcContext *ctx)
{
  struct GcBlock *block = NULL;

  if(ctx->needsCollect == 0)
  {
    return;
  }

  if(ctx->root == NULL)
  {
    return;
  }

  block = ctx->root;

  while(block != NULL)
  {
    block->generation = -1;
    block = block->next;
  }

  gc_scan_block(ctx, ctx->root);

  gc_purgeblocks(ctx);

  ctx->needsCollect = 0;
  ctx->state = UNMARK;
}

void gc_collect_incr(struct GcContext *ctx)
{
  struct GcBlock *block = NULL;

  if(ctx->state == UNMARK)
  {
    printf("Marking blocks\n");
    block = ctx->root;

    while(block != NULL)
    {
      block->generation = -1;
      block = block->next;
    }

    ctx->processList[0] = ctx->root;
    ctx->processListCount = 1;
    ctx->state = MARK;
  }
  else if(ctx->state == MARK)
  {
    block = ctx->processList[ctx->processListCount - 1];
    ctx->processListCount--;
    gc_scan_block_incr(ctx, block);

    if(ctx->processListCount <= 0)
    {
      ctx->state = REMARK;
    }
  }
  else if(ctx->state == REMARK)
  {
    if(ctx->processListCount <= 0)
    {
      printf("Remarking blocks\n");
      block = ctx->root;

      while(block != NULL)
      {
        if(block->generation == -1)
        {
          ctx->processList[ctx->processListCount] = block;
          ctx->processListCount++;
        }

        block = block->next;
      }

      printf("%i blocks set to be remarked\n", ctx->processListCount);
    }

    if(ctx->processListCount > 0)
    {
      if(gc_rescan_block(ctx, ctx->processList[ctx->processListCount - 1]) == 1)
      {
        ctx->processListCount = 0;
        return;
      }

      ctx->processListCount--;
    }

    if(ctx->processListCount <= 0)
    {
      ctx->state = SWEEP;
    }
  }
  else if(ctx->state == SWEEP)
  {
    printf("Sweeping blocks\n");
    gc_purgeblocks(ctx);
    ctx->state = UNMARK;
  }
}
