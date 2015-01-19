//- Copyright 2015 the Neutrino authors (see AUTHORS).
//- Licensed under the Apache License, Version 2.0 (see LICENSE).

#include "test/asserts.hh"
#include "test/unittest.hh"
#include "plankton-binary.hh"
#include "marshal-inl.hh"

using namespace plankton;

class Point {
public:
  Point(int x, int y)
    : x_(x)
    , y_(y) { }
  int x() { return x_; }
  int y() { return y_; }
  static SeedType<Point> *seed_type() { return &kType; }
private:
  static Point *new_instance(Variant header, Factory* factory);
  void init(Seed payload, Factory* factory);
  Variant to_seed(Factory *factory);
  static SeedType<Point> kType;
  int x_;
  int y_;
};

Point *Point::new_instance(Variant header, Factory* factory) {
  return new (*factory) Point(0, 0);
}

void Point::init(Seed payload, Factory* factory) {
  x_ = payload.get_field("x").integer_value();
  y_ = payload.get_field("y").integer_value();
}

Variant Point::to_seed(Factory *factory) {
  Seed obj = factory->new_seed(Point::seed_type());
  obj.set_field("x", x_);
  obj.set_field("y", y_);
  return obj;
}

SeedType<Point> Point::kType("binary.Point",
    tclib::new_callback(Point::new_instance),
    tclib::new_callback(&Point::init),
    tclib::new_callback(&Point::to_seed));

TEST(marshal, seed_type) {
  Arena arena;
  Seed obj = arena.new_seed();
  obj.set_header("binary.Point");
  obj.set_field("x", 10);
  obj.set_field("y", 18);
  Native value = Point::seed_type()->get_initial_instance(obj.header(), &arena);
  Point::seed_type()->get_complete_instance(value, obj, &arena);
  Point *p = value.as(Point::seed_type());
  ASSERT_TRUE(p != NULL);
  ASSERT_EQ(10, p->x());
  ASSERT_EQ(18, p->y());
}

TEST(marshal, registry) {
  TypeRegistry registry;
  registry.register_type(Point::seed_type());
  ASSERT_TRUE(registry.resolve_type("binary.Point") == Point::seed_type());
  char other[13] = "binary.Point";
  ASSERT_TRUE(registry.resolve_type(other) == Point::seed_type());
  ASSERT_TRUE(registry.resolve_type("blah") == NULL);
}

