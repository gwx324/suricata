/* Copyright (C) 2007-2017 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 *
 * Tests for the content inspection engine.
 */

#include "../suricata-common.h"
#include "../decode.h"
#include "../flow.h"
#include "../detect.h"

#define TEST_HEADER                                     \
    ThreadVars tv;                                      \
    memset(&tv, 0, sizeof(tv));                         \
    Flow f;                                             \
    memset(&f, 0, sizeof(f));

#define TEST_RUN(buf, buflen, sig, match, steps)                                            \
{                                                                                           \
    DetectEngineCtx *de_ctx = DetectEngineCtxInit();                                        \
    FAIL_IF_NULL(de_ctx);                                                                   \
    DetectEngineThreadCtx *det_ctx = NULL;                                                  \
    char rule[2048];                                                                        \
    snprintf(rule, sizeof(rule), "alert tcp any any -> any any (%s sid:1; rev:1;)", (sig)); \
    Signature *s = DetectEngineAppendSig(de_ctx, rule);                                     \
    FAIL_IF_NULL(s);                                                                        \
    SigGroupBuild(de_ctx);                                                                  \
    DetectEngineThreadCtxInit(&tv, (void *)de_ctx, (void *)&det_ctx);                       \
    FAIL_IF_NULL(det_ctx);                                                                  \
    int r = DetectEngineContentInspection(de_ctx, det_ctx,                                  \
                s, s->sm_arrays[DETECT_SM_LIST_PMATCH], &f,                                 \
                (uint8_t *)(buf), (buflen), 0,                                              \
                DETECT_ENGINE_CONTENT_INSPECTION_MODE_PAYLOAD, NULL);                       \
    FAIL_IF_NOT(r == (match));                                                              \
    FAIL_IF_NOT(det_ctx->inspection_recursion_counter == (steps));                          \
    DetectEngineThreadCtxDeinit(&tv, det_ctx);                                              \
    DetectEngineCtxFree(de_ctx);                                                            \
}
#define TEST_FOOTER     \
    PASS

/** \test simple match with distance */
static int DetectEngineContentInspectionTest01(void) {
    TEST_HEADER;
    TEST_RUN("ab", 2, "content:\"a\"; content:\"b\";", true, 2);
    TEST_RUN("ab", 2, "content:\"a\"; content:\"b\"; distance:0; ", true, 2);
    TEST_RUN("ba", 2, "content:\"a\"; content:\"b\"; distance:0; ", false, 2);
    TEST_FOOTER;
}

/** \test simple match with pcre/R */
static int DetectEngineContentInspectionTest02(void) {
    TEST_HEADER;
    TEST_RUN("ab", 2, "content:\"a\"; pcre:\"/b/\";", true, 2);
    TEST_RUN("ab", 2, "content:\"a\"; pcre:\"/b/R\";", true, 2);
    TEST_RUN("ba", 2, "content:\"a\"; pcre:\"/b/R\";", false, 2);
    TEST_FOOTER;
}

/** \test simple recursion logic */
static int DetectEngineContentInspectionTest03(void) {
    TEST_HEADER;
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"c\";", true, 3);
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"d\";", false, 3);

    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; content:\"c\"; distance:0;", true, 3);
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; content:\"d\"; distance:0;", false, 6); // TODO should be 3?

    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"d\"; distance:0; within:1;", false, 5);

    // 5 steps: (1) a, (2) 1st b, (3) c not found, (4) 2nd b, (5) c found
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"c\"; distance:0; within:1;", true, 5);
    // 6 steps: (1) a, (2) 1st b, (3) c not found, (4) 2nd b, (5) c found, (6) bab
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"c\"; distance:0; within:1; content:\"bab\";", true, 6);
    // 6 steps: (1) a, (2) 1st b, (3) c not found, (4) 2nd b, (5) c found, (6) no not found
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"c\"; distance:0; within:1; content:\"no\";", false, 6);

    // 5 steps: (1) a, (2) 1st b, (3) c not found, (4) 2nd b, (5) c found
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; pcre:\"/^c$/R\";", true, 5);
    // 6 steps: (1) a, (2) 1st b, (3) c not found, (4) 2nd b, (5) c found, (6) bab
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; pcre:\"/^c$/R\"; content:\"bab\";", true, 6);
    // 6 steps: (1) a, (2) 1st b, (3) c not found, (4) 2nd b, (5) c found, (6) no not found
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; pcre:\"/^c$/R\"; content:\"no\";", false, 6);

    TEST_FOOTER;
}

/** \test pcre recursion logic */
static int DetectEngineContentInspectionTest04(void) {
    TEST_HEADER;
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"c\";", true, 3);
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"d\";", false, 3);

    // simple chain of pcre
    TEST_RUN("ababc", 5, "pcre:\"/^a/\"; pcre:\"/^b/R\"; pcre:\"/c/R\"; ", true, 3);
    TEST_RUN("ababc", 5, "pcre:\"/a/\"; pcre:\"/^b/R\"; pcre:\"/^c/R\"; ", true, 5);
    TEST_RUN("ababc", 5, "pcre:\"/^a/\"; pcre:\"/^b/R\"; pcre:\"/d/R\"; ", false, 3);
    TEST_RUN("ababc", 5, "pcre:\"/^a/\"; pcre:\"/^b/R\"; pcre:\"/c/R\"; pcre:\"/d/\"; ", false, 4);

    TEST_FOOTER;
}

