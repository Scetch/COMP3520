#include <cstdio>
#include <algorithm>
#include <iostream>

#include <SDL.h>
#include <SDL_ttf.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

int menu(void* ptr);
void menu_points(uint32_t pixels[][SCREEN_WIDTH]);
void menu_line(uint32_t pixels[][SCREEN_WIDTH]);
void menu_circle(uint32_t pixels[][SCREEN_WIDTH]);

void clear(uint32_t pixels[][SCREEN_WIDTH]);
void rotate(int p[2], float angle);
void draw_line(uint32_t pixels[][SCREEN_WIDTH], int x1, int y1, int x2, int y2, int color);
void draw_ellipse(uint32_t pixels[][SCREEN_WIDTH], int x, int y, int width, int height, int color);

// Will handle the stdin in another thread, if we don't the window will not 
// update on Arch Linux. We will use a mutex to guard against reads/writes of 
// the running and dirty flag.
SDL_Thread* input_thread = NULL;
SDL_mutex* mutex = NULL;
bool dirty = false;
bool running = true;

int main(int argc, char* args[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not initialize sdl2: %s\n", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "COMP3520",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not create window %s\n", SDL_GetError());
        return 1;
    }

    // Create a renderer to paint to
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (renderer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    // Create a canvas that can be painted on
    SDL_Surface* canvas = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, SDL_PIXELFORMAT_RGBA8888);
    if (canvas == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not create surface: %s\n", SDL_GetError());
        return 1;
    }

    // Create a texture that can be rendered on the GPU
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);
    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not create texture: %s\n", SDL_GetError());
        return 1;
    }

    //
    // We will start input in a second thread so it does not interfere with rendering.
    input_thread = SDL_CreateThread(menu, "MenuThread", canvas);
    if (input_thread == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not create input thread: %s\n", SDL_GetError());
        return 1;
    }

    mutex = SDL_CreateMutex();
    if (mutex == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "could not create mutex: %s\n", SDL_GetError());
        return 1;
    }
    //

    // Handle events or our window will not respond.
    SDL_Event event;
    while(true) {
        // Poll for window events
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_QUIT:
                    // Normally we would handle the exit but this is messy when using the terminal input
                    // is_running = false;
                    break;
            }
        }

        // We will attempt to lock the mutex, if we can do this we will first check if the user
        // has requested that we close our application. If so we'll break out of the main loop.
        // If not we will check if they have updated the image data and if we we'll send that updated
        // data to the texture to be rendered.
        if (SDL_TryLockMutex(mutex) == 0) {
            if (!running) {
                break;
            }

            if (dirty) {
                // Render our drawing to the texture
                SDL_UpdateTexture(texture, NULL, canvas->pixels, canvas->pitch);
                dirty = false;
            }

            SDL_UnlockMutex(mutex);
        }

        // Render the image.
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    // Wait for the input thread to stop.
    int ret;
    SDL_WaitThread(input_thread, &ret);

    // Cleanup
    SDL_DestroyMutex(mutex);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(canvas);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window); 

    SDL_Quit();

    return 0;
}

int menu(void* ptr) {
    SDL_Surface* canvas = (SDL_Surface*) ptr;
    uint32_t (*pixels)[SCREEN_WIDTH] = (uint32_t(*)[SCREEN_WIDTH]) canvas->pixels;

    int option;

    while(true) {
        printf("Menu\n 1) End Program\n 2) Draw Points\n 3) Draw Line\n 4) Draw Circle\n");
        scanf("%d", &option);

        switch (option) {
            case 1:
                // To end the program we will tell the main thread that we're done 
                SDL_LockMutex(mutex);
                running = false;
                SDL_UnlockMutex(mutex);
                return 0;
            case 2:
                menu_points(pixels);
                break;
            case 3:
                menu_line(pixels);
                break;
            case 4:
                menu_circle(pixels);
                break;
            default:
                printf("Invalid menu option. Please specify an actual menu item.\n");
                break;
        }
    }

    return 0;
}

