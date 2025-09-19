#include <Gosu/Gosu.hpp>
#include <Gosu/AutoLink.hpp>
#include <vector>
#include <memory>
#include <algorithm>

// --- Base Object Class ---
class Objekt {
public:
    double x, y, width, height;
    Objekt(double x, double y, double width, double height)
        : x(x), y(y), width(width), height(height) {
    }
    virtual void draw(Gosu::Graphics& graphics) = 0;
    virtual ~Objekt() {}
};

// --- Platform Class: Now supports color! ---
class Platform : public Objekt {
    Gosu::Color color;
public:
    Platform(double x, double y, double width, double height, Gosu::Color color = Gosu::Color::GRAY)
        : Objekt(x, y, width, height), color(color) {
    }
    void draw(Gosu::Graphics& graphics) override {
        graphics.draw_rect(x, y, width, height, color, 0.0);
    }
};

// --- Obstacle (Triangle Spike) Class ---
class Obstacle : public Objekt {
public:
    Obstacle(double x, double y, double size) : Objekt(x, y, size, size) {}
    void draw(Gosu::Graphics& graphics) override {
        graphics.draw_triangle(
            x, y + height, Gosu::Color::RED,
            x + width / 2, y, Gosu::Color::RED,
            x + width, y + height, Gosu::Color::RED,
            0.0
        );
    }
};

// --- Player Class ---
class Player : public Objekt {
public:
    double velocity_x = 0, velocity_y = 0;
    bool on_ground = false;
    const double* world_width;
    const double* world_height;
    int jumps_available = 2;
    double spawn_x, spawn_y;
    bool jump_in_progress = false;

    Player(double x, double y, const double* ww, const double* wh)
        : Objekt(x, y, 50, 50), world_width(ww), world_height(wh),
        spawn_x(x), spawn_y(y) {
    }

    void update(
        const Gosu::Input& input,
        const std::vector<std::unique_ptr<Platform>>& platforms,
        Platform* temp_platform
    ) {
        const double gravity = 0.5;
        const double jump_strength = -10.0;
        const double move_speed = 3.0;

        velocity_x = 0;
        if (input.down(Gosu::KB_LEFT)) velocity_x -= move_speed;
        if (input.down(Gosu::KB_RIGHT)) velocity_x += move_speed;

        // Double jump logic
        if (input.down(Gosu::KB_UP)) {
            if (jumps_available > 0 && !jump_in_progress) {
                velocity_y = jump_strength;
                on_ground = false;
                jumps_available--;
                jump_in_progress = true;
            }
        }
        else {
            jump_in_progress = false;
        }

        velocity_y += gravity;

        double next_x = x + velocity_x;
        double next_y = y + velocity_y;
        bool on_any_platform = false;

        // Include both static and temp platform in collision checks
        std::vector<const Platform*> all_platforms;
        for (const auto& plat : platforms) all_platforms.push_back(plat.get());
        if (temp_platform) all_platforms.push_back(temp_platform);

        for (const Platform* plat : all_platforms) {
            bool within_x = next_x + width > plat->x && next_x < plat->x + plat->width;
            bool falling_onto = y + height <= plat->y && next_y + height >= plat->y;
            if (within_x && falling_onto && velocity_y >= 0) {
                next_y = plat->y - height;
                velocity_y = 0;
                on_any_platform = true;
            }
        }

        x = next_x;
        y = next_y;
        on_ground = on_any_platform;

        // World borders
        if (x < 0) x = 0;
        if (x + width > *world_width) x = *world_width - width;
        if (y < 0) y = 0;
        if (y + height > *world_height) {
            y = *world_height - height;
            velocity_y = 0;
            on_ground = true;
        }

        if (on_ground)
            jumps_available = 2;
    }

    void die() {
        x = spawn_x;
        y = spawn_y;
        velocity_x = 0;
        velocity_y = 0;
        jumps_available = 2;
    }
    void draw(Gosu::Graphics& graphics) override {
        graphics.draw_rect(x, y, width, height, Gosu::Color::GREEN, 0.0);
    }
};

