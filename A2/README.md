# Assignment 2

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
__Note:__ Colors are input as hex, for example: `FF0000FF`.

On program load the terminal displays a menu to the user:

```
Menu
 1) End Program
 2) Draw Points
 3) Draw Line
 4) Draw Circle
```

After each menu option the canvas is cleared before drawing the next option.

### End Program
The input thread locks the mutex and sets the `running` flag to `false`.
This will cause the main thread to break from the render loop and wait for the `input_thread` to exit.

### Draw Points
This option will allow a user to enter 1-5 points.

Each point has an `x` position, `y` position, and a `color`.

```
Specify number of points (1-5) > 2
Point 1 (x y color) > 10 10 FF0000FF
Point 2 (x y color) > 50 50 FF0000FF
```

### Draw Line
This option will draw 3 lines:
* A line specified by the user with two points, (`x1`, `y1`) and (`x2`, `y2`), and `color`
* The original line translated by `trans_x` and `trans_y`
* The original line rotated by `angle`

```
Specify line (x1 y1 x2 y2 color) > 0 0 50 50 FF0000FF
Specify a translation (trans_x trans_y) > 20 20
Specify an degree for ratation (angle) > 180
```

### Draw Circle
This option will draw 3 circles:
* A circle specified by a user with an `x` position, `y` position, `radius`, and `color`
* The original circle translated by `trans_x` and `trans_y`
* The original circle scaled by `scale_x` and `scale_y`

```
Specify circle (x y radius color) > 100 100 10 FF0000FF
Specify a translation (trans_x trans_y) > 40 40
Specify a scale (scale_x scale_y) > 10 0
```

## Code

Several helper functions have been written:

```c
// Helper function to rotate a point by an angle
void rotate(int p[2], float angle) {
    int rot_x = (int) (cos(angle) * p[0] - sin(angle) * p[1]);
    int rot_y = (int) (sin(angle) * p[0] + cos(angle) * p[1]);
    p[0] = rot_x;
    p[1] = rot_y;
}
```

```c
// Loop through all the pixels on the screen and set them to black
void clear(uint32_t pixels[][SCREEN_WIDTH]) {
    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            pixels[y][x] = 0x00000000;
        }
    }
}
```

```c
// Helper function to draw an ellipse
void draw_ellipse(uint32_t pixels[][SCREEN_WIDTH], int x, int y, int width, int height, int color) {
    for(int j = -height; j <= height; j++) {
        for(int i = -width; i <= width; i++) {
            // We check that this pixel is inside of the ellipse
            if(i * i * height * height + j * j * width * width <= height * height * width * width) {
                // Check to make sure we don't write off of the screen.
                if((y + j >= 0 && y + j < SCREEN_HEIGHT) && (x + i >= 0 && x + i < SCREEN_WIDTH)) {
                    pixels[y + j][x + i] = color;
                }
            }
        }
    }
}
```

```c
// Helper function to draw a simple line segment
void draw_line(uint32_t pixels[][SCREEN_WIDTH], int x1, int y1, int x2, int y2, int color) {
    if(x2 < x1) {
        std::swap(x2, x1);
        std::swap(y2, y1);
    }

    // 90 degree line, slope of 0
    if(x1 == x2) {
        if(y1 > y2)
            std::swap(y1, y2);

        for(int y = y1; y < y2; y++) {
            pixels[y][x1] = color;
        }
    } else {
        int m = (y2 - y1) / (x2 - x1);
        int c = y2 - m * x2;

        for(int x = x1; x <= x2; x++) {
            int y = m * x + c;
            // Make sure the point is on the screen before drawing it
            if((y >= 0 && y < SCREEN_HEIGHT) && (x >= 0 && x < SCREEN_WIDTH)) {
                pixels[y][x] = color;
            }
        }
    }
}
```

With these functions it is simple to implement each menu option:

### Draw Point
Drawing points is simple, we just have to make sure the points that the user gave as input are actually on the screen.

```c
// Draw the points
for(int i = 0; i < num_points; i++) {
    int* p = &points[i][0];
    // Make sure the point is on the screen before drawing it
    if((p[1] >= 0 && p[1] < SCREEN_HEIGHT) && (p[0] >= 0 && p[0] < SCREEN_WIDTH)) {
        pixels[p[1]][p[0]] = p[2];
    }
}
```

### Draw Line

Drawing the normal line and the translated line are simple enough but drawing the rotated line is  a bit more complex. 
* Find the midpoint of the line
* Translate the points to that midpoint
* Rotate the points
* Translate the points by the midpoint

```c
//
// Draw the rotated line segment
//
// Find the midpoint
int mid[2] = { (a[0] + b[0]) / 2, (a[1] + b[1]) / 2 };
// Set the positions of a and b at the midpoint
int a_rot[2] = { a[0] - mid[0],	a[1] - mid[1] };
int b_rot[2] = { b[0] - mid[0],	b[1] - mid[1] };
// Rotate about the midpoint
rotate(a_rot, angle);
rotate(b_rot, angle);
// Draw the line, adding the midpoints back to the points
draw_line(
    pixels,
    a_rot[0] + mid[0],
    a_rot[1] + mid[1],
    b_rot[0] + mid[0],
    b_rot[1] + mid[1],
    color
);
```

### Draw Circle

With the help of our helper function `draw_ellipse`, drawing the three circles is extremely simple.

```c
// Draw the 3 circles with the different properties
draw_ellipse(pixels, x, y, radius, radius, color);
draw_ellipse(pixels, x + trans_x, y + trans_y, radius, radius, color);
draw_ellipse(pixels, x, y, radius + scale_x, radius + scale_y, color);
```