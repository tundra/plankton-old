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
  static ObjectType<Point> *type() { return &kType; }
private:
  static Point *new_instance(Variant header, Factory* factory);
  void init(Object payload, Factory* factory);
  Variant to_plankton(Factory *factory);
  static ObjectType<Point> kType;
  int x_;
  int y_;
};

Point *Point::new_instance(Variant header, Factory* factory) {
  return new (*factory) Point(0, 0);
}

void Point::init(Object payload, Factory* factory) {
  x_ = payload.get_field("x").integer_value();
  y_ = payload.get_field("y").integer_value();
}

Variant Point::to_plankton(Factory *factory) {
  Object obj = factory->new_object();
  obj.set_header(type()->header());
  obj.set_field("x", x_);
  obj.set_field("y", y_);
  return obj;
}

template <> struct default_object_type<Point> {
  static ObjectType<Point> *get() { return Point::type(); }
};

ObjectType<Point> Point::kType("binary.Point",
    tclib::new_callback(Point::new_instance),
    tclib::new_callback(&Point::init),
    tclib::new_callback(&Point::to_plankton));

TEST(marshal, object_type) {
  Arena arena;
  Object obj = arena.new_object();
  obj.set_header("binary.Point");
  obj.set_field("x", 10);
  obj.set_field("y", 18);
  Native value = Point::type()->get_initial_instance(obj.header(), &arena);
  Point::type()->get_complete_instance(value, obj, &arena);
  Point *p = value.as(Point::type());
  ASSERT_TRUE(p != NULL);
  ASSERT_EQ(10, p->x());
  ASSERT_EQ(18, p->y());
}

TEST(marshal, registry) {
  TypeRegistry registry;
  registry.register_type(Point::type());
  ASSERT_TRUE(registry.resolve_type("binary.Point") == Point::type());
  char other[13] = "binary.Point";
  ASSERT_TRUE(registry.resolve_type(other) == Point::type());
  ASSERT_TRUE(registry.resolve_type("blah") == NULL);
}

TEST(marshal, simple_auto_object) {
  Arena arena;
  Object obj = arena.new_object();
  obj.set_header("binary.Point");
  obj.set_field("x", 11);
  obj.set_field("y", 12);
  BinaryWriter out;
  out.write(obj);
  TypeRegistry registry;
  registry.register_type(Point::type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  Point *pnt = value.as(Point::type());
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
  static ObjectType<Rect> *type() { return &kType; }
private:
  static Rect *new_instance(Variant header, Factory* factory);
  void init(Object payload, Factory* factory);
  Variant to_plankton(Factory *factory);
  static ObjectType<Rect> kType;
  Point *top_left_;
  Point *bottom_right_;
};

Rect *Rect::new_instance(Variant header, Factory* factory) {
  return new (*factory) Rect(NULL, NULL);
}

void Rect::init(Object payload, Factory* factory) {
  top_left_ = payload.get_field("top_left").native_as(Point::type());
  bottom_right_ = payload.get_field("bottom_right").native_as(Point::type());
}

Variant Rect::to_plankton(Factory *factory) {
  Object obj = factory->new_object(type());
  obj.set_field("top_left", factory->new_native(top_left_));
  obj.set_field("bottom_right", factory->new_native(bottom_right_));
  return obj;
}

template <> struct default_object_type<Rect> {
  static ObjectType<Rect> *get() { return Rect::type(); }
};

ObjectType<Rect> Rect::kType("binary.Rect",
    tclib::new_callback(Rect::new_instance),
    tclib::new_callback(&Rect::init),
    tclib::new_callback(&Rect::to_plankton));

TEST(marshal, complex_auto_object) {
  Arena arena;
  Object top_left = arena.new_object();
  top_left.set_header("binary.Point");
  top_left.set_field("x", 13);
  top_left.set_field("y", 14);
  Object bottom_right = arena.new_object();
  bottom_right.set_header("binary.Point");
  bottom_right.set_field("x", 15);
  bottom_right.set_field("y", 16);
  Object obj = arena.new_object();
  obj.set_header("binary.Rect");
  obj.set_field("top_left", top_left);
  obj.set_field("bottom_right", bottom_right);
  BinaryWriter out;
  out.write(obj);
  TypeRegistry registry;
  registry.register_type(Point::type());
  registry.register_type(Rect::type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.as(Point::type()) == NULL);
  Rect *rect = value.as(Rect::type());
  ASSERT_EQ(13, rect->top_left()->x());
  ASSERT_EQ(14, rect->top_left()->y());
  ASSERT_EQ(15, rect->bottom_right()->x());
  ASSERT_EQ(16, rect->bottom_right()->y());
}

TEST(marshal, invalid_auto_object) {
  Arena arena;
  Object top_left = arena.new_object();
  top_left.set_header("binary.Point");
  top_left.set_field("x", 13);
  top_left.set_field("y", 14);
  Object obj = arena.new_object();
  obj.set_header("binary.Rect");
  obj.set_field("top_left", top_left);
  BinaryWriter out;
  out.write(obj);
  TypeRegistry registry;
  registry.register_type(Point::type());
  registry.register_type(Rect::type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.as(Point::type()) == NULL);
  Rect *rect = value.as(Rect::type());
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
  registry.register_type(Point::type());
  registry.register_type(Rect::type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.is_native());
  Point *p2 = value.as(Point::type());
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
  registry.register_type(Point::type());
  registry.register_type(Rect::type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.is_native());
  Rect *r2 = value.as(Rect::type());
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
  registry.register_type(Point::type());
  registry.register_type(Rect::type());
  BinaryReader in(&arena);
  in.set_type_registry(&registry);
  Native value = in.parse(*out, out.size());
  ASSERT_TRUE(value.is_native());
  Rect *r2 = value.as(Rect::type());
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
