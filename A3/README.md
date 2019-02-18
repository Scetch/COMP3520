# Assignment 3

## Changes to CMakeLists
On Arch Linux I had to change
```cmake
target_link_libraries(main
PUBLIC
${SDL2_LIBRARIES}
${SDL2_TTF_LIBRARY}
)
```
to
```cmake
target_link_libraries(main
PUBLIC
SDL2::SDL2
${SDL2_TTF_LIBRARY}
)
```
in order for the the program to link with SDL2.

## Input Thread Handling

Stopping to request input causes the window to hang and not update, as a result the window does not get painted. To overcome this problem I created a seperate `SDL_thread` with `SDL_CreateThread` for reading input from the terminal allowing the window to continously recieve events and paint the window. The input thread will update a dirty flag to tell the render thread that we have written the data and the texture can be updated. To prevent reading and writing at the same time we use an `SDL_mutex` and locking. 

## Interaction

On program load the terminal displays a menu to the user:

```
Menu
 1) End Program
 2) Clip
 3) Fill
```

After each menu option the canvas is cleared before drawing the next option.

### End Program
The input thread locks the mutex and sets the `running` flag to `false`.
This will cause the main thread to break from the render loop and wait for the `input_thread` to exit.

### Clip
This allows the user to input a set of `vertices`, two `start points`, and a `clipping algorithm`.

```
Number of verticies ( > 2 ) > 3
Points in clockwise order:
Enter point (x y) > 40 0
Enter point (x y) > 80 80
Enter point (x y) > 0 80
Enter a starting point (x y) > -10 -10
Enter a second starting point (x y) > 100 100
Clipping algorithm:
1) Sutherland-Hodgman
2) Liang-Barsky
1
```

### Fill
This allows the user to input a set of `vertices`, and a `point` inside the polygon for floodfill.

```
Number of verticies ( > 2 ) > 4
Points in clockwise order:
Enter point (x y) > 0 0
Enter point (x y) > 100 0
Enter point (x y) > 100 100
Enter point (x y) > 0 100
Enter a point inside of the polygon (x y) > 20 20
Draw scanline algorithm? (y) > y
```