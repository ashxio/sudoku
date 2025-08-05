#include <raylib.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h> // For seeding rand()
#include <string.h> // For memcpy

// --- Config/DRAWING BOARD ---
#define windowWidth 900
#define windowHeight 900
#define ROWS 9
#define COLS 9
#define cellWidth (windowWidth / COLS)
#define cellHeight (windowHeight / ROWS)

// Difficulty settings 
#define EASY_CELLS_REMOVED 35
#define MEDIUM_CELLS_REMOVED 45
#define HARD_CELLS_REMOVED 55

// --- Types ---
typedef enum {
  V_None = 0, // empty cell
  V_1, V_2, V_3, V_4, V_5, V_6, V_7, V_8, V_9
} CellValue;

typedef enum {
  FIXED = 0, // OG puzzle cell (cannot be changed by user)
  DYNAMIC    // userinput cell (can be changed)
} CellType;

typedef struct {
  CellValue value;
  CellType type;
} Cell;

typedef struct {
  size_t rows, cols;
  Cell *grid; // dynamic array for the grid
} Sudoku;

// --- Game States ---
typedef enum {
  STATE_MENU,
  STATE_PLAYING,
  STATE_WIN,
  STATE_LOSE
} GameState;

// --- Globals ---
int selectedRow = -1, selectedCol = -1; // -1 = no cell selected
GameState gameState = STATE_MENU;
int selectedDifficulty = 0; // number of cells to remove
double startTime = 0;
double elapsed = 0;
int mistakeCount = 0;
int score = 0;

Sudoku current_puzzle_grid;
Sudoku solution_grid;

// --- Sudoku Functions ---

// Initializes a new Sudoku grid by allocating memory
Sudoku game_open(size_t rows, size_t cols) {
  Sudoku newSudokuGrid = {rows, cols, calloc(rows * cols, sizeof(Cell))};
  if (newSudokuGrid.grid == NULL) {
    // allocation error gracefully
    fprintf(stderr, "Error: Failed to allocate memory\n");
    exit(EXIT_FAILURE); 
  }
  return newSudokuGrid;
}

// Frees the memory/avoid leaks
void game_close(Sudoku sudoku_instance) {
  if (sudoku_instance.grid != NULL) {
    free(sudoku_instance.grid);
    sudoku_instance.grid = NULL; 
  }
}

// Fills all the grid with a specified value and sets all cell types 
void grid_fill(Sudoku sudoku_instance, CellValue val, CellType type) {
  for (size_t i = 0; i < sudoku_instance.rows * sudoku_instance.cols; i++) {
    sudoku_instance.grid[i].value = val;
    sudoku_instance.grid[i].type = type;
  }
}

// Checks if a value is valid at a given (Sudoku rules)
bool is_valid(Sudoku sudoku_instance, int row, int col, CellValue val) {
  // Check row
  for (int x = 0; x < sudoku_instance.cols; x++) {
    if (x != col && sudoku_instance.grid[row * sudoku_instance.cols + x].value == val) {
      return false;
    }
  }

  // Check column
  for (int y = 0; y < sudoku_instance.rows; y++) {
    if (y != row && sudoku_instance.grid[y * sudoku_instance.cols + col].value == val) {
      return false;
    }
  }

  // Check 3x3 box
  int boxRow = (row / 3) * 3;
  int boxCol = (col / 3) * 3;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      int i = (boxRow + r) * sudoku_instance.cols + (boxCol + c);
        if ((boxRow + r != row || boxCol + c != col) && sudoku_instance.grid[i].value == val) {
          return false;
        }
    }
  }
  return true;
}

