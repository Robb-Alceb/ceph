// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2014 Adam Crume <adamcrume@gmail.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include "common/escape.h"
#include "gtest/gtest.h"
#include <stdint.h>
#include <boost/foreach.hpp>
#include "rbd_replay/Deser.hpp"
#include "rbd_replay/ImageNameMap.hpp"
#include "rbd_replay/rbd_loc.hpp"


using rbd_replay::ImageNameMap;
using rbd_replay::NameMap;
using rbd_replay::rbd_loc;

std::ostream& operator<<(std::ostream& o, const ImageName& name) {
  return o << "('" << name.pool() << "', '" << name.image() << "', '" << name.snap() << "')";
}

static ImageName bad_mapping(ImageName input, std::string output) {
  return ImageName("xxx", "xxx", "xxx");
}

static void add_mapping(ImageNameMap *map, std::string mapping_string) {
  ImageNameMap::Mapping mapping;
  if (!map->parse_mapping(mapping_string, &mapping)) {
    ASSERT_TRUE(false) << "Failed to parse mapping string '" << mapping_string << "'";
  }
  map->add_mapping(mapping);
}

static void add_mapping(NameMap *map, std::string mapping_string) {
  NameMap::Mapping mapping;
  if (!map->parse_mapping(mapping_string, &mapping)) {
    ASSERT_TRUE(false) << "Failed to parse mapping string '" << mapping_string << "'";
  }
  map->add_mapping(mapping);
}

TEST(RBDReplay, Deser) {
  const char data[] = {1, 2, 3, 4, 0, 0, 0, 5, 'h', 'e', 'l', 'l', 'o', 1, 0};
  const std::string s(data, sizeof(data));
  std::istringstream iss(s);
  rbd_replay::Deser deser(iss);
  EXPECT_EQ(false, deser.eof());
  EXPECT_EQ(0x01020304u, deser.read_uint32_t());
  EXPECT_EQ(false, deser.eof());
  EXPECT_EQ("hello", deser.read_string());
  EXPECT_EQ(false, deser.eof());
  EXPECT_EQ(true, deser.read_bool());
  EXPECT_EQ(false, deser.eof());
  EXPECT_EQ(false, deser.read_bool());
  EXPECT_EQ(false, deser.eof());
  deser.read_uint8_t();
  EXPECT_EQ(true, deser.eof());
}

TEST(RBDReplay, ImageNameMap) {
  ImageNameMap m;
  m.set_bad_mapping_fallback(bad_mapping);
  add_mapping(&m, "x@y=y@x");
  add_mapping(&m, "a\\=b@c=h@i");
  add_mapping(&m, "a@b\\=c=j@k");
  add_mapping(&m, "a\\\\@b@c=d@e");
  add_mapping(&m, "a@b\\\\@c=f@g");
  add_mapping(&m, "image@snap_(.*)=image_$1@");
  add_mapping(&m, "bad=@@@");
  EXPECT_EQ(ImageName("", "y", "x"), m.map(ImageName("", "x", "y")));
  EXPECT_EQ(ImageName("", "h", "i"), m.map(ImageName("", "a=b", "c")));
  EXPECT_EQ(ImageName("", "j", "k"), m.map(ImageName("", "a", "b=c")));
  EXPECT_EQ(ImageName("", "d", "e"), m.map(ImageName("", "a@b", "c")));
  EXPECT_EQ(ImageName("", "f", "g"), m.map(ImageName("", "a", "b@c")));
  EXPECT_EQ(ImageName("", "image_1", ""), m.map(ImageName("", "image", "snap_1")));
  EXPECT_EQ(ImageName("xxx", "xxx", "xxx"), m.map(ImageName("", "bad", "")));
}

TEST(RBDReplay, NameMap) {
  NameMap m;
  add_mapping(&m, "x=y");
  add_mapping(&m, "image_(.*)=$1_image");
  EXPECT_EQ("y", m.map("x"));
  EXPECT_EQ("ab_image", m.map("image_ab"));
  EXPECT_EQ("asdf", m.map("asdf"));
}