void menu_points(uint32_t pixels[][SCREEN_WIDTH]) {
    int num_points = 0;
    while(num_points < 1 || num_points > 5) {
        printf("Specify number of points (1-5) > ");
        scanf("%d", &num_points);
    }

    int points[5][3];
    for(int i = 0; i < num_points; i++) {
        printf("Point %d (x y color) > ", i + 1);
        scanf("%d %d %x", &points[i][0], &points[i][1], &points[i][2]);
    }

    // Draw, making sure to lock and unlock the mutex
    SDL_LockMutex(mutex);

    // Clear the screen
    clear(pixels);
    
    // Draw the points
    for(int i = 0; i < num_points; i++) {
        int* p = &points[i][0];
        // Make sure the point is on the screen before drawing it
        if((p[1] >= 0 && p[1] < SCREEN_HEIGHT) && (p[0] >= 0 && p[0] < SCREEN_WIDTH)) {
            pixels[p[1]][p[0]] = p[2];
        }
    }

    // Tell the main thread that we have changed the canvas
    dirty = true;

    SDL_UnlockMutex(mutex);
}

void menu_line(uint32_t pixels[][SCREEN_WIDTH]) {
    int a[2], b[2], color;
    printf("Specify line (x1 y1 x2 y2 color) > ");
    scanf("%d %d %d %d %x", &a[0], &a[1], &b[0], &b[1], &color);

    int trans_x, trans_y;
    printf("Specify a translation (trans_x trans_y) > ");
    scanf("%d %d", &trans_x, &trans_y);

    int deg;
    printf("Specify an angle in degrees (angle) > ");
    scanf("%d", &deg);

    // Convert the degree angle to radius
    float angle = (float) deg * (M_PI / 180);

    printf("angle in radians %f\n", angle);

    // Draw, making sure to lock and unlock the mutex
    SDL_LockMutex(mutex);

    //
    // Clear the screen
    //
    clear(pixels);

    //
    // Draw the main line segment
    //
    draw_line(
        pixels,
        a[0],
        a[1],
        b[0],
        b[1],
        color
    );

    //
    // Draw the translated line segment
    //
    draw_line(
        pixels,
        a[0] + trans_x,
        a[1] + trans_y,
        b[0] + trans_x,
        b[1] + trans_y,
        color
    );

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

    // Tell the main thread that we have changed the canvas
    dirty = true;

    SDL_UnlockMutex(mutex);
}

void menu_circle(uint32_t pixels[][SCREEN_WIDTH]) {
    int x, y, radius, color;
    printf("Specify circle (x y radius color) > ");
    scanf("%d %d %d %x", &x, &y, &radius, &color);

    int trans_x, trans_y;
    printf("Specify a translation (trans_x trans_y) > ");
    scanf("%d %d", &trans_x, &trans_y);

    int scale_x, scale_y;
    printf("Specify a scale (scale_x scale_y) > ");
    scanf("%d %d", &scale_x, &scale_y);
    
    // Draw, making sure to lock and unlock the mutex
    SDL_LockMutex(mutex);

    // Clear the screen
    clear(pixels);

    // Draw the 3 circles with the different properties
    draw_ellipse(pixels, x, y, radius, radius, color);
    draw_ellipse(pixels, x + trans_x, y + trans_y, radius, radius, color);
    draw_ellipse(pixels, x, y, radius + scale_x, radius + scale_y, color);

    // Tell the main thread that we have changed the canvas
    dirty = true;

    SDL_UnlockMutex(mutex);
}

// Helper function to rotate a point by an angle
void rotate(int p[2], float angle) {
    int rot_x = (int) (cos(angle) * p[0] - sin(angle) * p[1]);
    int rot_y = (int) (sin(angle) * p[0] + cos(angle) * p[1]);
    p[0] = rot_x;
    p[1] = rot_y;
}

// Loop through all the pixels on the screen and set them to black
void clear(uint32_t pixels[][SCREEN_WIDTH]) {
    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            pixels[y][x] = 0x00000000;
        }
    }
}

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