// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "util/bitmap.h"

#include <gtest/gtest.h>

#include <iostream>

namespace starrocks {

class BitMapTest : public testing::Test {
public:
    BitMapTest() = default;
    ~BitMapTest() override = default;
};

TEST_F(BitMapTest, normal) {
    // bitmap size
    ASSERT_EQ(0, BitmapSize(0));
    ASSERT_EQ(1, BitmapSize(1));
    ASSERT_EQ(1, BitmapSize(8));
    ASSERT_EQ(2, BitmapSize(9));
}

} // namespace starrocks