TEST(marshal, simple_auto_object) {
  Arena arena;
  Seed obj = arena.new_seed();
  obj.set_header("binary.Point");
  obj.set_field("x", 11);
  obj.set_field("y", 12);
  BinaryWriter out;
  out.write(obj);
  TypeRegistry registry;
  registry.register_type(Point::seed_type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  Point *pnt = value.as(Point::seed_type());
  ASSERT_FALSE(pnt == NULL);
  ASSERT_EQ(11, pnt->x());
  ASSERT_EQ(12, pnt->y());
}

class Rect {
public:
  Rect(Point *top_left, Point *bottom_right)
    : top_left_(top_left)
    , bottom_right_(bottom_right) { }
  Point *top_left() { return top_left_; }
  Point *bottom_right() { return bottom_right_; }
  static SeedType<Rect> *seed_type() { return &kType; }
private:
  static Rect *new_instance(Variant header, Factory* factory);
  void init(Seed payload, Factory* factory);
  Variant to_seed(Factory *factory);
  static SeedType<Rect> kType;
  Point *top_left_;
  Point *bottom_right_;
};

Rect *Rect::new_instance(Variant header, Factory* factory) {
  return new (factory) Rect(NULL, NULL);
}

void Rect::init(Seed payload, Factory* factory) {
  top_left_ = payload.get_field("top_left").native_as(Point::seed_type());
  bottom_right_ = payload.get_field("bottom_right").native_as(Point::seed_type());
}

Variant Rect::to_seed(Factory *factory) {
  Seed obj = factory->new_seed(seed_type());
  obj.set_field("top_left", factory->new_native(top_left_));
  obj.set_field("bottom_right", factory->new_native(bottom_right_));
  return obj;
}

SeedType<Rect> Rect::kType("binary.Rect",
    tclib::new_callback(Rect::new_instance),
    tclib::new_callback(&Rect::init),
    tclib::new_callback(&Rect::to_seed));

TEST(marshal, complex_auto_object) {
  Arena arena;
  Seed top_left = arena.new_seed();
  top_left.set_header("binary.Point");
  top_left.set_field("x", 13);
  top_left.set_field("y", 14);
  Seed bottom_right = arena.new_seed();
  bottom_right.set_header("binary.Point");
  bottom_right.set_field("x", 15);
  bottom_right.set_field("y", 16);
  Seed obj = arena.new_seed();
  obj.set_header("binary.Rect");
  obj.set_field("top_left", top_left);
  obj.set_field("bottom_right", bottom_right);
  BinaryWriter out;
  out.write(obj);
  TypeRegistry registry;
  registry.register_type(Point::seed_type());
  registry.register_type(Rect::seed_type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.as(Point::seed_type()) == NULL);
  Rect *rect = value.as(Rect::seed_type());
  ASSERT_EQ(13, rect->top_left()->x());
  ASSERT_EQ(14, rect->top_left()->y());
  ASSERT_EQ(15, rect->bottom_right()->x());
  ASSERT_EQ(16, rect->bottom_right()->y());
}

TEST(marshal, invalid_auto_object) {
  Arena arena;
  Seed top_left = arena.new_seed();
  top_left.set_header("binary.Point");
  top_left.set_field("x", 13);
  top_left.set_field("y", 14);
  Seed obj = arena.new_seed();
  obj.set_header("binary.Rect");
  obj.set_field("top_left", top_left);
  BinaryWriter out;
  out.write(obj);
  TypeRegistry registry;
  registry.register_type(Point::seed_type());
  registry.register_type(Rect::seed_type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.as(Point::seed_type()) == NULL);
  Rect *rect = value.as(Rect::seed_type());
  ASSERT_EQ(13, rect->top_left()->x());
  ASSERT_EQ(14, rect->top_left()->y());
  ASSERT_EQ(NULL, rect->bottom_right());
}

TEST(marshal, simple_encode) {
  Point p(15, 16);
  Arena arena;
  Native n = arena.new_native(&p);
  BinaryWriter out;
  out.write(n);
  TypeRegistry registry;
  registry.register_type(Point::seed_type());
  registry.register_type(Rect::seed_type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.is_native());
  Point *p2 = value.as(Point::seed_type());
  ASSERT_EQ(15, p2->x());
  ASSERT_EQ(16, p2->y());
}

TEST(marshal, complex_encode) {
  Point top_left(17, 18);
  Point bottom_right(19, 20);
  Rect rect(&top_left, &bottom_right);
  Arena arena;
  Native n = arena.new_native(&rect);
  BinaryWriter out;
  out.write(n);
  TypeRegistry registry;
  registry.register_type(Point::seed_type());
  registry.register_type(Rect::seed_type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.is_native());
  Rect *r2 = value.as(Rect::seed_type());
  ASSERT_EQ(17, r2->top_left()->x());
  ASSERT_EQ(18, r2->top_left()->y());
  ASSERT_EQ(19, r2->bottom_right()->x());
  ASSERT_EQ(20, r2->bottom_right()->y());
}

TEST(marshal, partial_encode) {
  Point top_left(17, 18);
  Rect rect(&top_left, NULL);
  Arena arena;
  Native n = arena.new_native(&rect);
  BinaryWriter out;
  out.write(n);
  TypeRegistry registry;
  registry.register_type(Point::seed_type());
  registry.register_type(Rect::seed_type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.is_native());
  Rect *r2 = value.as(Rect::seed_type());
  ASSERT_EQ(17, r2->top_left()->x());
  ASSERT_EQ(18, r2->top_left()->y());
  ASSERT_EQ(NULL, r2->bottom_right());
}

TEST(marshal, variant_map) {
  VariantMap<int> ints;
  ASSERT_TRUE(ints["foo"] == NULL);
  ints.set("foo", 3);
  ASSERT_EQ(3, *ints["foo"]);
  ASSERT_TRUE(ints[Variant::yes()] == NULL);
  ints.set(Variant::yes(), 4);
  ASSERT_EQ(3, *ints["foo"]);
  ASSERT_EQ(4, *ints[Variant::yes()]);
  ints.set("foo", 5);
  ASSERT_EQ(5, *ints["foo"]);
  ASSERT_EQ(4, *ints[Variant::yes()]);
  ASSERT_TRUE(ints[Variant::null()] == NULL);
  ints.set(Variant::null(), 6);
  ASSERT_EQ(5, *ints["foo"]);
  ASSERT_EQ(4, *ints[Variant::yes()]);
  ASSERT_EQ(6, *ints[Variant::null()]);
  ints.set(Variant::null(), 7);
  ASSERT_EQ(5, *ints["foo"]);
  ASSERT_EQ(4, *ints[Variant::yes()]);
  ASSERT_EQ(7, *ints[Variant::null()]);
}

class A {
public:
  A(int *count) : count_(count) { inc(); }
  ~A() { dec(); }
  static A *new_instance(int *count, Variant header, Factory *factory);
  Variant to_seed(Factory *factory);
  void inc() { (*count_)++; }
  void dec() { (*count_)--; }
private:
  int *count_;
};

A *A::new_instance(int *count, Variant header, Factory *factory) {
  return factory->register_destructor(new (factory) A(count));
}

Variant A::to_seed(Factory *factory) {
  Seed seed = factory->new_seed();
  seed.set_header("A");
  return seed;
}

TEST(marshal, destruct) {
  int count = 0;
  SeedType<A> a_type("A",
      tclib::new_callback(A::new_instance, &count),
      tclib::empty_callback(),
      tclib::new_callback(&A::to_seed));
  BinaryWriter writer;
  {
    A a0(&count);
    Arena arena;
    writer.write(arena.new_native(&a0, &a_type));
  }
  ASSERT_EQ(0, count);
  {
    Arena arena;
    BinaryReader reader(&arena);
    TypeRegistry registry;
    registry.register_type(&a_type);
    reader.set_type_registry(&registry);
    ASSERT_EQ(0, count);
    Native value = reader.parse(*writer, writer.size());
    ASSERT_EQ(1, count);
    A *a1 = value.as(&a_type);
    ASSERT_FALSE(a1 == NULL);
    ASSERT_EQ(1, count);
  }
  ASSERT_EQ(0, count);
}
