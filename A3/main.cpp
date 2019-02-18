#include <cstdio>
#include <algorithm>
#include <iostream>
#include <vector>

#include <unistd.h>

#include <SDL.h>
#include <SDL_ttf.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

struct Point {
    int x;
    int y;
};

int menu(void* ptr);

void menu_clip(uint32_t pixels[][SCREEN_WIDTH]);
void menu_fill(uint32_t pixels[][SCREEN_WIDTH]);

std::vector<Point> menu_polygon();

void sutherland_hodgman(std::vector<Point>& verts, const std::vector<Point>& clipper);
void liang_barksy(std::vector<Point>& verts);

void draw_floodfill(uint32_t pixels[][SCREEN_WIDTH], int x, int y, uint32_t color);
void draw_scanline(uint32_t pixels[][SCREEN_WIDTH], const std::vector<Point>& verts, uint32_t color);

void plot_point(uint32_t pixels[][SCREEN_WIDTH], int x, int y, uint32_t color);
void draw_line(uint32_t pixels[][SCREEN_WIDTH], Point p0, Point p1, uint32_t color);
void draw_polygon(uint32_t pixels[][SCREEN_WIDTH], const std::vector<Point>& verts, uint32_t color);
void clear(uint32_t pixels[][SCREEN_WIDTH]);

std::vector<Point> translate_polygon(const std::vector<Point>& verts, Point p);

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
        printf("Menu\n 1) End Program\n 2) Clip\n 3) Fill\n");
        scanf("%d", &option);

        switch (option) {
            case 1:
                // To end the program we will tell the main thread that we're done 
                SDL_LockMutex(mutex);
                running = false;
                SDL_UnlockMutex(mutex);
                return 0;
            case 2:
                menu_clip(pixels);
                break;
            case 3:
                menu_fill(pixels);
                break;
            default:
                printf("Invalid menu option. Please specify an actual menu item.\n");
                break;
        }
    }

    return 0;
}

void menu_clip(uint32_t pixels[][SCREEN_WIDTH]) {
    const std::vector<Point> clipper {
        Point { 0, 0 },
        Point { 0, SCREEN_HEIGHT - 1 },
        Point { SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 },
        Point { SCREEN_WIDTH - 1, 0 }
    };

    // Get input from the user
    std::vector<Point> verts = menu_polygon();

    int start_x0, start_y0;
    printf("Enter a starting point (x y) > ");
    scanf("%d %d", &start_x0, &start_y0);

    int start_x1, start_y1;
    printf("Enter a second starting point (x y) > ");
    scanf("%d %d", &start_x1, &start_y1);

    int option;
    while(true) {
        printf("Clipping algorithm:\n1) Sutherland-Hodgman\n2) Liang-Barsky\n");
        scanf("%d", &option);

        if(option == 1 || option == 2) {
            break;
        }

        printf("Invalid option.\n");
    }

    std::vector<Point> first_poly = translate_polygon(verts, Point { start_x0, start_y0 });
    std::vector<Point> second_poly = translate_polygon(verts, Point { start_y1, start_y1 });

    // Draw, remembering to lock the mutex
    SDL_LockMutex(mutex);
    
    // Clear the screen
    clear(pixels);

    if(option == 1) {
        // Sutherlang-Hodgman
        sutherland_hodgman(first_poly, clipper);
        draw_polygon(pixels, first_poly, 0xFF000000);

        sutherland_hodgman(second_poly, clipper);
        draw_polygon(pixels, second_poly, 0x00FF0000);
    } else if(option == 2) {
        // Liang-Barsky

        // TODO
    }

    // Tell the main thread we have changed the texture
    dirty = true;

    SDL_UnlockMutex(mutex);
}

void menu_fill(uint32_t pixels[][SCREEN_WIDTH]) {
    const std::vector<Point> clipper {
        Point { 0, 0 },
        Point { 0, SCREEN_HEIGHT - 1 },
        Point { SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 },
        Point { SCREEN_WIDTH - 1, 0 }
    };

    //

    std::vector<Point> verts = menu_polygon();
    sutherland_hodgman(verts, clipper); // Clip the polygon

    int x, y;
    printf("Enter a point inside of the polygon (x y) > ");
    scanf("%d %d", &x, &y);

    // Draw with Flood Fill
    SDL_LockMutex(mutex);

    // Clear the screen
    clear(pixels);

    draw_polygon(pixels, verts, 0xFF000000);
    draw_floodfill(pixels, x, y, 0xFF000000);

    dirty = true;

    // We can be waiting for a while if the user doesn't hit a key so we'll unlock for now.
    SDL_UnlockMutex(mutex);

    char cont;
    while(true) {
        printf("Draw scanline algorithm? (y) > ");
        scanf("%c", &cont);

        if(cont == 'y')
            break;
    }

    // Draw with scan line
    SDL_LockMutex(mutex);

    // Clear the screen
    clear(pixels);

    draw_scanline(pixels, verts, 0x00FF0000);

    dirty = true;

    // We can be waiting for a while if the user doesn't hit a key so we'll unlock for now.
    SDL_UnlockMutex(mutex);
}

