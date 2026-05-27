#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <mouse_jiggler_icons.h>
#include <string.h>

#define APP_VERSION "1.3"
#define ERROR_MESSAGE_ROTATE_MS 5000

typedef enum {
    UsbStateSwitching,
    UsbStateActive,
    UsbStateError,
} UsbState;

typedef enum {
    ModeRandom,
    ModeSquare,
} MovementMode;

typedef struct {
    UsbState state;
    bool paused;
    bool show_unplug_message;
    uint32_t last_message_switch_tick;
    MovementMode mode;
    uint16_t square_size;
    bool square_direction_up; // true = up/right, false = down/left
    uint16_t square_current_x;
    uint16_t square_current_y;
    bool square_moving_horizontal; // true = moving horizontally, false = vertically
} MouseJigglerContext;

static MouseJigglerContext* global_app_ctx = NULL;

static void mouse_jiggler_render_callback(Canvas* canvas, void* ctx) {
    MouseJigglerContext* app_ctx = ctx;
    canvas_clear(canvas);

    canvas_draw_icon(canvas, 0, 0, &I_mouse_jiggler);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 14, 9, "Mouse Jiggler v");
    canvas_draw_str(canvas, 94, 9, APP_VERSION);

    canvas_set_font(canvas, FontSecondary);

    switch(app_ctx->state) {
        case UsbStateSwitching:
            canvas_draw_str(canvas, 0, 33, "Switching to HID mode...");
            canvas_draw_str(canvas, 0, 43, "Please wait");
            break;
        case UsbStateError:
            if(app_ctx->show_unplug_message) {
                canvas_draw_str(canvas, 0, 33, "Try unplugging your Flipper");
                canvas_draw_str(canvas, 0, 43, "and plug it in again");
            } else {
                canvas_draw_str(canvas, 0, 33, "Switch to HID mode failed");
                canvas_draw_str(canvas, 0, 43, "Retrying...");
            }
            break;
        case UsbStateActive:
            default:
                if(app_ctx->paused) {
                    canvas_draw_str(canvas, 0, 25, "** PAUSED **");
                    canvas_draw_str(canvas, 0, 45, "Press [OK] to resume");
                } else {
                    canvas_draw_str(canvas, 0, 25, "Jiggling...");
                    canvas_draw_str(canvas, 0, 45, "Press [OK] to pause");
                }
                
                // Show mode and square size
                char mode_str[20];
                if(app_ctx->mode == ModeRandom) {
                    strcpy(mode_str, "Mode: Random");
                } else {
                    strcpy(mode_str, "Mode: Square");
                    char size_str[16];
                    snprintf(size_str, sizeof(size_str), "Size: %d", app_ctx->square_size);
                    canvas_draw_str(canvas, 0, 55, size_str);
                }
                canvas_draw_str(canvas, 0, 35, mode_str);
        }

    canvas_draw_str(canvas, 0, 63, "Hold [back] to exit");
}

