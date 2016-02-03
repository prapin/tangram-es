#include "tangram.h"
#include "platform_osx.h"
#include "data/clientGeoJsonSource.h"
#include <cmath>
#include <memory>
#include <signal.h>
#include <stdlib.h>

// Forward declaration
void init_main_window();

std::string sceneFile = "scene.yaml";

GLFWwindow* main_window = nullptr;
int width = 800;
int height = 600;
bool recreate_context;

// Input handling
// ==============

const double double_tap_time = 0.5; // seconds
const double scroll_span_multiplier = 0.05; // scaling for zoom and rotation
const double scroll_distance_multiplier = 5.0; // scaling for shove
const double single_tap_time = 0.25; //seconds (to avoid a long press being considered as a tap)

bool was_panning = false;
double last_time_released = -double_tap_time; // First click should never trigger a double tap
double last_time_pressed = 0.0;
double last_time_moved = 0.0;
double last_x_down = 0.0;
double last_y_down = 0.0;
double last_x_velocity = 0.0;
double last_y_velocity = 0.0;

using namespace Tangram;

std::shared_ptr<ClientGeoJsonSource> data_source;
std::shared_ptr<Marker> marker;
LngLat last_point;

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {

    if (button != GLFW_MOUSE_BUTTON_1) {
        return; // This event is for a mouse button that we don't care about
    }

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    double time = glfwGetTime();

    if (was_panning && action == GLFW_RELEASE) {
        was_panning = false;
        Tangram::handleFlingGesture(x, y, last_x_velocity, last_y_velocity);
        return; // Clicks with movement don't count as taps, so stop here
    }

    if (action == GLFW_PRESS) {
        Tangram::handlePanGesture(0.0f, 0.0f, 0.0f, 0.0f);
        last_x_down = x;
        last_y_down = y;
        last_time_pressed = time;
        return;
    }

    if ((time - last_time_released) < double_tap_time) {
        // Double tap recognized
        LngLat p { x, y };
        Tangram::screenToWorldCoordinates(p.longitude, p.latitude);
        Tangram::setPosition(p.longitude, p.latitude, 1.f);

        logMsg("pick feature\n");
        Tangram::clearDataSource(*data_source, true, true);

        auto picks = Tangram::pickFeaturesAt(x, y);
        std::string name;
        logMsg("picked %d features\n", picks.size());
        for (const auto& it : picks) {
            if (it.properties->getString("name", name)) {
                logMsg(" - %f\t %s\n", it.distance, name.c_str());
            }
        }
    } else if ((time - last_time_pressed) < single_tap_time) {
        // Single tap recognized

        // Remove previous marker
        // Tangram::removeMarker(marker);

        // Add new marker
        glm::dvec2 point {x, y};
        Tangram::screenToWorldCoordinates(point.x, point.y);
        if (marker) {
            marker->setCoordinates(point.x, point.y, 1, EaseType::cubic);
        } else {
            marker = Tangram::createMarker("pois", "sunburst");
            marker->setCoordinates(point.x, point.y);
        }

        requestRender();
    }

    last_time_released = time;

}

void cursor_pos_callback(GLFWwindow* window, double x, double y) {

    int action = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_1);
    double time = glfwGetTime();

    if (action == GLFW_PRESS) {

        if (was_panning) {
            Tangram::handlePanGesture(last_x_down, last_y_down, x, y);
        }

        was_panning = true;
        last_x_velocity = (x - last_x_down) / (time - last_time_moved);
        last_y_velocity = (y - last_y_down) / (time - last_time_moved);
        last_x_down = x;
        last_y_down = y;

    }

    last_time_moved = time;

}