// Helper function for getting a set of points (polygon) from stdin
// This is used for both clipping, and filling.
std::vector<Point> menu_polygon() {
    std::vector<Point> points;
    int n;

    while(true) {
        printf("Number of vertices ( > 2 ) > ");
        scanf("%d", &n);

        if(n > 2) {
            break;
        }

        printf("Number of vertices must be > 2\n");
    }

    printf("Points in clockwise order:\n");

    int x, y;
    for(int i = 0; i < n; i++) {
        printf("Enter point (x y) > ");
        scanf("%d %d", &x, &y);
        points.push_back(Point { x, y });
    }

    return points;
}

// 
// Sutherland-Hodgman Algorithm
// https://www.geeksforgeeks.org/polygon-clipping-sutherland-hodgman-algorithm-please-change-bmp-images-jpeg-png/

// Returns x-value of point of intersectipn of two lines 
int x_intersect(Point p0, Point p1, Point p2, Point p3) {
    int num = (p0.x * p1.y - p0.y * p1.x) * (p2.x - p3.x) - (p0.x - p1.x) * (p2.x * p3.y - p2.y * p3.x); 
    int den = (p0.x - p1.x) * (p2.y - p3.y) - (p0.y - p1.y) * (p2.x - p3.x);
    return num / den;
}

// Returns y-value of point of intersectipn of two lines 
int y_intersect(Point p0, Point p1, Point p2, Point p3) { 
    int num = (p0.x * p1.y - p0.y * p1.x) * (p2.y - p3.y) - (p0.y - p1.y) * (p2.x * p3.y - p2.y * p3.x); 
    int den = (p0.x - p1.x) * (p2.y - p3.y) - (p0.y - p1.y) * (p2.x - p3.x); 
    return num / den;
}

void sh_clip(std::vector<Point>& verts, Point p0, Point p1) {
    std::vector<Point> new_verts;

    for(int i = 0; i < (int) verts.size(); i++) {
        int k = (i + 1) % verts.size();
        Point pi = verts[i];
        Point pk = verts[k];

        int i_pos = (p1.x - p0.x) * (pi.y - p0.y) - (p1.y - p0.y) * (pi.x - p0.x);
        int k_pos = (p1.x - p0.x) * (pk.y - p0.y) - (p1.y - p0.y) * (pk.x - p0.x);

        if(i_pos < 0 && k_pos < 0) {

            // Case 1: Both points are inside
            new_verts.push_back(pk);

        } else if(i_pos >= 0 && k_pos < 0) {
            
            // Case 2: First point is outside
            new_verts.push_back(Point {
                x_intersect(p0, p1, pi, pk),
                y_intersect(p0, p1, pi, pk)
            });

            new_verts.push_back(pk);
            
        } else if(i_pos < 0 && k_pos >= 0) {
            // Case 3: Second point is outside
            
            new_verts.push_back(Point {
                x_intersect(p0, p1, pi, pk),
                y_intersect(p0, p1, pi, pk)
            });

        } else {
            // Case 4: Both are outside, do nothing
        }
    }

    verts = new_verts;
}

void sutherland_hodgman(std::vector<Point>& verts, const std::vector<Point>& clipper) {
    for(int i = 0; i < (int) clipper.size(); i++) {
        int k = (i + 1) % clipper.size();
        sh_clip(verts, clipper[i], clipper[k]);
    }
}

//
// Liang-Barsky Algorithm
//
void liangBarsky(float xmin, float ymin, float xmax, float ymax, Point pi, Point pk) {
}

//
// Flood Fill
//
// MUST BE USED WHEN THE MUTEX IS LOCKED
void draw_floodfill(uint32_t pixels[][SCREEN_WIDTH], int x, int y, uint32_t color) {
    // Check to make sure we aren't accidentally writing to memory outside of the screen if
    // for some reason we break free from the polygon
    if((x <= 0 || x >= SCREEN_WIDTH) || (y <= 0 || y >= SCREEN_HEIGHT))
        return;

    if(pixels[y][x] == color)
        return;
    
    pixels[y][x] = color;

    draw_floodfill(pixels, x, y - 1, color);
    draw_floodfill(pixels, x, y + 1, color);
    draw_floodfill(pixels, x - 1, y, color);
    draw_floodfill(pixels, x + 1, y, color);
}

