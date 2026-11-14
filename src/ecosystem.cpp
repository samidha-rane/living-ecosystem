// Living Ecosystem: an OOP simulation in C++17, compiled to WebAssembly.
// Classes: Entity (abstract) -> Plant, Creature (abstract) -> Herbivore, Predator. World owns everything.
#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define EXPORT extern "C" EMSCRIPTEN_KEEPALIVE
#else
#define EXPORT extern "C"
#endif

struct Vec2 {
  float x = 0, y = 0;
  Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  Vec2 operator*(float k) const { return {x * k, y * k}; }
  float len() const { return std::sqrt(x * x + y * y); }
  Vec2 norm() const { float l = len(); return l > 1e-5f ? Vec2{x / l, y / l} : Vec2{}; }
};

enum Kind { PLANT = 0, HERBIVORE = 1, PREDATOR = 2 };
class World;

// Heritable traits that mutate each generation: this is where evolution happens.
struct Genome {
  float speed = 60, sense = 80;
  Genome mutated(std::mt19937& r) const {
    std::uniform_real_distribution<float> a(-6, 6), b(-8, 8);
    return {std::clamp(speed + a(r), 20.f, 140.f), std::clamp(sense + b(r), 20.f, 200.f)};
  }
};

class Entity {
public:
  Vec2 pos; float energy; bool alive = true;
  Entity(Vec2 p, float e) : pos(p), energy(e) {}
  virtual ~Entity() = default;
  virtual Kind kind() const = 0;
  virtual float radius() const = 0;
  virtual void update(World& w, float dt) = 0;
};

class World {
public:
  float w, h, plantGrowth = 1.f; int plantCap = 450, herbCap = 170, predCap = 30;
  std::mt19937 rng;
  std::vector<std::unique_ptr<Entity>> ents, newborn;
  int counts[3] = {0, 0, 0};

  World(float W, float H, unsigned seed) : w(W), h(H), rng(seed) {}
  float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }
  Vec2 clampPos(Vec2 p) const { return {std::clamp(p.x, 0.f, w), std::clamp(p.y, 0.f, h)}; }
  Vec2 randomPos() { return {rnd(0, w), rnd(0, h)}; }
  int count(Kind k) const { return counts[k]; }
  void spawn(std::unique_ptr<Entity> e) { counts[e->kind()]++; newborn.push_back(std::move(e)); }

  Entity* nearest(const Entity& self, float range, Kind k) {
    Entity* best = nullptr; float bd = range;
    for (auto& e : ents) {
      if (!e->alive || e.get() == &self || e->kind() != k) continue;
      float d = (e->pos - self.pos).len();
      if (d < bd) { bd = d; best = e.get(); }
    }
    return best;
  }
  void recount() {
    counts[0] = counts[1] = counts[2] = 0;
    for (auto& e : ents) if (e->alive) counts[e->kind()]++;
  }
  void step(float dt);
};

class Plant : public Entity {
public:
  using Entity::Entity;
  Kind kind() const override { return PLANT; }
  float radius() const override { return 1.5f + energy * 0.12f; }
  void update(World& wd, float dt) override {
    energy = std::min(energy + dt * 0.9f * wd.plantGrowth, 20.f);
    if (energy > 14 && wd.count(PLANT) < wd.plantCap && wd.rnd(0, 1) < dt * 0.35f) {
      wd.spawn(std::make_unique<Plant>(wd.clampPos(pos + Vec2{wd.rnd(-40, 40), wd.rnd(-40, 40)}), 3.f));
      energy -= 5;
    }
  }
};

class Creature : public Entity {
protected:
  Genome g; Vec2 vel; float heading = 0;
  virtual Entity* pickTarget(World& w) = 0;
  virtual Entity* pickThreat(World&) { return nullptr; }
  virtual std::unique_ptr<Creature> offspring(World& w, float e) = 0;
  virtual float eatGain(const Entity& prey) const = 0;
  virtual float reproduceAt() const = 0;
  virtual float baseCost() const = 0;
  virtual bool atCap(World& w) const = 0;
public:
  Creature(Vec2 p, float e, Genome gn) : Entity(p, e), g(gn) {}
  const Genome& genome() const { return g; }

  void update(World& wd, float dt) override {
    energy -= dt * (baseCost() + g.speed * g.speed * 0.00012f + g.sense * 0.004f);
    if (energy <= 0) { alive = false; return; }
    Vec2 dir; float boost = 1.f;
    if (Entity* threat = pickThreat(wd)) { dir = (pos - threat->pos).norm(); boost = 1.3f; }
    else if (Entity* t = pickTarget(wd)) {
      Vec2 d = t->pos - pos;
      if (d.len() < radius() + t->radius() + 2) { energy += eatGain(*t); t->alive = false; }
      else dir = d.norm();
    }
    if (dir.len() < 1e-4f) {  // nothing to chase: wander
      heading += wd.rnd(-1.5f, 1.5f) * dt * 3;
      dir = Vec2{std::cos(heading), std::sin(heading)} * 0.5f;
    }
    vel = vel * 0.9f + dir * (g.speed * boost * 0.1f);
    Vec2 np = pos + vel * dt;
    if (np.x < 0 || np.x > wd.w) vel.x = -vel.x;
    if (np.y < 0 || np.y > wd.h) vel.y = -vel.y;
    pos = wd.clampPos(np);
    if (energy > reproduceAt() && !atCap(wd)) {
      energy *= 0.5f;
      wd.spawn(offspring(wd, energy));
    }
  }
};