/** \test multiple independent blocks recursion logic */
static int DetectEngineContentInspectionTest05(void) {
    TEST_HEADER;
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"c\";", true, 3);
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"d\";", false, 3);

    // first block 2: (1) a, (2) b
    // second block 3: (1) b, (2) c not found, (x) b continues within loop, (3) c found
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"b\"; content:\"c\"; distance:0; within:1;", true, 5);

    TEST_FOOTER;
}

/** \test isdataat recursion logic */
static int DetectEngineContentInspectionTest06(void) {
    TEST_HEADER;
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"c\";", true, 3);
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; content:\"d\";", false, 3);

    // 6 steps: (1) a, (2) 1st b, (3) c not found, (4) 2nd b, (5) c found, isdataat
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"c\"; distance:0; within:1; isdataat:!1,relative;", true, 6);
    TEST_RUN("ababc", 5, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"c\"; distance:0; within:1; isdataat:1,relative;", false, 6);

    TEST_RUN("ababcabc", 8, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"c\"; distance:0; within:1; isdataat:!1,relative;", true, 9);
    TEST_RUN("ababcabc", 8, "content:\"a\"; content:\"b\"; distance:0; within:1; content:\"c\"; distance:0; within:1; isdataat:1,relative;", true, 6);

    TEST_FOOTER;
}

/** \test extreme recursion */
static int DetectEngineContentInspectionTest07(void) {
    TEST_HEADER;
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcd", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; content:\"d\";", true, 4);
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcd", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; content:\"d\"; within:1; distance:0; ", true, 31);
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcx", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; content:\"d\"; within:1; distance:0; ", false, 31);

    TEST_RUN("abcabcabcabcabcabcabcabcabcabcx", 31, "content:\"a\"; content:\"b\"; distance:0; content:\"c\"; distance:0; content:\"d\"; distance:0; ", false, 286); // TODO should be 4?
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcx", 31, "content:\"a\"; content:\"b\"; distance:0; content:\"c\"; distance:0; pcre:\"/^d/R\"; ", false, 286); // TODO should be 4?
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcx", 31, "content:\"a\"; content:\"b\"; distance:0; content:\"c\"; distance:0; isdataat:!1,relative; ", false, 286); // TODO should be 4?
    TEST_RUN("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdx", 41,
            "content:\"a\"; content:\"b\"; distance:0; content:\"c\"; distance:0; content:\"d\"; distance:0; content:\"e\"; distance:0; ", false, 1001); // TODO should be 5?
    TEST_RUN("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdx", 41,
            "content:\"a\"; content:\"b\"; distance:0; content:\"c\"; distance:0; content:\"d\"; distance:0; pcre:\"/^e/R\"; ", false, 1001); // TODO should be 5?
    TEST_RUN("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdx", 41,
            "content:\"a\"; content:\"b\"; distance:0; content:\"c\"; distance:0; content:\"d\"; distance:0; isdataat:!1,relative; ", false, 1001); // TODO should be 5?

    TEST_RUN("abcabcabcabcabcabcabcabcabcabcd", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; pcre:\"/d/\";", true, 4);
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcd", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; pcre:\"/d/R\";", true, 4);
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcd", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; pcre:\"/^d/R\";", true, 31);

    TEST_RUN("abcabcabcabcabcabcabcabcabcabcx", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; pcre:\"/d/\";", false, 4);
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcx", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; pcre:\"/d/R\";", false, 31);
    TEST_RUN("abcabcabcabcabcabcabcabcabcabcx", 31, "content:\"a\"; content:\"b\"; within:1; distance:0; content:\"c\"; distance:0; within:1; pcre:\"/^d/R\";", false, 31);
    TEST_FOOTER;
}

void DetectEngineContentInspectionRegisterTests(void)
{
    UtRegisterTest("DetectEngineContentInspectionTest01",
                   DetectEngineContentInspectionTest01);
    UtRegisterTest("DetectEngineContentInspectionTest02",
                   DetectEngineContentInspectionTest02);
    UtRegisterTest("DetectEngineContentInspectionTest03",
                   DetectEngineContentInspectionTest03);
    UtRegisterTest("DetectEngineContentInspectionTest04",
                   DetectEngineContentInspectionTest04);
    UtRegisterTest("DetectEngineContentInspectionTest05",
                   DetectEngineContentInspectionTest05);
    UtRegisterTest("DetectEngineContentInspectionTest06",
                   DetectEngineContentInspectionTest06);
    UtRegisterTest("DetectEngineContentInspectionTest07",
                   DetectEngineContentInspectionTest07);
}

#undef TEST_HEADER
#undef TEST_RUN
#undef TEST_FOOTER