static void mouse_jiggler_input_callback(InputEvent* input_event, void* ctx) {
    // Store the app context globally for use in jiggle function
    // Access it from the main loop where we set it
    if (global_app_ctx != NULL && input_event->type == InputTypeShort) {
        // Handle button inputs
        if (input_event->key == InputKeyLeft) {
            // Decrease square size
            if (global_app_ctx->mode == ModeSquare && global_app_ctx->square_size > 10) {
                global_app_ctx->square_size -= 10;
            }
        } else if (input_event->key == InputKeyRight) {
            // Increase square size
            if (global_app_ctx->mode == ModeSquare && global_app_ctx->square_size < 200) {
                global_app_ctx->square_size += 10;
            }
        } else if (input_event->key == InputKeyUp) {
            // Change movement direction (up/down toggle)
            if (global_app_ctx->mode == ModeSquare) {
                global_app_ctx->square_direction_up = !global_app_ctx->square_direction_up;
            }
        } else if (input_event->key == InputKeyDown) {
            // Toggle between random and square mode
            if (global_app_ctx->state == UsbStateActive) {
                global_app_ctx->mode = (global_app_ctx->mode == ModeRandom) ? ModeSquare : ModeRandom;
                // Reset square position when changing modes
                global_app_ctx->square_current_x = 0;
                global_app_ctx->square_current_y = 0;
                global_app_ctx->square_moving_horizontal = true;
            }
        } else if (input_event->key == InputKeyOk) {
            // Middle button already handled in main loop for start/stop
            // Nothing to do here, it's handled in the main loop
        }
    }
    
    // Forward the input event to the queue (ctx is the event_queue)
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static void mouse_jiggler_jiggle(void* ctx) {
    UNUSED(ctx);

    // Use global app context if available
    if (global_app_ctx == NULL) {
        // Fallback to random movement if context not available
        static short horizontal_travel_dist = 0;
        static short horizontal_movement_cycles = 0;
        static short horizontal_current_cycle = 0;
        static short vertical_travel_dist = 0;
        static short vertical_movement_cycles = 0;
        static short vertical_current_cycle = 0;

        if(horizontal_current_cycle >= horizontal_movement_cycles) {
            horizontal_travel_dist = furi_hal_random_get() % 3 - 1;
            horizontal_movement_cycles = furi_hal_random_get() % 1000 + 1;
            horizontal_current_cycle = 0;
        }

        if(vertical_current_cycle >= vertical_movement_cycles) {
            vertical_travel_dist = furi_hal_random_get() % 3 - 1;
            vertical_movement_cycles = furi_hal_random_get() % 1000 + 1;
            vertical_current_cycle = 0;
        }

        furi_hal_hid_mouse_move(horizontal_travel_dist, vertical_travel_dist);
        horizontal_current_cycle++;
        vertical_current_cycle++;
        return;
    }

    // Use the actual movement logic based on mode
    if (global_app_ctx->mode == ModeRandom) {
        // Original random movement code
        static short horizontal_travel_dist = 0;
        static short horizontal_movement_cycles = 0;
        static short horizontal_current_cycle = 0;
        static short vertical_travel_dist = 0;
        static short vertical_movement_cycles = 0;
        static short vertical_current_cycle = 0;

        if(horizontal_current_cycle >= horizontal_movement_cycles) {
            horizontal_travel_dist = furi_hal_random_get() % 3 - 1;
            horizontal_movement_cycles = furi_hal_random_get() % 1000 + 1;
            horizontal_current_cycle = 0;
        }

        if(vertical_current_cycle >= vertical_movement_cycles) {
            vertical_travel_dist = furi_hal_random_get() % 3 - 1;
            vertical_movement_cycles = furi_hal_random_get() % 1000 + 1;
            vertical_current_cycle = 0;
        }

        furi_hal_hid_mouse_move(horizontal_travel_dist, vertical_travel_dist);
        horizontal_current_cycle++;
        vertical_current_cycle++;
    } else {
        // Square movement pattern using state machine
        // States: 0=right, 1=down, 2=left, 3=up
        int square_size = global_app_ctx->square_size;
        
        // Get current state from context (we store step in square_current_x, progress in square_current_y)
        uint8_t side = global_app_ctx->square_current_x % 4;
        int16_t progress = global_app_ctx->square_current_y;
        
        // Define directions for each state: 0=right, 1=down, 2=left, 3=up
        const int8_t dx_table[4] = {1, 0, -1, 0};
        const int8_t dy_table[4] = {0, 1, 0, -1};
        
        int8_t dx = dx_table[side];
        int8_t dy = dy_table[side];
        
        // Move one step in current direction
        furi_hal_hid_mouse_move(dx, dy);
        
        // Track progress and switch sides when reaching corners
        progress++;
        if (progress >= square_size) {
            // Switch to next side of the square
            side = (side + 1) & 3; // Cycle 0->1->2->3->0
            progress = 0;
        }
        
        // Store state back to context
        global_app_ctx->square_current_x = side;
        global_app_ctx->square_current_y = progress;
    }
}

int32_t mouse_jiggler_app(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    furi_check(event_queue);

    ViewPort* view_port = view_port_alloc();

    FuriTimer* timer = furi_timer_alloc(mouse_jiggler_jiggle, FuriTimerTypePeriodic, event_queue);

    FuriHalUsbInterface* usb_mode_prev = furi_hal_usb_get_config();
    MouseJigglerContext app_ctx = {
        .state = UsbStateSwitching,
        .paused = false,
        .show_unplug_message = false,
        .last_message_switch_tick = 0,
        .mode = ModeRandom,
        .square_size = 100,
        .square_direction_up = true,
        .square_current_x = 0,
        .square_current_y = 0,
        .square_moving_horizontal = true,
    };
    global_app_ctx = &app_ctx;

    view_port_draw_callback_set(view_port, mouse_jiggler_render_callback, &app_ctx);
    view_port_input_callback_set(view_port, mouse_jiggler_input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool usb_switch_success = false;

    InputEvent event;

    while(1) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, usb_switch_success ? FuriWaitForever : 100);
        
        if(event_status == FuriStatusOk && event.type == InputTypeLong && event.key == InputKeyBack) {
            break;
        }

        if(event_status == FuriStatusOk && event.type == InputTypeShort && event.key == InputKeyOk) {
            if(app_ctx.state == UsbStateActive) {
                if(!app_ctx.paused) {
                    if(furi_timer_stop(timer) == FuriStatusOk) {
                        app_ctx.paused = true;
                    }
                } else {
                    if(furi_timer_start(timer, 3) == FuriStatusOk) {
                        app_ctx.paused = false;
                    }
                }
            }
        }

        if(!usb_switch_success) {
            if(furi_hal_usb_set_config(&usb_hid, NULL)) {
                usb_switch_success = true;
                app_ctx.state = UsbStateActive;
                furi_timer_start(timer, 3);
            } else {
                if(app_ctx.state != UsbStateError) {
                    app_ctx.last_message_switch_tick = furi_get_tick();
                }
                app_ctx.state = UsbStateError;
                furi_delay_ms(200);
            }
        }

        if(app_ctx.state == UsbStateError) {
            uint32_t now = furi_get_tick();
            if((now - app_ctx.last_message_switch_tick) >= ERROR_MESSAGE_ROTATE_MS) {
                app_ctx.show_unplug_message = !app_ctx.show_unplug_message;
                app_ctx.last_message_switch_tick = now;
            }
        }

        view_port_update(view_port);
    }

    furi_timer_stop(timer);
    furi_hal_usb_set_config(usb_mode_prev, NULL);

    // remove & free all stuff created by app
    furi_timer_free(timer);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);

    return 0;
}