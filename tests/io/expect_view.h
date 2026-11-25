#ifndef OPENTLV_TEST_IO_EXPECT_VIEW_H
#define OPENTLV_TEST_IO_EXPECT_VIEW_H

#include <opentlv/core/element.h>
#include <gtest/gtest.h>

namespace opentlv_test {

inline void ExpectView(const tlv_element_t &expected, const tlv_element_t &actual)
{
    EXPECT_EQ(expected.tag.data, actual.tag.data);
    EXPECT_EQ(expected.tag.size, actual.tag.size);
    EXPECT_EQ(expected.value.data, actual.value.data);
    EXPECT_EQ(expected.value.size, actual.value.size);
}

} // namespace opentlv_test

#endif