// Sudoku solver (Generate/Fills in the complete & valid solution)
bool fill_grid(Sudoku sudoku_instance, int row, int col) {
    // Base case: If all rows are filled, the grid is complete
    if (row == sudoku_instance.rows) {
        return true;
    }

    // Calculate next cell coordinates
    int nextRow = (col == sudoku_instance.cols - 1) ? row + 1 : row;
    int nextCol = (col + 1) % sudoku_instance.cols;

    CellValue numbers[9] = {V_1, V_2, V_3, V_4, V_5, V_6, V_7, V_8, V_9};

    // Shuffle numbers to randomize puzzle
    for (int i = 0; i < 9; i++) {
        int j = rand() % 9;
        CellValue tmp = numbers[i];
        numbers[i] = numbers[j];
        numbers[j] = tmp;
    }

    // Try each shuffled number
    for (int i = 0; i < 9; i++) {
        CellValue current_val = numbers[i];
        if (is_valid(sudoku_instance, row, col, current_val)) {
            sudoku_instance.grid[row * sudoku_instance.cols + col].value = current_val;
            if (fill_grid(sudoku_instance, nextRow, nextCol)) {
                return true; 
            }
            // Backtrack: if the current number doesn't lead to a solution, reset the cell and try the next number
            sudoku_instance.grid[row * sudoku_instance.cols + col].value = V_None;
        }
    }
    return false;
}

// Recursively counts the number of solutions and stop once found
int count_solutions(Sudoku *sudoku_ptr, int limit) {
    int solutions_found = 0;
    // go through the grid to find the next empty cell
    for (int row = 0; row < sudoku_ptr->rows; row++) {
        for (int col = 0; col < sudoku_ptr->cols; col++) {
            int i = row * sudoku_ptr->cols + col;
            if (sudoku_ptr->grid[i].value == V_None) {
                // Try each possible value (V_1 to V_9)
                for (CellValue val = V_1; val <= V_9; val++) {
                    if (is_valid(*sudoku_ptr, row, col, val)) {
                        sudoku_ptr->grid[i].value = val; 
                        solutions_found += count_solutions(sudoku_ptr, limit); // Recurse

                        // Backtrack: Always reset the cell after exploring its path
                        sudoku_ptr->grid[i].value = V_None; // Reset to V_None

                        // Optimization: if enough solutions are found we stop
                        if (solutions_found >= limit) {
                            return solutions_found;
                        }
                    }
                }
                return solutions_found;
            }
        }
    }
    return 1;
}

// Checks for a unique solution
bool has_unique_solution(Sudoku *sudoku_ptr) {
    // Create a copy of the puzzle to solve so the original isn't modified
    Sudoku copy = game_open(sudoku_ptr->rows, sudoku_ptr->cols);
    memcpy(copy.grid, sudoku_ptr->grid, sudoku_ptr->rows * sudoku_ptr->cols * sizeof(Cell));

    int num_solutions = count_solutions(&copy, 2); 
    game_close(copy); 
    return num_solutions == 1; 
}

// Removes cells from a solved Sudoku grid to create a puzzle
void remove_cells(Sudoku puzzle_grid, Sudoku solution_grid_ref, int count) {
    int removed_count = 0;
    while (removed_count < count) {
        int row = rand() % puzzle_grid.rows;
        int col = rand() % puzzle_grid.cols;
        int idx = row * puzzle_grid.cols + col;

        Cell *cell = &puzzle_grid.grid[idx];
        if (cell->value == V_None) { 
            continue; 
        }

        CellValue backup_value = cell->value;
        CellType backup_type = cell->type;

        cell->value = V_None;   // Temporarily remove the value (set to V_None)
        cell->type = DYNAMIC;   // Mark as dynamic (user editable) for solution checking

        // check against the 'puzzle_grid' (which has values removed) but it still needs to be a valid and solvable state.
        if (!has_unique_solution(&puzzle_grid)) {
            // If it leads to multiple solutions or no solution, restore the cell
            cell->value = backup_value;
            cell->type = FIXED; 
        } else {
            removed_count++;
        }
    }

    // After removing cells copy the
    // 'puzzle_grid' (with removed cells) to current_puzzle_grid
    // 'solution_grid_ref' (the original solved grid to the global solution_grid.
    // This function now generates the initial puzzle and sets the solution
    for (int i = 0; i < puzzle_grid.rows * puzzle_grid.cols; i++) {
        if (puzzle_grid.grid[i].value == V_None) {
            puzzle_grid.grid[i].type = DYNAMIC; 
        } else {
            puzzle_grid.grid[i].type = FIXED; 
        }
    }
}

