#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <math.h>    // for sqrtf, sinf

// Screen dimensions
#define W 128
#define H 64

// Global running flag must be declared before input_callback
static bool app_running = true;

// Minimum and maximum dot-density thresholds (0–100)
static uint8_t dot_threshold = 50; // start at 50% chance per pixel

// Four “styles” for kaleidoscope, but reordered per your request:
// 0 = old-style2  (rotated-line “star” → originally Vis 3)
// 1 = old-style1  (concentric-arcs → originally Vis 2, unchanged)
// 2 = old-style3  (quadrant-noise → originally Vis 4)
// 3 = old-style0  (random mirrored dots → originally Vis 1)
static uint8_t style = 0;

// Frame counter for animation
static uint32_t frame = 0;

// Clamp helper
static uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi) {
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

//--------------------------------------------------------------------------------
// OLD-STYLE0 → is now at index 3: Random mirrored dots (static until arrow redraw)
//--------------------------------------------------------------------------------
static void render_style0(Canvas* canvas) {
    canvas_clear(canvas);
    for(uint8_t x = 0; x < W/2; x++) {
        for(uint8_t y = 0; y < H; y++) {
            if((rand() % 100) < dot_threshold) {
                canvas_draw_dot(canvas, x, y);
                canvas_draw_dot(canvas, W - 1 - x, y);
            }
        }
    }
}

//--------------------------------------------------------------------------------
// OLD-STYLE1 → is still at index 1: Animated concentric-arc segments
//--------------------------------------------------------------------------------
static void render_style1(Canvas* canvas) {
    canvas_clear(canvas);
    int cx = W/2;
    int cy = H/2;
    uint8_t step = clamp_u8(dot_threshold / 10 + 2, 2, 10);
    int offset = frame % step;
    for(int r = offset; r < cy; r += step) {
        for(int dy = -r; dy <= r; dy++) {
            int inside = (r * r) - (dy * dy);
            if(inside < 0) continue;
            float xf = sqrtf((float)inside);
            int dx = (int)(xf + 0.5f);
            canvas_draw_dot(canvas, cx - dx, cy + dy);
            canvas_draw_dot(canvas, cx + dx, cy + dy);
        }
    }
}

//--------------------------------------------------------------------------------
// OLD-STYLE2 → is now at index 0: Animated rotated-line “star” pattern
//--------------------------------------------------------------------------------
static void render_style2(Canvas* canvas) {
    canvas_clear(canvas);
    int cx = W/2;
    int cy = H/2;
    uint8_t spokes = clamp_u8(dot_threshold / 10 + 2, 2, 16);
    float base_angle = (frame * 0.05f);
    float angle_step = 3.14159f / spokes;
    for(uint8_t s = 0; s < spokes; s++) {
        float angle = base_angle + (s * angle_step);
        for(int len = 0; len < (W/2); len++) {
            int x_off = (int)(cosf(angle) * len);
            int y_off = (int)(sinf(angle) * len);
            canvas_draw_dot(canvas, cx + x_off, cy + y_off);
            canvas_draw_dot(canvas, cx - x_off, cy + y_off);
        }
        float perp = angle + 3.14159f / 2.0f;
        for(int len = 0; len < (H/2); len++) {
            int x_off = (int)(cosf(perp) * len);
            int y_off = (int)(sinf(perp) * len);
            canvas_draw_dot(canvas, cx + x_off, cy + y_off);
            canvas_draw_dot(canvas, cx - x_off, cy + y_off);
        }
    }
}

//--------------------------------------------------------------------------------
// OLD-STYLE3 → is now at index 2: Animated quadrant-based noise (gradient)
//--------------------------------------------------------------------------------
static void render_style3(Canvas* canvas) {
    canvas_clear(canvas);
    int cx = W/2;
    int cy = H/2;
    int maxDist = cx + cy;
    srand(furi_get_tick() ^ frame);
    for(int x = 0; x < W; x++) {
        for(int y = 0; y < H; y++) {
            int dx = abs(x - cx);
            int dy = abs(y - cy);
            int dist = dx + dy;
            int localThresh = dot_threshold - (dist * dot_threshold / maxDist);
            if(localThresh < 0) localThresh = 0;
            if((rand() % 100) < (uint8_t)localThresh) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

//--------------------------------------------------------------------------------
// NEW-STYLE4: Spiral swirl
//--------------------------------------------------------------------------------
static void render_style4(Canvas* canvas) {
    canvas_clear(canvas);
    int cx = W/2, cy = H/2;

    for(int y = 0; y < H; y++) {
        for(int x = 0; x < W; x++) {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            float r = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx);

            float val = sinf(r*0.3f + angle*6.0f - frame*0.1f);
            if(val > 0.8f) canvas_draw_dot(canvas, x, y);
        }
    }
}

//--------------------------------------------------------------------------------
// NEW-STYLE5: Animated checkerboard wave
//--------------------------------------------------------------------------------
static void render_style5(Canvas* canvas) {
    canvas_clear(canvas);

    for(int y = 0; y < H; y++) {
        for(int x = 0; x < W; x++) {
            float nx = x * 0.1f + frame*0.05f;
            float ny = y * 0.1f;

            int checker = (((int)floorf(nx) + (int)floorf(ny)) & 1);
            if(checker) {
                float wave = sinf(nx*1.5f) * cosf(ny*1.5f);
                if(wave > 0.3f) canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

//--------------------------------------------------------------------------------
// NEW-STYLE6: Pulsating radial sunburst
//--------------------------------------------------------------------------------
static void render_style6(Canvas* canvas) {
    canvas_clear(canvas);
    int cx = W/2, cy = H/2;

    // Rays count depends on density (between 6 and 24)
    int rays = 6 + (dot_threshold / 5); 
    float speed = frame * 0.08f;

    for(int y = 0; y < H; y++) {
        for(int x = 0; x < W; x++) {
            float dx = (float)(x - cx);
            float dy = (float)(y - cy);
            float r = sqrtf(dx*dx + dy*dy);
            float angle = atan2f(dy, dx);

            // Angular ray pattern
            float ray_val = cosf(angle * rays + speed);

            // Radial pulsation
            float ring_val = sinf(r * 0.25f - speed * 0.7f);

            // Combine
            if(ray_val * ring_val > 0.65f) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}


//--------------------------------------------------------------------------------
// General render switcher (reordered)
//--------------------------------------------------------------------------------
static void render_pattern(Canvas* canvas) {
    frame++;
    switch(style) {
        case 0: render_style2(canvas); break; // rotated star
        case 1: render_style1(canvas); break; // arcs
        case 2: render_style3(canvas); break; // noise
        case 3: render_style0(canvas); break; // mirrored dots
        case 4: render_style4(canvas); break; // spiral swirl
        case 5: render_style5(canvas); break; // checkerboard
        case 6: render_style6(canvas); break; // sunburst
        default: render_style0(canvas); break;
    }
}



// ViewPort draw callback (ctx unused here)
static void view_callback(Canvas* canvas, void* ctx) {
    (void)ctx;
    render_pattern(canvas);
}

// ViewPort input callback (arrow keys + Back, plus immediate redraw)
static void input_callback(InputEvent* event, void* ctx) {
    ViewPort* vp = ctx;
    if(event->type != InputTypeShort) return;

    bool do_redraw = false;

    switch(event->key) {
        case InputKeyBack:
            app_running = false;
            return;
        case InputKeyLeft:
            style = (style == 0) ? 6 : (style - 1);
            do_redraw = true;
            break;
        case InputKeyRight:
            style = (style == 6) ? 0 : (style + 1);
            do_redraw = true;
            break;
        case InputKeyUp:
            dot_threshold = clamp_u8(dot_threshold + 10, 0, 100);
            do_redraw = true;
            break;
        case InputKeyDown:
            dot_threshold = (dot_threshold < 10) ? 0 : (dot_threshold - 10);
            do_redraw = true;
            break;
        default:
            break;
    }

    if(do_redraw) {
        view_port_update(vp);
    }
}

// Entry point (must match entry_point in application.fam)
int32_t digital_kaleidoscope_app(void* p) {
    (void)p;
    srand(furi_get_tick());

    // Set up GUI and ViewPort
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, view_callback, NULL);
    view_port_input_callback_set(viewport, input_callback, viewport);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);
    view_port_enabled_set(viewport, true);

    // Main loop: update viewport for animation until Back is pressed
    while(app_running) {
        furi_delay_ms(100);
        view_port_update(viewport);
    }

    // Clean up
    gui_remove_view_port(gui, viewport);
    view_port_enabled_set(viewport, false);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    return 0;
}
	
