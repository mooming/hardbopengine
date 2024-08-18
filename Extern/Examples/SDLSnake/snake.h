/*
 * Interface definition of the Snake game.
 *
 * This code is public domain. Feel free to use it for any purpose!
 */
#ifndef SNAKE_H
#define SNAKE_H

#include <SDL3/SDL.h>
#include <cstdint>

#define SNAKE_GAME_WIDTH  1024U
#define SNAKE_GAME_HEIGHT 1024U
#define SNAKE_MATRIX_SIZE (SNAKE_GAME_WIDTH * SNAKE_GAME_HEIGHT)

enum class SnakeCell : uint8_t
{
    SNAKE_CELL_NOTHING = 0U,
    SNAKE_CELL_SRIGHT = 1U,
    SNAKE_CELL_SUP = 2U,
    SNAKE_CELL_SLEFT = 3U,
    SNAKE_CELL_SDOWN = 4U,
    SNAKE_CELL_FOOD = 5U
};

#define SNAKE_CELL_MAX_BITS 3U /* floor(log2(SNAKE_CELL_FOOD)) + 1 */

enum class SnakeDirection : uint8_t
{
    SNAKE_DIR_RIGHT,
    SNAKE_DIR_UP,
    SNAKE_DIR_LEFT,
    SNAKE_DIR_DOWN
};

struct SnakeContext final
{
    SnakeCell cells[(SNAKE_MATRIX_SIZE * SNAKE_CELL_MAX_BITS) / 8U];

	int32_t head_xpos;
    int32_t head_ypos;
    int32_t tail_xpos;
    int32_t tail_ypos;
    
	SnakeDirection next_dir;
    
	int32_t inhibit_tail_step;
    uint32_t occupied_cells;
};

typedef Sint32 (SDLCALL *RandFunc)(Sint32 n);

void snake_initialize(SnakeContext *ctx, RandFunc rand);
void snake_redir(SnakeContext *ctx, SnakeDirection dir);
void snake_step(SnakeContext *ctx, RandFunc rand);
SnakeCell snake_cell_at(const SnakeContext *ctx, int32_t x, int32_t y);

#endif /* SNAKE_H */
