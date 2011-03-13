/* -*- coding: utf-8 -*-
 * ----------------------------------------------------------------------
 * Copyright © 2009-2010, RedJack, LLC.
 * All rights reserved.
 *
 * Please see the LICENSE.txt file in this distribution for license
 * details.
 * ----------------------------------------------------------------------
 */

#include <stdlib.h>
#include <stdio.h>

#include <check.h>

#include "libcork/core/allocator.h"
#include "libcork/core/types.h"
#include "libcork/ds/managed-buffer.h"


/*-----------------------------------------------------------------------
 * Helper functions
 */

struct flag_buffer {
    cork_managed_buffer_t  parent;
    bool  *flag;
};

static void
set_flag_on_free(cork_managed_buffer_t *mbuf)
{
    struct flag_buffer  *fbuf =
        cork_container_of(mbuf, struct flag_buffer, parent);
    *fbuf->flag = true;
    cork_delete(mbuf->alloc, struct flag_buffer, fbuf);
}

static cork_managed_buffer_t *
flag_buffer_new(cork_allocator_t *alloc,
                const void *buf, size_t size,
                bool *flag)
{
    struct flag_buffer  *fbuf = cork_new(alloc, struct flag_buffer);
    fbuf->parent.buf = buf;
    fbuf->parent.size = size;
    fbuf->parent.free = set_flag_on_free;
    fbuf->parent.ref_count = 1;
    fbuf->parent.alloc = alloc;
    fbuf->flag = flag;
    return &fbuf->parent;
}



/*-----------------------------------------------------------------------
 * Buffer reference counting
 */

START_TEST(test_managed_buffer_refcount)
{
    cork_allocator_t  *alloc = cork_allocator_new_debug();

    bool  flag = false;

    /*
     * Make a bunch of references, unreference them all, and then
     * verify that the free function got called.
     */

    cork_managed_buffer_t  *pb0 = flag_buffer_new(alloc, NULL, 0, &flag);
    cork_managed_buffer_t  *pb1 = cork_managed_buffer_ref(pb0);
    cork_managed_buffer_t  *pb2 = cork_managed_buffer_ref(pb0);
    cork_managed_buffer_t  *pb3 = cork_managed_buffer_ref(pb2);

    cork_managed_buffer_unref(pb0);
    cork_managed_buffer_unref(pb1);
    cork_managed_buffer_unref(pb2);
    cork_managed_buffer_unref(pb3);

    fail_unless(flag,
                "Packet buffer free function never called.");

    cork_allocator_free(alloc);
}
END_TEST


START_TEST(test_managed_buffer_bad_refcount)
{
    cork_allocator_t  *alloc = cork_allocator_new_debug();

    bool  flag = false;

    /*
     * Make a bunch of references, forget to unreference one of them,
     * and then verify that the free function didn't called.
     */

    cork_managed_buffer_t  *pb0 = flag_buffer_new(alloc, NULL, 0, &flag);
    cork_managed_buffer_t  *pb1 = cork_managed_buffer_ref(pb0);
    cork_managed_buffer_t  *pb2 = cork_managed_buffer_ref(pb0);
    cork_managed_buffer_t  *pb3 = cork_managed_buffer_ref(pb2);

    cork_managed_buffer_unref(pb0);
    cork_managed_buffer_unref(pb1);
    cork_managed_buffer_unref(pb2);
    /* cork_managed_buffer_unref(pb3);   OH NO! */
    (void) pb3;

    fail_if(flag,
            "Packet buffer free function was called unexpectedly.");

    /* free the buffer here to quiet valgrind */
    cork_managed_buffer_unref(pb3);
    cork_allocator_free(alloc);
}
END_TEST


/*-----------------------------------------------------------------------
 * Slicing
 */

START_TEST(test_slice)
{
    /*
     * Try to slice a NULL buffer.
     */

    cork_slice_t  ps1;

    fail_if(cork_managed_buffer_slice(&ps1, NULL, 0, 0),
            "Shouldn't be able to slice a NULL buffer");
    fail_if(cork_managed_buffer_slice_offset(&ps1, NULL, 0),
            "Shouldn't be able to slice a NULL buffer");

    fail_if(cork_slice_slice(&ps1, NULL, 0, 0),
            "Shouldn't be able to slice a NULL slice");
    fail_if(cork_slice_slice_offset(&ps1, NULL, 0),
            "Shouldn't be able to slice a NULL slice");

    cork_slice_finish(&ps1);
}
END_TEST


/*-----------------------------------------------------------------------
 * Slice reference counting
 */

START_TEST(test_slice_refcount)
{
    cork_allocator_t  *alloc = cork_allocator_new_debug();

    bool  flag = false;

    /*
     * Make a bunch of slices, finish them all, and then verify that
     * the free function got called.
     */

    static char  *BUF =
        "abcdefg";
    static size_t  LEN = 7;

    cork_managed_buffer_t  *pb = flag_buffer_new(alloc, BUF, LEN, &flag);

    cork_slice_t  ps1;
    cork_slice_t  ps2;
    cork_slice_t  ps3;

    cork_managed_buffer_slice(&ps1, pb, 0, 7);
    cork_managed_buffer_slice(&ps2, pb, 1, 1);
    cork_managed_buffer_slice(&ps3, pb, 4, 3);

    cork_managed_buffer_unref(pb);
    cork_slice_finish(&ps1);
    cork_slice_finish(&ps2);
    cork_slice_finish(&ps3);

    fail_unless(flag,
                "Packet buffer free function never called.");

    cork_allocator_free(alloc);
}
END_TEST


