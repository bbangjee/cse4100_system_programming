#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

#define WSIZE 4             /* word 및 header/footer의 크기 (bytes) */
#define DSIZE 8             /* Double Word의 크기 (bytes) */
#define CHUNKSIZE (1 << 12) /* heap 한 번 늘릴 때 늘리는 사이즈 (4kb) */
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) /* 크기와 할당 여부 bit 합쳐서 하나의 워드로 */

#define GET(p) (*(unsigned int *)(p))              /* word 읽기 */
#define PUT(p, val) (*(unsigned int *)(p) = (val)) /* word 쓰기 */

#define GET_SIZE(p) (GET(p) & ~0x7) /* 크기 추출 */
#define GET_ALLOC(p) (GET(p) & 0x1) /* 할당 여부 추출 */

#define HDRP(bp) ((char *)(bp) - WSIZE)                      /* block ptr로 header 주소 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /* block ptr로 footer 주소 */

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) /* block ptr로 다음 block ptr 주소 */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) /* block ptr로 이전 block ptr 주소 */

#define NEXT_PTR(bp) ((char *)(bp))
#define PREV_PTR(bp) ((char *)(bp + WSIZE))

static char *heap_listp;
static char *heap_first_free_block;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void remove_free_block(void *bp);
static void insert_free_block(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);

team_t team = {
    /* Your student ID */
    "20190808",
    /* Your full name*/
    "JeeHyeok Bang",
    /* Your email address */
    "qkdwl9584@naver.com",
};

/* extend_heap */
static void *extend_heap(size_t words)
{
  char *block_ptr;
  size_t size;

  size = ((words + 1) >> 1) << 1;
  size = size * WSIZE; // byte 변환
  size = ALIGN(size);  // 8 byte 정렬
  
  if ((long)(block_ptr = mem_sbrk(size)) == -1)
    return NULL;

  PUT(HDRP(block_ptr), PACK(size, 0));         /* 블록 헤더 설정*/
  PUT(FTRP(block_ptr), PACK(size, 0));         /* 블록 푸터 설정*/
  PUT(HDRP(NEXT_BLKP(block_ptr)), PACK(0, 1)); /* 새 에필로그 블록 */

  return coalesce(block_ptr);
}

/* coalesce */
static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))) || PREV_BLKP(bp) == bp;
  size_t prev_size = GET_SIZE(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(bp)));

  size_t new_size = GET_SIZE(HDRP(bp));

  /* 1. prev만 free */
  if (!prev_alloc && next_alloc)
  {
    new_size += prev_size;
    bp = PREV_BLKP(bp);
    remove_free_block(bp);
    PUT(HDRP(bp), PACK(new_size, 0));
    PUT(FTRP(bp), PACK(new_size, 0));
  }
  /* 2. next만 free */
  else if (prev_alloc && !next_alloc)
  {
    new_size += next_size;
    remove_free_block(NEXT_BLKP(bp));
    PUT(HDRP(bp), PACK(new_size, 0));
    PUT(FTRP(bp), PACK(new_size, 0));
  }
  /* 3. prev와 next 모두 free */
  else if (!prev_alloc && !next_alloc)
  {
    new_size += prev_size;
    new_size += next_size;
    remove_free_block(NEXT_BLKP(bp));
    bp = PREV_BLKP(bp);
    remove_free_block(bp);
    PUT(HDRP(bp), PACK(new_size, 0));
    PUT(FTRP(bp), PACK(new_size, 0));
  }
  insert_free_block(bp);
  
  return bp;
}

/* insert_free_block */
static void insert_free_block(void *bp)
{
  if (heap_first_free_block == NULL)
  {
    heap_first_free_block = bp;
    PUT(NEXT_PTR(bp), 0);
    PUT(PREV_PTR(bp), 0);
  }
  else
  {
    PUT(NEXT_PTR(bp), (unsigned int)heap_first_free_block);
    PUT(PREV_PTR(bp), 0);
    PUT(PREV_PTR(heap_first_free_block), (unsigned int)bp);
    heap_first_free_block = bp;
  }
}