class Herbivore : public Creature {
public:
  using Creature::Creature;
  Kind kind() const override { return HERBIVORE; }
  float radius() const override { return 3.f; }
protected:
  Entity* pickTarget(World& w) override { return w.nearest(*this, g.sense, PLANT); }
  Entity* pickThreat(World& w) override { return w.nearest(*this, g.sense * 0.7f, PREDATOR); }
  std::unique_ptr<Creature> offspring(World& w, float e) override {
    return std::make_unique<Herbivore>(pos, e, g.mutated(w.rng));
  }
  float eatGain(const Entity& p) const override { return 4.f + p.energy * 0.6f; }
  float reproduceAt() const override { return 52.f; }
  float baseCost() const override { return 0.6f; }
  bool atCap(World& w) const override { return w.count(HERBIVORE) >= w.herbCap; }
};

class Predator : public Creature {
public:
  using Creature::Creature;
  Kind kind() const override { return PREDATOR; }
  float radius() const override { return 4.5f; }
protected:
  Entity* pickTarget(World& w) override { return w.nearest(*this, g.sense * 1.3f, HERBIVORE); }
  std::unique_ptr<Creature> offspring(World& w, float e) override {
    return std::make_unique<Predator>(pos, e, g.mutated(w.rng));
  }
  float eatGain(const Entity&) const override { return 30.f; }
  float reproduceAt() const override { return 75.f; }
  float baseCost() const override { return 0.7f; }
  bool atCap(World& w) const override { return w.count(PREDATOR) >= w.predCap; }
};

void World::step(float dt) {
  for (auto& e : ents) if (e->alive) e->update(*this, dt);
  for (auto& n : newborn) ents.push_back(std::move(n));
  newborn.clear();
  ents.erase(std::remove_if(ents.begin(), ents.end(), [](auto& e) { return !e->alive; }), ents.end());
  recount();
  // gentle "rewilding" so the ecosystem never goes completely extinct
  if (counts[HERBIVORE] < 6) for (int i = 0; i < 6; i++) ents.push_back(std::make_unique<Herbivore>(randomPos(), 25.f, Genome{}));
  if (counts[PREDATOR] < 2 && counts[HERBIVORE] > 40) ents.push_back(std::make_unique<Predator>(randomPos(), 50.f, Genome{70, 90}));
  if (counts[PLANT] < 30) for (int i = 0; i < 20; i++) ents.push_back(std::make_unique<Plant>(randomPos(), 8.f));
}

// ---------- C API for JavaScript ----------
static std::unique_ptr<World> world;
static std::vector<float> buf;

static void addKind(int k, Vec2 p) {
  if (k == PLANT) world->ents.push_back(std::make_unique<Plant>(p, 8.f));
  else if (k == HERBIVORE) world->ents.push_back(std::make_unique<Herbivore>(p, 25.f, Genome{}));
  else world->ents.push_back(std::make_unique<Predator>(p, 50.f, Genome{70, 90}));
}

EXPORT void sim_init(float w, float h, unsigned seed, int plants, int herbs, int preds) {
  world = std::make_unique<World>(w, h, seed);
  for (int i = 0; i < plants; i++) addKind(PLANT, world->randomPos());
  for (int i = 0; i < herbs; i++) addKind(HERBIVORE, world->randomPos());
  for (int i = 0; i < preds; i++) addKind(PREDATOR, world->randomPos());
  world->recount();
}
EXPORT void sim_step(float dt) { if (world) world->step(dt); }
EXPORT void sim_add(int kind, float x, float y) { if (world) addKind(kind, world->clampPos({x, y})); }
EXPORT void sim_set_growth(float g) { if (world) world->plantGrowth = g; }
EXPORT int sim_count(int kind) { return world ? world->count((Kind)kind) : 0; }

// Packs every entity as 5 floats: kind, x, y, radius, speed-gene. Returns entity count.
EXPORT int sim_fill() {
  buf.clear();
  for (auto& e : world->ents) {
    float sp = 0;
    if (auto* c = dynamic_cast<Creature*>(e.get())) sp = c->genome().speed;
    buf.insert(buf.end(), {(float)e->kind(), e->pos.x, e->pos.y, e->radius(), sp});
  }
  return (int)(buf.size() / 5);
}
EXPORT float* sim_buffer() { return buf.data(); }

// Average gene value (0 = speed, 1 = sense) for a creature kind.
EXPORT float sim_avg_gene(int kind, int gene) {
  float s = 0; int n = 0;
  for (auto& e : world->ents)
    if (e->kind() == kind) if (auto* c = dynamic_cast<Creature*>(e.get())) { s += gene ? c->genome().sense : c->genome().speed; n++; }
  return n ? s / n : 0;
}

#ifdef NATIVE_TEST
#include <cstdio>
int main() {
  sim_init(1000, 600, 7, 250, 60, 6);
  for (int i = 0; i <= 6000; i++) {
    sim_step(1.f / 60);
    if (i % 1000 == 0) printf("t=%4ds plants=%d herb=%d pred=%d herbSpeed=%.1f\n", i / 60, sim_count(0), sim_count(1), sim_count(2), sim_avg_gene(1, 0));
  }
  printf("packed=%d\n", sim_fill());
}
#endif