// Generates a Sudoku puzzle and its unique solution
void generate_new_game(int cells_to_remove) {
  // 1. Generate a complete, solved Sudoku board into a temporary grid
  Sudoku temp_solution = game_open(ROWS, COLS);
  grid_fill(temp_solution, V_None, DYNAMIC); 
  fill_grid(temp_solution, 0, 0); // fills 'temp_solution' with a completed and valid solution

  // 2. Copy the complete solution into solution_grid
  if (solution_grid.grid == NULL) {
    solution_grid = game_open(ROWS, COLS);
  }
  memcpy(solution_grid.grid, temp_solution.grid, ROWS * COLS * sizeof(Cell));

  // 3. Removing cells from a copy of the solution to create the puzzle
  if (current_puzzle_grid.grid == NULL) {
    current_puzzle_grid = game_open(ROWS, COLS);
  }
  // Start current_puzzle_grid as a copy of the full solution 
  memcpy(current_puzzle_grid.grid, temp_solution.grid, ROWS * COLS * sizeof(Cell));
  remove_cells(current_puzzle_grid, solution_grid, cells_to_remove);
  game_close(temp_solution);
}


// --- Rendering ---

// Draws tgrid lines (thin for cells, thick for 3x3)
void draw_grid_lines(Sudoku sudoku_instance) {
    int thick = 5; 
    int thin = 1; 

    // Vertical lines
    for (int x = 1; x < sudoku_instance.cols; x++) {
        int t = (x % 3 == 0) ? thick : thin;
        DrawLineEx((Vector2){(float)x * cellWidth, 0.0f}, (Vector2){(float)x * cellWidth, (float)windowHeight}, (float)t, BLACK);
    }
    // Horizontal lines
    for (int y = 1; y < sudoku_instance.rows; y++) {
        int t = (y % 3 == 0) ? thick : thin;
        DrawLineEx((Vector2){0.0f, (float)y * cellHeight}, (Vector2){(float)windowWidth, (float)y * cellHeight}, (float)t, BLACK);
    }
}

// Draw the numbers in the grid cells
void draw_values(Sudoku sudoku_instance) {
    for (int row = 0; row < sudoku_instance.rows; row++) {
        for (int col = 0; col < sudoku_instance.cols; col++) {
            Cell cell = sudoku_instance.grid[row * sudoku_instance.cols + col];
            if (cell.value != V_None) { // Check for non-V_None
                char text[2]; // Buffer
                sprintf(text, "%d", (int)cell.value);
                Color color = (cell.type == FIXED) ? BLACK : BLUE; // fixed = black, userinput/dynamic = blue

                int fontSize = 30;
                // Measure text size 
                Vector2 textSize = MeasureTextEx(GetFontDefault(), text, (float)fontSize, 0); 

                // Calculate position to center the text within the cell
                float x = (float)col * cellWidth + (cellWidth - textSize.x) / 2.0f;
                float y = (float)row * cellHeight + (cellHeight - textSize.y) / 2.0f;

                DrawText(text, (int)x, (int)y, fontSize, color);
            }
        }
    }
}

// draw selection rectangle around the selected cell
void draw_selection() {
    if (selectedRow >= 0 && selectedCol >= 0) {
        DrawRectangleLines(
            selectedCol * cellWidth,
            selectedRow * cellHeight,
            cellWidth,
            cellHeight,
            GREEN
        );
    }
}

// menu screen
void draw_menu() {
    int screenCenterX = windowWidth / 2;
    int currentY = 200;
    int lineHeight = 50;

    const char* title = "Select Difficulty:";
    int titleFontSize = 40;
    float titleTextWidth = MeasureText(title, titleFontSize);
    DrawText(title, screenCenterX - (int)(titleTextWidth / 2), currentY, titleFontSize, DARKBLUE);
    currentY += lineHeight + 50;

    char difficultyText[64];
    int optionFontSize = 30;

    sprintf(difficultyText, "1. Easy (%d cells removed)", EASY_CELLS_REMOVED);
    float easyTextWidth = MeasureText(difficultyText, optionFontSize);
    DrawText(difficultyText, screenCenterX - (int)(easyTextWidth / 2), currentY, optionFontSize, BLACK);
    currentY += lineHeight;

    sprintf(difficultyText, "2. Medium (%d cells removed)", MEDIUM_CELLS_REMOVED);
    float mediumTextWidth = MeasureText(difficultyText, optionFontSize);
    DrawText(difficultyText, screenCenterX - (int)(mediumTextWidth / 2), currentY, optionFontSize, BLACK);
    currentY += lineHeight;

    sprintf(difficultyText, "3. Hard (%d cells removed)", HARD_CELLS_REMOVED);
    float hardTextWidth = MeasureText(difficultyText, optionFontSize);
    DrawText(difficultyText, screenCenterX - (int)(hardTextWidth / 2), currentY, optionFontSize, BLACK);
    currentY += lineHeight + 20;

    const char* hint = "Press number key to start";
    int hintFontSize = 20;
    float hintTextWidth = MeasureText(hint, hintFontSize);
    DrawText(hint, screenCenterX - (int)(hintTextWidth / 2), currentY, hintFontSize, GRAY);
}