/* remove_free_block */
static void remove_free_block(void *bp)
{
  if (bp == NULL)
    return;

  char *next_ptr = (char *)GET(NEXT_PTR(bp));
  char *prev_ptr = (char *)GET(PREV_PTR(bp));

  if (prev_ptr == NULL && next_ptr == NULL)
    heap_first_free_block = NULL;
  else if (prev_ptr == NULL)
  {
    heap_first_free_block = next_ptr;
    PUT(PREV_PTR(next_ptr), 0);
  }
  else if (next_ptr == NULL)
  {
    PUT(NEXT_PTR(prev_ptr), 0);
  }

  else
  {
    PUT(NEXT_PTR(prev_ptr), (unsigned int)next_ptr); // prev->next = next
    PUT(PREV_PTR(next_ptr), (unsigned int)prev_ptr);
  }
}

/* mm_init */
int mm_init(void)
{
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    return -1;

  PUT(heap_listp, 0);                            /* 패딩: 1 word */
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* 프롤로그 헤더 */
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* 프롤로그 푸터 */
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* 에필로그 헤더 */

  /* 첫 번째 free 블록 포인터는 프롤로그 푸터를 가리킴 */
  // heap_first_free_block = heap_listp + (2 * WSIZE);
  heap_first_free_block = NULL;

  // CHUNKSIZE 바이트의 free 블록으로 힙 확장
  if (extend_heap(4) == NULL)
    return -1;

  return 0;
}

/* mm_malloc */
void *mm_malloc(size_t size)
{
  size_t asize;
  size_t extendsize;
  char *bp;
  if (size == 0)
    return NULL;

  if (size <= DSIZE)
    asize = 2 * DSIZE;
  else
    asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

  if ((bp = find_fit(asize)) != NULL)
  {
    place(bp, asize);
    return bp;
  }

  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    return NULL;

  place(bp, asize);
  
  return bp;
}

/* mm_free */
void mm_free(void *bp)
{
  if (bp == NULL)
    return;
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
  
  return;
}

/* mm_realloc */
void *mm_realloc(void *ptr, size_t size)
{
  if (ptr == NULL)
    return mm_malloc(size);
  if (size == 0)
  {
    mm_free(ptr);
    return NULL;
  }

  size_t oldsize = GET_SIZE(HDRP(ptr));
  size_t newsize;

  if (size <= DSIZE)
    newsize = 2 * DSIZE;
  else
    newsize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  
  
  if (newsize <= oldsize)
    return ptr;
  
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
  size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

  if (!next_alloc && ((oldsize + next_size) >= newsize))
  {
    remove_free_block(NEXT_BLKP(ptr));
    PUT(HDRP(ptr), PACK(oldsize + next_size, 1));
    PUT(FTRP(ptr), PACK(oldsize + next_size, 1));
    return ptr;
  }

  void *new_ptr = mm_malloc(newsize);
  if (new_ptr == NULL)
    return NULL;
  memcpy(new_ptr, ptr, oldsize - DSIZE);
  mm_free(ptr);
  return new_ptr;
}

/* place */
static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp)); // 현재 block 크기 정보

  remove_free_block(bp);

  if ((csize - asize) >= (2 * DSIZE))
  {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    void *next_bp = NEXT_BLKP(bp);
    PUT(HDRP(next_bp), PACK(csize - asize, 0));
    PUT(FTRP(next_bp), PACK(csize - asize, 0));
    coalesce(next_bp);
  }
  else
  {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

/* find_fit */
static void *find_fit(size_t asize)
{
  char *bp = heap_first_free_block;
  char *best_bp = NULL;
  size_t best_size = 0;

  while (bp != NULL)
  {
    size_t block_size = GET_SIZE(HDRP(bp)); /* 현재 블록 크기 */

    if (block_size < asize)
    {
      bp = (char *)GET(NEXT_PTR(bp));
      continue;
    }
    if (block_size == asize)
      return bp;

    if (block_size > asize)
    {
      if (best_bp == NULL || block_size < best_size)
      {
        best_bp = bp;
        best_size = block_size;
      }
    }
    bp = (char *)GET(NEXT_PTR(bp));
  }
  return best_bp;
}