TEST(RBDReplay, rbd_loc_str) {
  EXPECT_EQ("", rbd_loc("", "", "").str());
  EXPECT_EQ("a/", rbd_loc("a", "", "").str());
  EXPECT_EQ("b", rbd_loc("", "b", "").str());
  EXPECT_EQ("a/b", rbd_loc("a", "b", "").str());
  EXPECT_EQ("@c", rbd_loc("", "", "c").str());
  EXPECT_EQ("a/@c", rbd_loc("a", "", "c").str());
  EXPECT_EQ("b@c", rbd_loc("", "b", "c").str());
  EXPECT_EQ("a/b@c", rbd_loc("a", "b", "c").str());
  EXPECT_EQ("a\\@x/b\\@y@c\\@z", rbd_loc("a@x", "b@y", "c@z").str());
  EXPECT_EQ("a\\/x/b\\/y@c\\/z", rbd_loc("a/x", "b/y", "c/z").str());
  EXPECT_EQ("a\\\\x/b\\\\y@c\\\\z", rbd_loc("a\\x", "b\\y", "c\\z").str());
}

TEST(RBDReplay, rbd_loc_parse) {
  rbd_loc m("x", "y", "z");

  EXPECT_EQ(true, m.parse(""));
  EXPECT_EQ("", m.pool);
  EXPECT_EQ("", m.image);
  EXPECT_EQ("", m.snap);

  EXPECT_EQ(true, m.parse("a/"));
  EXPECT_EQ("a", m.pool);
  EXPECT_EQ("", m.image);
  EXPECT_EQ("", m.snap);

  EXPECT_EQ(true, m.parse("b"));
  EXPECT_EQ("", m.pool);
  EXPECT_EQ("b", m.image);
  EXPECT_EQ("", m.snap);

  EXPECT_EQ(true, m.parse("a/b"));
  EXPECT_EQ("a", m.pool);
  EXPECT_EQ("b", m.image);
  EXPECT_EQ("", m.snap);

  EXPECT_EQ(true, m.parse("@c"));
  EXPECT_EQ("", m.pool);
  EXPECT_EQ("", m.image);
  EXPECT_EQ("c", m.snap);

  EXPECT_EQ(true, m.parse("a/@c"));
  EXPECT_EQ("a", m.pool);
  EXPECT_EQ("", m.image);
  EXPECT_EQ("c", m.snap);

  EXPECT_EQ(true, m.parse("b@c"));
  EXPECT_EQ("", m.pool);
  EXPECT_EQ("b", m.image);
  EXPECT_EQ("c", m.snap);

  EXPECT_EQ(true, m.parse("a/b@c"));
  EXPECT_EQ("a", m.pool);
  EXPECT_EQ("b", m.image);
  EXPECT_EQ("c", m.snap);

  EXPECT_EQ(true, m.parse("a\\@x/b\\@y@c\\@z"));
  EXPECT_EQ("a@x", m.pool);
  EXPECT_EQ("b@y", m.image);
  EXPECT_EQ("c@z", m.snap);

  EXPECT_EQ(true, m.parse("a\\/x/b\\/y@c\\/z"));
  EXPECT_EQ("a/x", m.pool);
  EXPECT_EQ("b/y", m.image);
  EXPECT_EQ("c/z", m.snap);

  EXPECT_EQ(true, m.parse("a\\\\x/b\\\\y@c\\\\z"));
  EXPECT_EQ("a\\x", m.pool);
  EXPECT_EQ("b\\y", m.image);
  EXPECT_EQ("c\\z", m.snap);

  EXPECT_EQ(false, m.parse("a@b@c"));
  EXPECT_EQ(false, m.parse("a/b/c"));
  EXPECT_EQ(false, m.parse("a@b/c"));
}