bool rects_overlap(const Objekt& a, const Objekt& b) {
    return a.x < b.x + b.width &&
        a.x + a.width > b.x &&
        a.y < b.y + b.height &&
        a.y + a.height > b.y;
}

class GameWindow : public Gosu::Window {
    std::vector<std::unique_ptr<Platform>> platforms;
    std::vector<std::unique_ptr<Obstacle>> obstacles;
    std::unique_ptr<Player> player;
    std::unique_ptr<Platform> temp_platform;
    unsigned long temp_platform_created = 0;
    unsigned long temp_platform_last_placed = 0;
    const unsigned long platform_cooldown = 5000; // 5 seconds

    const double world_width = 2000;
    const double world_height = 1000;

public:
    GameWindow()
        : Gosu::Window(800, 600, false)
    {
        set_caption("2D Sidescroller - AQUA Platform Limited");

        player = std::make_unique<Player>(150, 100, &world_width, &world_height);

        platforms.push_back(std::make_unique<Platform>(0, 950, 2000, 50));
        platforms.push_back(std::make_unique<Platform>(300, 800, 250, 30));
        platforms.push_back(std::make_unique<Platform>(700, 700, 250, 30));
        platforms.push_back(std::make_unique<Platform>(1300, 850, 300, 25));
        platforms.push_back(std::make_unique<Platform>(1700, 600, 200, 30));
        platforms.push_back(std::make_unique<Platform>(1800, 400, 120, 30));
        platforms.push_back(std::make_unique<Platform>(100, 650, 180, 20));

        obstacles.push_back(std::make_unique<Obstacle>(500, 920, 40));
        obstacles.push_back(std::make_unique<Obstacle>(900, 670, 40));
        obstacles.push_back(std::make_unique<Obstacle>(1350, 820, 40));
        obstacles.push_back(std::make_unique<Obstacle>(1800, 570, 40));
    }

    void update() override {
        // Platform cooldown/creation
        static bool down_pressed_last_frame = false;

        if (input().down(Gosu::KB_DOWN)) {
            if (!down_pressed_last_frame) {
                bool on_cooldown = (Gosu::milliseconds() - temp_platform_last_placed < platform_cooldown);
                if (!temp_platform && !on_cooldown) {
                    double width = 100, height = 15;
                    double platform_x = player->x + player->width / 2 - width / 2;
                    double platform_y = player->y + player->height + 2;
                    temp_platform = std::make_unique<Platform>(
                        platform_x, platform_y, width, height, Gosu::Color::AQUA
                    );
                    temp_platform_created = Gosu::milliseconds();
                    temp_platform_last_placed = temp_platform_created;
                }
            }
            down_pressed_last_frame = true;
        }
        else {
            down_pressed_last_frame = false;
        }

        // Remove temp platform after 5 seconds
        if (temp_platform && Gosu::milliseconds() - temp_platform_created > 5000) {
            temp_platform.reset();
        }

        player->update(input(), platforms, temp_platform.get());

        // Obstacle collision
        for (const auto& obstacle : obstacles) {
            if (rects_overlap(*player, *obstacle)) {
                player->die();
            }
        }
    }

    void draw() override {
        double camera_x = player->x + player->width / 2 - width() / 2;
        double camera_y = player->y + player->height / 2 - height() / 2;

        camera_x = std::max(0.0, std::min(camera_x, world_width - width()));
        camera_y = std::max(0.0, std::min(camera_y, world_height - height()));

        graphics().transform(Gosu::translate(-camera_x, -camera_y), [&] {
            for (const auto& plat : platforms) plat->draw(graphics());
            for (const auto& obstacle : obstacles) obstacle->draw(graphics());
            if (temp_platform) temp_platform->draw(graphics());
            player->draw(graphics());
            });
    }
};

int main() {
    GameWindow window;
    window.show();
}
