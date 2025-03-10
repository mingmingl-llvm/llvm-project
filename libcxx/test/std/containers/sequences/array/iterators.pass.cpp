//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// <array>

// iterator begin() noexcept;                         // constexpr in C++17
// const_iterator begin() const noexcept;             // constexpr in C++17
// iterator end() noexcept;                           // constexpr in C++17
// const_iterator end() const noexcept;               // constexpr in C++17
//
// reverse_iterator rbegin() noexcept;                // constexpr in C++17
// const_reverse_iterator rbegin() const noexcept;    // constexpr in C++17
// reverse_iterator rend() noexcept;                  // constexpr in C++17
// const_reverse_iterator rend() const noexcept;      // constexpr in C++17
//
// const_iterator cbegin() const noexcept;            // constexpr in C++17
// const_iterator cend() const noexcept;              // constexpr in C++17
// const_reverse_iterator crbegin() const noexcept;   // constexpr in C++17
// const_reverse_iterator crend() const noexcept;     // constexpr in C++17

#include <array>
#include <iterator>
#include <cassert>

#include "test_macros.h"

struct NoDefault {
  TEST_CONSTEXPR NoDefault(int) {}
};

template <class T>
TEST_CONSTEXPR_CXX17 void check_noexcept(T& c) {
  ASSERT_NOEXCEPT(c.begin());
  ASSERT_NOEXCEPT(c.end());
  ASSERT_NOEXCEPT(c.cbegin());
  ASSERT_NOEXCEPT(c.cend());
  ASSERT_NOEXCEPT(c.rbegin());
  ASSERT_NOEXCEPT(c.rend());
  ASSERT_NOEXCEPT(c.crbegin());
  ASSERT_NOEXCEPT(c.crend());

  const T& cc = c;
  (void)cc;
  ASSERT_NOEXCEPT(cc.begin());
  ASSERT_NOEXCEPT(cc.end());
  ASSERT_NOEXCEPT(cc.rbegin());
  ASSERT_NOEXCEPT(cc.rend());
}

TEST_CONSTEXPR_CXX17 bool tests() {
  {
    typedef std::array<int, 5> C;
    C array = {};
    check_noexcept(array);
    typename C::iterator i       = array.begin();
    typename C::const_iterator j = array.cbegin();
    assert(i == j);
  }
  {
    typedef std::array<int, 0> C;
    C array = {};
    check_noexcept(array);
    typename C::iterator i       = array.begin();
    typename C::const_iterator j = array.cbegin();
    assert(i == j);
  }

  {
    typedef std::array<int, 0> C;
    C array = {};
    check_noexcept(array);
    typename C::iterator i       = array.begin();
    typename C::const_iterator j = array.cbegin();
    assert(i == array.end());
    assert(j == array.cend());
  }
  {
    typedef std::array<int, 1> C;
    C array = {1};
    check_noexcept(array);
    typename C::iterator i = array.begin();
    assert(*i == 1);
    assert(&*i == array.data());
    *i = 99;
    assert(array[0] == 99);
  }
  {
    typedef std::array<int, 2> C;
    C array = {1, 2};
    check_noexcept(array);
    typename C::iterator i = array.begin();
    assert(*i == 1);
    assert(&*i == array.data());
    *i = 99;
    assert(array[0] == 99);
    assert(array[1] == 2);
  }
  {
    typedef std::array<double, 3> C;
    C array = {1, 2, 3.5};
    check_noexcept(array);
    typename C::iterator i = array.begin();
    assert(*i == 1);
    assert(&*i == array.data());
    *i = 5.5;
    assert(array[0] == 5.5);
    assert(array[1] == 2.0);
  }
  {
    typedef std::array<NoDefault, 0> C;
    C array                 = {};
    typename C::iterator ib = array.begin();
    typename C::iterator ie = array.end();
    assert(ib == ie);
  }

#if TEST_STD_VER >= 14
  { // N3644 testing
    {
      typedef std::array<int, 5> C;
      C::iterator ii1{}, ii2{};
      C::iterator ii4 = ii1;
      C::const_iterator cii{};
      assert(ii1 == ii2);
      assert(ii1 == ii4);
      assert(ii1 == cii);

      assert(!(ii1 != ii2));
      assert(!(ii1 != cii));

      C c = {};
      check_noexcept(c);
      assert(c.begin() == std::begin(c));
      assert(c.cbegin() == std::cbegin(c));
      assert(c.rbegin() == std::rbegin(c));
      assert(c.crbegin() == std::crbegin(c));
      assert(c.end() == std::end(c));
      assert(c.cend() == std::cend(c));
      assert(c.rend() == std::rend(c));
      assert(c.crend() == std::crend(c));

      assert(std::begin(c) != std::end(c));
      assert(std::rbegin(c) != std::rend(c));
      assert(std::cbegin(c) != std::cend(c));
      assert(std::crbegin(c) != std::crend(c));

#  if TEST_STD_VER >= 20
      // P1614 + LWG3352
      std::same_as<std::strong_ordering> decltype(auto) r1 = ii1 <=> ii2;
      assert(r1 == std::strong_ordering::equal);

      std::same_as<std::strong_ordering> decltype(auto) r2 = cii <=> ii2;
      assert(r2 == std::strong_ordering::equal);
#  endif
    }
    {
      typedef std::array<int, 0> C;
      C::iterator ii1{}, ii2{};
      C::iterator ii4 = ii1;
      C::const_iterator cii{};
      assert(ii1 == ii2);
      assert(ii1 == ii4);

      assert(!(ii1 != ii2));

      assert((ii1 == cii));
      assert((cii == ii1));
      assert(!(ii1 != cii));
      assert(!(cii != ii1));
      assert(!(ii1 < cii));
      assert(!(cii < ii1));
      assert((ii1 <= cii));
      assert((cii <= ii1));
      assert(!(ii1 > cii));
      assert(!(cii > ii1));
      assert((ii1 >= cii));
      assert((cii >= ii1));
      assert(cii - ii1 == 0);
      assert(ii1 - cii == 0);

      C c = {};
      check_noexcept(c);
      assert(c.begin() == std::begin(c));
      assert(c.cbegin() == std::cbegin(c));
      assert(c.rbegin() == std::rbegin(c));
      assert(c.crbegin() == std::crbegin(c));
      assert(c.end() == std::end(c));
      assert(c.cend() == std::cend(c));
      assert(c.rend() == std::rend(c));
      assert(c.crend() == std::crend(c));

      assert(std::begin(c) == std::end(c));
      assert(std::rbegin(c) == std::rend(c));
      assert(std::cbegin(c) == std::cend(c));
      assert(std::crbegin(c) == std::crend(c));

#  if TEST_STD_VER >= 20
      // P1614 + LWG3352
      std::same_as<std::strong_ordering> decltype(auto) r1 = ii1 <=> ii2;
      assert(r1 == std::strong_ordering::equal);

      std::same_as<std::strong_ordering> decltype(auto) r2 = cii <=> ii2;
      assert(r2 == std::strong_ordering::equal);
#  endif
    }
  }
#endif
  return true;
}

int main(int, char**) {
  tests();
#if TEST_STD_VER >= 17
  static_assert(tests(), "");
#endif
  return 0;
}