void scroll_callback(GLFWwindow* window, double scrollx, double scrolly) {

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    bool rotating = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
    bool shoving = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    if (shoving) {
        Tangram::handleShoveGesture(scroll_distance_multiplier * scrolly);
    } else if (rotating) {
        Tangram::handleRotateGesture(x, y, scroll_span_multiplier * scrolly);
    } else {
        Tangram::handlePinchGesture(x, y, 1.0 + scroll_span_multiplier * scrolly, 0.f);
    }

}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {

    if (action == GLFW_PRESS) {
        switch (key) {
            case GLFW_KEY_1:
                Tangram::toggleDebugFlag(Tangram::DebugFlags::freeze_tiles);
                break;
            case GLFW_KEY_2:
                Tangram::toggleDebugFlag(Tangram::DebugFlags::proxy_colors);
                break;
            case GLFW_KEY_3:
                Tangram::toggleDebugFlag(Tangram::DebugFlags::tile_bounds);
                break;
            case GLFW_KEY_4:
                Tangram::toggleDebugFlag(Tangram::DebugFlags::tile_infos);
                break;
            case GLFW_KEY_5:
                Tangram::toggleDebugFlag(Tangram::DebugFlags::labels);
                break;
            case GLFW_KEY_6:
                Tangram::toggleDebugFlag(Tangram::DebugFlags::tangram_infos);
                break;
            case GLFW_KEY_BACKSPACE:
                recreate_context = true;
                break;
            case GLFW_KEY_R:
                Tangram::loadScene(sceneFile.c_str());
                break;
            case GLFW_KEY_Z:
                Tangram::setZoom(Tangram::getZoom() + 1.f, 1.5f);
                break;
            case GLFW_KEY_N:
                Tangram::setRotation(0.f, 1.f);
                break;
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(main_window, true);
                break;
        default:
                break;
        }
    }
}

void drop_callback(GLFWwindow* window, int count, const char** paths) {

    sceneFile = std::string(paths[0]);
    Tangram::loadScene(sceneFile.c_str());

}

// Window handling
// ===============

void window_size_callback(GLFWwindow* window, int width, int height) {

    Tangram::resize(width, height);

    // Work-around for a bug in GLFW on retina displays
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(main_window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);
}

void init_main_window() {

    // Setup tangram
    Tangram::initialize(sceneFile.c_str());

    // Destroy old window
    if (main_window != nullptr) {
        glfwDestroyWindow(main_window);
    }

    // Create a windowed mode window and its OpenGL context
    glfwWindowHint(GLFW_SAMPLES, 2);
    main_window = glfwCreateWindow(width, height, "Tangram ES", NULL, NULL);
    if (!main_window) {
        glfwTerminate();
    }

    // Make the main_window's context current
    glfwMakeContextCurrent(main_window);

    // Set input callbacks
    glfwSetWindowSizeCallback(main_window, window_size_callback);
    glfwSetMouseButtonCallback(main_window, mouse_button_callback);
    glfwSetCursorPosCallback(main_window, cursor_pos_callback);
    glfwSetScrollCallback(main_window, scroll_callback);
    glfwSetKeyCallback(main_window, key_callback);
    glfwSetDropCallback(main_window, drop_callback);

    // Setup graphics
    Tangram::setupGL();
    Tangram::resize(width, height);

    // Work-around for a bug in GLFW on retina displays
    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(main_window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    data_source = std::make_shared<ClientGeoJsonSource>("touch", "");
    Tangram::addDataSource(data_source);

}

// Main program
// ============

int main(int argc, char* argv[]) {

    static bool keepRunning = true;

    // Give it a chance to shutdown cleanly on CTRL-C
    signal(SIGINT, [](int) {
            if (keepRunning) {
                logMsg("shutdown\n");
                keepRunning = false;
                glfwPostEmptyEvent();
            } else {
                logMsg("killed!\n");
                exit(1);
            }});

    int argi = 0;
    while (++argi < argc) {
        if (strcmp(argv[argi - 1], "-f") == 0) {
            sceneFile = std::string(argv[argi]);
            logMsg("File from command line: %s\n", argv[argi]);
            break;
        }
    }

    // Initialize the windowing library
    if (!glfwInit()) {
        return -1;
    }

    init_main_window();

    // Initialize networking
    NSurlInit();

    double lastTime = glfwGetTime();

    recreate_context = false;

    // Loop until the user closes the window
    while (keepRunning && !glfwWindowShouldClose(main_window)) {

        double currentTime = glfwGetTime();
        double delta = currentTime - lastTime;
        lastTime = currentTime;

        // Render
        Tangram::update(delta);
        Tangram::render();

        // Swap front and back buffers
        glfwSwapBuffers(main_window);

        // Poll for and process events
        if (isContinuousRendering()) {
            glfwPollEvents();
        } else {
            glfwWaitEvents();
        }

        if (recreate_context) {
            logMsg("recreate context\n");
             // Simulate GL context loss
            init_main_window();
            recreate_context = false;
        }

    }

    glfwTerminate();
    return 0;
}