// --- Main Game Loop ---
int main() {
  // raylib window
  InitWindow(windowWidth, windowHeight, "Sudoku with ty and denn");
  SetTargetFPS(60); 
  srand((unsigned int)time(0)); 

  // sudoku
  current_puzzle_grid.grid = NULL;
  solution_grid.grid = NULL;

    // game loop
    while (!WindowShouldClose()) { 
        BeginDrawing(); 
        ClearBackground(RAYWHITE); 

        if (gameState == STATE_MENU) {
            draw_menu();

            // difficulty selection
            if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_THREE)) {
                if (IsKeyPressed(KEY_ONE)) {
                    selectedDifficulty = EASY_CELLS_REMOVED;
                } else if (IsKeyPressed(KEY_TWO)) {
                    selectedDifficulty = MEDIUM_CELLS_REMOVED;
                } else {
                    selectedDifficulty = HARD_CELLS_REMOVED;
                }

                gameState = STATE_PLAYING;
                generate_new_game(selectedDifficulty); // generate puzzle and solution
                mistakeCount = 0;
                selectedRow = -1; 
                selectedCol = -1;
                startTime = GetTime(); 
                elapsed = 0;
                score = 0; 
                EndDrawing(); 
                continue; 
            }

        } else if (gameState == STATE_PLAYING) {
            elapsed = GetTime() - startTime; 

            if (mistakeCount < 3) { 
                // mouse input for cell selection
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mouse = GetMousePosition();
                    if (mouse.x >= 0 && mouse.x < windowWidth && mouse.y >= 0 && mouse.y < windowHeight) {
                        selectedCol = (int)mouse.x / cellWidth;
                        selectedRow = (int)mouse.y / cellHeight;
                    } else {
                        selectedRow = -1; 
                        selectedCol = -1;
                    }
                }

                // keyboard input for entering numbers
                for (int key = KEY_ONE; key <= KEY_NINE; key++) {
                    if (IsKeyPressed(key) && selectedRow >= 0 && selectedCol >= 0) {
                        Cell *current_cell = &current_puzzle_grid.grid[selectedRow * COLS + selectedCol];
                        // Get the value from the solution grid for this cell
                        CellValue correct_value = solution_grid.grid[selectedRow * COLS + selectedCol].value;

                        if (current_cell->type == DYNAMIC) {
                            CellValue guess = (CellValue)(key - KEY_ZERO);

                            // Check against the stored solution
                            if (guess == correct_value) {
                                current_cell->value = guess; 
                                bool all_filled = true;
                                for (int i = 0; i < ROWS * COLS; i++) {
                                    if (current_puzzle_grid.grid[i].type == DYNAMIC && current_puzzle_grid.grid[i].value == V_None) {
                                        all_filled = false;
                                        break;
                                    }
                                }
                                if (all_filled) {
                                    gameState = STATE_WIN;
                                    score = 1000 - (int)elapsed - (mistakeCount * 100); 
                                    if (score < 0) {
                                      score = 0;
                                    }
                                }
                            } else {
                                mistakeCount++;
                                if (mistakeCount >= 3) {
                                    gameState = STATE_LOSE;
                                }
                            }
                        }
                    }
                }

                // backspace/delete to clear userinput
                if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE)) && selectedRow >= 0 && selectedCol >= 0) {
                    Cell *current_cell = &current_puzzle_grid.grid[selectedRow * COLS + selectedCol];
                    if (current_cell->type == DYNAMIC) {
                        current_cell->value = V_None; 
                    }
                }

            }

            // Always draw grid and values in playing state
            draw_grid_lines(current_puzzle_grid);
            draw_values(current_puzzle_grid);
            draw_selection();

            // --- Game Information Display ---
            int topInfoYOffset = 10;
            int topInfoFontSize = 20;
            int screenMiddleX = windowWidth / 2;

            // Display Mistakes (top left)
            char info[64];
            sprintf(info, "Mistakes: %d / 3", mistakeCount);
            DrawText(info, 10, topInfoYOffset, topInfoFontSize, RED);

            // Display Time (top center)
            char timeText[64];
            sprintf(timeText, "Time: %.0f sec", elapsed);
            float timeTextWidth = MeasureText(timeText, topInfoFontSize);
            DrawText(timeText, screenMiddleX - (int)(timeTextWidth / 2), topInfoYOffset, topInfoFontSize, DARKGRAY);

            // Display Score (top right)
            char currentScoreText[64];
            sprintf(currentScoreText, "Score: %d", score);
            float scoreTextWidth = MeasureText(currentScoreText, topInfoFontSize);
            DrawText(currentScoreText, windowWidth - (int)scoreTextWidth - 10, topInfoYOffset, topInfoFontSize, DARKGREEN);


        } else if (gameState == STATE_WIN) {
            int screenCenterX = windowWidth / 2;
            int currentY = 300;
            int lineHeight = 40;

            const char* winMsg = "You Win!";
            float winMsgWidth = MeasureText(winMsg, 40);
            DrawText(winMsg, screenCenterX - (int)(winMsgWidth / 2), currentY, 40, DARKGREEN);
            currentY += lineHeight + 20;

            char finalTime[64];
            sprintf(finalTime, "Time: %.0f seconds", elapsed);
            float finalTimeWidth = MeasureText(finalTime, 30);
            DrawText(finalTime, screenCenterX - (int)(finalTimeWidth / 2), currentY, 30, BLACK);
            currentY += lineHeight;

            char scoreText[64];
            sprintf(scoreText, "Final Score: %d", score);
            float scoreTextWidth = MeasureText(scoreText, 30);
            DrawText(scoreText, screenCenterX - (int)(scoreTextWidth / 2), currentY, 30, BLACK);
            currentY += lineHeight + 20;

            const char* restartHint = "Press R to Restart or ESC to Exit";
            float restartHintWidth = MeasureText(restartHint, 20);
            DrawText(restartHint, screenCenterX - (int)(restartHintWidth / 2), currentY, 20, GRAY);

            if (IsKeyPressed(KEY_R)) {
                gameState = STATE_MENU;
            }
        } else if (gameState == STATE_LOSE) {
            int screenCenterX = windowWidth / 2;
            int currentY = 300;
            int lineHeight = 40;

            const char* loseMsg = "You Lose!";
            float loseMsgWidth = MeasureText(loseMsg, 40);
            DrawText(loseMsg, screenCenterX - (int)(loseMsgWidth / 2), currentY, 40, MAROON);
            currentY += lineHeight + 20;

            char timeText[64];
            sprintf(timeText, "Time: %.0f seconds", elapsed);
            float timeTextWidth = MeasureText(timeText, 30);
            DrawText(timeText, screenCenterX - (int)(timeTextWidth / 2), currentY, 30, BLACK);
            currentY += lineHeight;

            char scoreText[64];
            sprintf(scoreText, "Final Score: %d", score);
            float scoreTextWidth = MeasureText(scoreText, 30);
            DrawText(scoreText, screenCenterX - (int)(scoreTextWidth / 2), currentY, 30, BLACK);
            currentY += lineHeight + 20;

            const char* restartHint = "Press R to Restart or ESC to Exit";
            float restartHintWidth = MeasureText(restartHint, 20);
            DrawText(restartHint, screenCenterX - (int)(restartHintWidth / 2), currentY, 20, GRAY);

            if (IsKeyPressed(KEY_R)) {
                gameState = STATE_MENU; 
            }
        }

        EndDrawing(); 
    }

  game_close(current_puzzle_grid);
  game_close(solution_grid);     
  CloseWindow(); 
  return 0; 
}