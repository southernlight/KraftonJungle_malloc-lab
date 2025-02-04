/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4             /* Word and header/footer size (bytes) */
#define DSIZE 8             /* Double word size (bytes) */
#define CHUNKSIZE (1 << 12) /*Extend heap bt this amount (bytes)*/
#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
// 메모리 블록의 헤더에서 마지막 3비트를 제거-> 크기
#define GET_SIZE(p) (GET(p) & ~0x7)
// 메모리 블록의 헤더에서 마지막 1비트만 추출-> alloc
#define GET_ALLOC(p) (GET(p) & 0x1)

/*Given block ptr bp, compute address of its header and footer*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/*Given block ptr bp, compute address of next and previous blocks*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

static void *coalesce(void *bp) {

  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  /*Case 1*/
  if (prev_alloc && next_alloc) {
    return bp;
    /*Case 2*/
  } else if (prev_alloc && !next_alloc) {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    /*Case 3*/
  } else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);

    /*Case 4*/
  } else {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  return bp;
}

static void *extend_heap(size_t words) {
  char *bp;
  size_t size;

  /*Allocate an even number of words to maintain alignment*/
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
    return NULL;

  /*Initialize free block header/footer and the epilogue header*/
  PUT(HDRP(bp), PACK(size, 0));         /*Free block header*/
  PUT(FTRP(bp), PACK(size, 0));         /*Free block footer*/
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /*New epliogue header*/

  /*Coalesce if the previous block was free*/
  return coalesce(bp);
}
static void *heap_listp;

/* mm_init - initialize the malloc package.*/
int mm_init(void) {
  /* Create the initial empty heap */
  if (((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1))
    return -1;

  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
  heap_listp += (2 * WSIZE);

  // (CHUNKSIZE / WSIZE) 몇 개의 word를 extend 할지
  if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    return -1;
  return 0;
}

void mm_free(void *bp) {
  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */

// First fit
static void *find_fit(size_t asize) {
  void *bp = mem_heap_lo() + 2 * DSIZE;
  while (GET_SIZE(HDRP(bp)) > 0) {
    if (!GET_ALLOC(HDRP(bp)) && GET_SIZE((HDRP(bp))) >= asize)
      return bp;
    bp = NEXT_BLKP(bp);
  }

  return NULL;
}

static void place(void *bp, size_t asize) {
  size_t csize = GET_SIZE(HDRP(bp));

  if (csize - asize >= 2 * DSIZE) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));

    //남은 크기를 다음 블록에 할당
    PUT(HDRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(csize - asize, 0));

  } else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

void *mm_malloc(size_t size) {
  size_t asize; // 조정된 블록 사이즈
  size_t extendsize;
  char *bp;

  if (size == 0)
    return NULL;

  if (size <= DSIZE) // 8바이트 이하이면
    // 최소 블록 크기 16바이트 할당(헤더 4 + 푸터 4 + 저장공간 8)
    asize = 2 * DSIZE;
  else
    // 8 배수로 올림 처리
    asize = DSIZE * ((size + DSIZE + DSIZE - 1) / DSIZE);

  /*Search the free list for a fit*/
  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  /*No fit found. Get more memory and place the block*/
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize)) == NULL)
    return NULL;
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
  void *oldptr = ptr;
  void *newptr;
  size_t copySize;

  newptr = mm_malloc(size);
  if (newptr == NULL)
    return NULL;
  // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
  //-> SIZE_T_SIZE 는 8(64bit system) 이다. 원래 헤드를 가리킬라면, bp에서 4만큼
  //빼주어야하는데, 여기서는 8만큼 빼주고 있다. 따라서 잘못된 참조가 발생한다.
  copySize = GET_SIZE(HDRP(oldptr));
  if (size < copySize)
    copySize = size;
  memcpy(newptr, oldptr, copySize);
  mm_free(oldptr);
  return newptr;
}
