# Tetris Game in C with CRUD 

## Overview
This project is a complete implementation of the classic Tetris game using the C programming language and the Raylib graphics library.  
It demonstrates real-time game mechanics, structured program design, GUI interaction, and persistent data storage.  
The project uses pointers, dynamic memory allocation, structs, linked lists, and text-based file I/O, meeting all required course concepts.

## Features

### Gameplay
- Falling Tetris pieces with correct movement, rotation, collision checking, and locking.
- Gravity-based descent, soft drop, hard drop, and pause functionality.
- Difficulty scaling based on score and elapsed time.
- Line detection and clearing with an animated flash effect.
- Sidebar HUD showing score, level, gravity speed, playtime, and next shape preview.

### Player Profiles (CRUD System)
The program supports full CRUD operations on player profiles with persistent storage in `playerdb.txt`.

Supported operations:
- Create a new profile  
- Read existing profiles and display details  
- Update profile names  
- Delete one profile or delete all profiles  

Each player profile stores:
- Name  
- Times played  
- Best score  
- Total playtime (in seconds)

### High Score Management
- High scores are stored in `scores.txt`.
- Only the best score per player name is kept.
- Scores are automatically sorted in descending order.
- Dedicated GUI screen for viewing scores.

### Interface and Rendering
- GUI-based menu system: main menu, profile manager, score screen.
- Themed buttons with hover detection.
- Input dialogs for entering player names.
- Scrollable and sortable profile list.
- Clean layout for the grid, sidebar, preview box, and controls panel.

## Concepts and Techniques Used
- Pointers and pointer arithmetic  
- Dynamic memory allocation (malloc, calloc, free)  
- Structs for shapes, profiles, players, nodes, and GUI elements  
- Linked lists (next-piece queue and profile database)  
- File I/O using text-based persistence  
- Modular architecture across multiple C source files  
- Real-time rendering, timing, and input handling with Raylib  

## Project Structure

- **tetris.h**  
  Shared declarations, structs, constants, extern variables

- **main.c**  
  Entry point, window setup, main menu, profile selection

- **game_logic.c**  
  Core Tetris logic: spawning, movement, rotation, scoring

- **board.c**  
  Rendering of grid, preview, HUD, buttons, dialogs

- **file_manager.c**  
  Profile CRUD operations, high score management, persistence

- **playerdb.txt**  
  Auto-generated profile database

- **scores.txt**  
  Auto-generated high score file


## Building the Project
Example (gcc):
- gcc main.c game_logic.c board.c file_manager.c -lraylib -lm -o tetris
- Raylib must be installed on the system and properly linked.

## Running the Program

## Controls
- A / Left Arrow: Move left  
- D / Right Arrow: Move right  
- W / Up Arrow: Rotate  
- S / Down Arrow: Soft drop  
- Space: Hard drop  
- P: Pause  
- Q: Quit  