//
// Scan-line algorithm
//
// MUST BE USED WHEN THE MUTEX IS LOCKED
void draw_scanline(uint32_t pixels[][SCREEN_WIDTH], const std::vector<Point>& verts, uint32_t color) {
    int min_y = verts[0].y;
    int max_y = verts[0].y;

    for(auto& vert : verts) {
        if(vert.y < min_y) {
            min_y = vert.y;
        }

        if(vert.y > max_y) {
            max_y = vert.y;
        }
    }

    //
    SDL_LockMutex(mutex);

    for(int y = min_y + 1; y < max_y; y++){
        // Get the intersections with the scanline
        std::vector<int> v;
        for(int i = 0; i < (int) verts.size(); i++) {
            int k = (i + 1) % verts.size();
            Point p1 = verts[i];
            Point p2 = verts[k];

            if( ((y >= p1.y && y <= p2.y) || (y <= p1.y && y >= p2.y)) && (p1.y != p2.y) ) {
                double m = (double) (p2.y - p1.y) / (double) (p2.x - p1.x);
                double c = p2.y - (double) m * (double) p2.x;
                int x = p1.x == p2.x ? p1.x : round((y - c) / m);

                v.push_back(x);
            }
        }

        // Sort the intersections by X
        std::sort(v.begin(), v.end());
        // Remove duplicates
        v.erase(std::unique(v.begin(), v.end()), v.end());

        // Connect pairs of intersections by a line
        for(int i = 0; i < (int) v.size() - 1; i += 2) {
            draw_line(pixels, Point { v[i], y }, Point { v[i + 1], y }, color);
        }
    }

    dirty = true;

    SDL_UnlockMutex(mutex);
}


// Helper function to make sure we're only writing to pixels on the screen
// MUST BE USED WHEN THE MUTEX IS LOCKED
void plot_point(uint32_t pixels[][SCREEN_WIDTH], int x, int y, uint32_t color) {
    if((y >= 0 && y < SCREEN_HEIGHT) && (x >= 0 && x < SCREEN_WIDTH))
        pixels[y][x] = color;
}

// Helper function for drawing a line
// Bresenham's Algorithm
// https://rosettacode.org/wiki/Bitmap/Bresenham%27s_line_algorithm#C.2B.2B
// MUST BE USED WHEN THE MUTEX IS LOCKED
void draw_line(uint32_t pixels[][SCREEN_WIDTH], Point p0, Point p1, uint32_t color) {
    const bool steep = abs(p1.y - p0.y) > abs(p1.x - p0.x);
    if(steep) {
        std::swap(p0.x, p0.y);
        std::swap(p1.x, p1.y);
    }

    if(p0.x > p1.x) {
        std::swap(p0.x, p1.x);
        std::swap(p0.y, p1.y);
    }

    const float dx = (float) p1.x - p0.x;
    const float dy = (float) abs(p1.y - p0.y);
    
    float error = dx / 2.0f;
    const int ystep = (p0.y < p1.y) ? 1 : -1;
    
    int y = p0.y;

    for(int x = p0.x; x < p1.x; x++) {
        if(steep) {
            plot_point(pixels, y, x, color);
        } else {
            plot_point(pixels, x, y, color);
        }

        error -= dy;
        if(error < 0) {
            y += ystep;
            error += dx;
        }
    }
}

// Helper function to draw a polygon from supplied vertic`es
// MUST BE USED WHEN THE MUTEX IS LOCKED
void draw_polygon(uint32_t pixels[][SCREEN_WIDTH], const std::vector<Point>& verts, uint32_t color) {
    for(int i = 0; i < (int) verts.size() - 1; i++) {
        draw_line(pixels, verts[i], verts[i + 1], color);
    }

    // Connect the last vertex with the first
    draw_line(pixels, verts[verts.size() - 1], verts[0], color);
}

// Loop through all the pixels on the screen and set them to black
// MUST BE USED WHEN THE MUTEX IS LOCKED
void clear(uint32_t pixels[][SCREEN_WIDTH]) {
    for(int y = 0; y < SCREEN_HEIGHT; y++) {
        for(int x = 0; x < SCREEN_WIDTH; x++) {
            pixels[y][x] = 0x00000000;
        }
    }
}

// Translate each vertex in a polygon by a point and return a new set of points (non-destructive)
std::vector<Point> translate_polygon(const std::vector<Point>& verts, Point p) {
    std::vector<Point> new_verts(verts);

    for(auto& vert : new_verts) {
        vert.x += p.x;
        vert.y += p.y;
    }

    return new_verts;
}