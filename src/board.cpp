#include "board.hpp"
#include <ftxui/dom/elements.hpp>
#include <random>

namespace {

constexpr int g_board_width = 150;
constexpr int g_board_height = 150;

struct CollisionCallback : public b2QueryCallback {
  bool ReportFixture(b2Fixture* /*fixture*/) override {
    collided = true;
    return false; // Terminate the query.
  }
  bool collided = false;
};

b2Vec2 Gravity() {
  return b2Vec2(0.F, 140.F);  // NOLINT
}

}  // namespace

Board::Board(BoardConfig config) : config_(config), world_(Gravity()) {
  world_.SetContactListener(&contact_listener_);

  InitializeBricks();
}

void Board::InitializeBricks() {
  std::random_device rd;
  std::mt19937 mt(rd());

  using uniform = std::uniform_int_distribution<int>;
  auto x_dist = uniform(0, g_board_width / 2);        // NOLINT
  auto y_dist =
      uniform(g_board_height * 2 / 4, 5 * g_board_height / 4);  // NOLINT
  auto half_width_dist = uniform(2, 10);                        // NOLINT
  auto half_height_dist = uniform(1, 4);                        // NOLINT
  auto counter_distribution = uniform(1, 5);                    // NOLINT

  const int max_iterations = 100000;
  for (int i = 0; i < max_iterations; ++i) {
    int x = x_dist(mt) * 2;                      // NOLINT
    int y = y_dist(mt) * 4;                      // NOLINT
    int half_width = half_width_dist(mt) * 2;    // NOLINT
    int half_height = half_height_dist(mt) * 4;  // NOLINT
    int counter = counter_distribution(mt);

    b2AABB aabb;
    aabb.lowerBound.x = static_cast<float>(x - half_width);
    aabb.lowerBound.y = static_cast<float>(y - half_height);
    aabb.upperBound.x = static_cast<float>(x + half_width);
    aabb.upperBound.y = static_cast<float>(y + half_height);

    CollisionCallback callback;
    world_.QueryAABB(&callback, aabb);
    if (callback.collided) {
      continue;
    }

    bricks_.push_back(std::make_unique<BrickBase>(world_, x, y, half_width,
                                                  half_height, counter));
    const size_t bricks = 5000;
    if (bricks_.size() >= bricks) {
      break;
    }
  }
}

// NOLINTNEXTLINE
bool Board::OnEvent(ftxui::Event event) {
  if (!event.is_mouse()) {
    return false;
  }

  mouse_x_ = (event.mouse().x - 1) * 2;
  mouse_y_ = (event.mouse().y - 1) * 4;

  if (is_shooting_) {
    return false;
  }

  if (event.mouse().motion != ftxui::Mouse::Released) {
    return false;
  }

  is_shooting_ = true;
  remaining_balls_to_shoot_ = config_.balls;
  shooting_direction_ = ShootSpeed();
  return true;
}

void Board::Step() {
  // Evolve the worlds using Box2D.
  step_ ++;
  float timeStep = 1.0f / 60.0f;  // NOLINT
  int32 velocityIterations = 6;   // NOLINT
  int32 positionIterations = 2;   // NOLINT
  world_.Step(timeStep, velocityIterations, positionIterations);

  for (auto& brick : bricks_) {
    brick->Step();
  }

  // Erase dead bricks.
  auto counter_null = [](const Brick& brick) { return brick->counter() == 0; };
  bricks_.erase(std::remove_if(bricks_.begin(), bricks_.end(), counter_null),
                bricks_.end());

  // Shoot a sequence of balls.
  const int shoot_steps = 10;
  if (is_shooting_ && step_ % shoot_steps == 0 &&
      remaining_balls_to_shoot_ >= 1) {
    remaining_balls_to_shoot_--;
    const float radius = 3.F;
    balls_.push_back(std::make_unique<BallBase>(world_, ShootPosition(),
                                                shooting_direction_, radius));
  }

  // Erase out of screen balls.
  auto ball_out_of_screen = [](const Ball& ball) {
    return ball->x() < -10.F ||  // NOLINT
           ball->x() > 160.F ||  // NOLINT
           ball->y() >= 160.F;   // NOLINT
  };
  balls_.erase(std::remove_if(balls_.begin(), balls_.end(), ball_out_of_screen),
               balls_.end());

  // Allow user to shoot again after the previous shoot completed.
  if (is_shooting_ && remaining_balls_to_shoot_ == 0 && balls_.empty()) {
    is_shooting_ = false;
    MoveUp();
  }

  int min_y = g_board_height * 2;
  for (const auto& brick : bricks_) {
    min_y = std::min(min_y, brick->y());
  }
  if (min_y > g_board_height * 4 / 5) {  // NOLINT
    MoveUp();
  }

  // Move bricks that are close to the bottom of the screen higher.
  for (const auto& brick : bricks_) {
    const int threshold = 20;
    if (brick->y() > g_board_height &&
        brick->y() < g_board_height + threshold) {
      brick->MoveUp();
    }
  }
}

void Board::MoveUp() {
  for (auto& brick : bricks_) {
    brick->MoveUp();
  }
}

void Board::Draw(ftxui::Canvas& c) const {
  for (const Ball& ball : balls_) {
    ball->Draw(c);
  }
  for (const Brick& brick : bricks_) {
    brick->Draw(c);
  }

  DrawShootingLine(c);
}

void Board::DrawShootingLine(ftxui::Canvas& c) const {
  if (is_shooting_) {
    return;
  }

  b2Vec2 position = ShootPosition();
  b2Vec2 speed = ShootSpeed();
  float timeStep = 1.0f / 60.0f;  // NOLINT

  const float ball_friction = 0.45F;
  const float friction = std::pow(ball_friction, timeStep);

  const int iterations = 50;
  const int iterations_inner = 2;
  for (int i = 0; i < iterations; i++) {
    b2Vec2 start = position;
    for (int j = 0; j < iterations_inner; ++j) {
      b2Vec2 position_increment = speed;
      b2Vec2 speed_increment = Gravity();

      position_increment *= timeStep;
      speed_increment *= timeStep * friction;

      position += position_increment;
      speed += speed_increment;

      speed *= friction;
    }

    const int color_value = -10 * step_ + 15 * i;  // NOLINT
    c.DrawPointLine(
        static_cast<int>(start.x),     //
        static_cast<int>(start.y),     //
        static_cast<int>(position.x),  //
        static_cast<int>(position.y),  //
        // NOLINTNEXTLINE
        ftxui::Color::HSV(static_cast<uint8_t>(color_value), 255, 128));
  }
}

// static
b2Vec2 Board::ShootPosition() {
  const b2Vec2 position(75.F, 0.F);
  return position;
}

b2Vec2 Board::ShootSpeed() const {
  const b2Vec2 position = ShootPosition();
  const auto mouse_x = static_cast<float>(mouse_x_);
  const auto mouse_y = static_cast<float>(mouse_y_);
  const b2Vec2 target(mouse_x, mouse_y);
  b2Vec2 speed = target - position;
  speed.Normalize();
  const float speed_norm = 100.F;
  speed *= speed_norm;
  return speed;
}