START_TEST(test_slice_bad_refcount)
{
    cork_allocator_t  *alloc = cork_allocator_new_debug();

    bool  flag = false;

    /*
     * Make a bunch of slices, forget to finish one of them, and then
     * verify that the free function didn't called.
     */

    static char  *BUF =
        "abcdefg";
    static size_t  LEN = 7;

    cork_managed_buffer_t  *pb = flag_buffer_new(alloc, BUF, LEN, &flag);

    cork_slice_t  ps1;
    cork_slice_t  ps2;
    cork_slice_t  ps3;

    cork_managed_buffer_slice(&ps1, pb, 0, 7);
    cork_managed_buffer_slice(&ps2, pb, 1, 1);
    cork_managed_buffer_slice(&ps3, pb, 4, 3);

    cork_managed_buffer_unref(pb);
    cork_slice_finish(&ps1);
    cork_slice_finish(&ps2);
    /* cork_slice_finish(&ps3);   OH NO! */

    fail_if(flag,
            "Packet buffer free function was called unexpectedly.");

    /* free the slice here to quiet valgrind */
    cork_slice_finish(&ps3);
    cork_allocator_free(alloc);
}
END_TEST


/*-----------------------------------------------------------------------
 * Slice equality
 */

START_TEST(test_slice_equals_01)
{
    cork_allocator_t  *alloc = cork_allocator_new_debug();

    /*
     * Make a bunch of slices, finish them all, and then verify that
     * the free function got called.
     */

    static char  *BUF =
        "abcdefg";
    static size_t  LEN = 7;

    cork_managed_buffer_t  *pb = cork_managed_buffer_new_copy(alloc, BUF, LEN);

    cork_slice_t  ps1;
    cork_slice_t  ps2;

    cork_managed_buffer_slice_offset(&ps1, pb, 0);
    cork_managed_buffer_slice(&ps2, pb, 0, LEN);

    fail_unless(cork_slice_equal(&ps1, &ps2),
                "Packet slices aren't equal");

    cork_managed_buffer_unref(pb);
    cork_slice_finish(&ps1);
    cork_slice_finish(&ps2);

    cork_allocator_free(alloc);
}
END_TEST


START_TEST(test_slice_equals_02)
{
    cork_allocator_t  *alloc = cork_allocator_new_debug();

    /*
     * Make a bunch of slices, finish them all, and then verify that
     * the free function got called.
     */

    static char  *BUF =
        "abcdefg";
    static size_t  LEN = 7;

    cork_managed_buffer_t  *pb = cork_managed_buffer_new_copy(alloc, BUF, LEN);

    cork_slice_t  ps1;
    cork_slice_t  ps2;
    cork_slice_t  ps3;

    cork_managed_buffer_slice(&ps1, pb, 3, 3);

    cork_managed_buffer_slice_offset(&ps2, pb, 1);
    cork_slice_slice(&ps3, &ps2, 2, 3);

    fail_unless(cork_slice_equal(&ps1, &ps3),
                "Packet slices aren't equal");

    cork_managed_buffer_unref(pb);
    cork_slice_finish(&ps1);
    cork_slice_finish(&ps2);
    cork_slice_finish(&ps3);

    cork_allocator_free(alloc);
}
END_TEST


/*-----------------------------------------------------------------------
 * Testing harness
 */

Suite *
test_suite()
{
    Suite  *s = suite_create("managed-buffer");

    TCase  *tc_buffer_refcount = tcase_create("managed-buffer-refcount");
    tcase_add_test(tc_buffer_refcount, test_managed_buffer_refcount);
    tcase_add_test(tc_buffer_refcount, test_managed_buffer_bad_refcount);
    suite_add_tcase(s, tc_buffer_refcount);

    TCase  *tc_slice = tcase_create("slice");
    tcase_add_test(tc_slice, test_slice);
    suite_add_tcase(s, tc_slice);

    TCase  *tc_slice_refcount = tcase_create("slice-refcount");
    tcase_add_test(tc_slice_refcount, test_slice_refcount);
    tcase_add_test(tc_slice_refcount, test_slice_bad_refcount);
    suite_add_tcase(s, tc_slice_refcount);

    TCase  *tc_slice_equality = tcase_create("slice-equality");
    tcase_add_test(tc_slice_equality, test_slice_equals_01);
    tcase_add_test(tc_slice_equality, test_slice_equals_02);
    suite_add_tcase(s, tc_slice_equality);

    return s;
}


int
main(int argc, const char **argv)
{
    int  number_failed;
    Suite  *suite = test_suite();
    SRunner  *runner = srunner_create(suite);

    srunner_run_all(runner, CK_NORMAL);
    number_failed = srunner_ntests_failed(runner);
    srunner_free(runner);

    return (number_failed == 0)? EXIT_SUCCESS: EXIT_FAILURE;
}
