// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.net.truth.UrlResponseInfoSubject.assertThat;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;
import org.chromium.net.CronetTestRule.RequiresMinApi;

import java.util.Arrays;

/**
 * Simple test for Brotli support.
 */
@RunWith(AndroidJUnit4.class)
@RequiresMinApi(5) // Brotli support added in API version 5: crrev.com/465216
@Batch(Batch.UNIT_TESTS)
public class BrotliTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

    private CronetEngine mCronetEngine;

    @Before
    public void setUp() throws Exception {
        assertThat(Http2TestServer.startHttp2TestServer(mTestRule.getTestFramework().getContext()))
                .isTrue();
    }

    @After
    public void tearDown() throws Exception {
        assertThat(Http2TestServer.shutdownHttp2TestServer()).isTrue();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBrotliAdvertised() throws Exception {
        mTestRule.getTestFramework().applyEngineBuilderPatch((builder) -> {
            builder.enableBrotli(true);
            CronetTestUtil.setMockCertVerifierForTesting(
                    builder, QuicTestServer.createMockCertVerifier());
        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).contains("accept-encoding: gzip, deflate, br");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBrotliNotAdvertised() throws Exception {
        mTestRule.getTestFramework().applyEngineBuilderPatch((builder) -> {
            CronetTestUtil.setMockCertVerifierForTesting(
                    builder, QuicTestServer.createMockCertVerifier());
        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getEchoAllHeadersUrl();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(callback.mResponseAsString).doesNotContain("br");
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testBrotliDecoded() throws Exception {
        mTestRule.getTestFramework().applyEngineBuilderPatch((builder) -> {
            builder.enableBrotli(true);
            CronetTestUtil.setMockCertVerifierForTesting(
                    builder, QuicTestServer.createMockCertVerifier());
        });

        mCronetEngine = mTestRule.getTestFramework().startEngine();
        String url = Http2TestServer.getServeSimpleBrotliResponse();
        TestUrlRequestCallback callback = startAndWaitForComplete(url);
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        String expectedResponse = "The quick brown fox jumps over the lazy dog";
        assertThat(callback.mResponseAsString).isEqualTo(expectedResponse);
        assertThat(callback.getResponseInfoWithChecks())
                .hasHeadersThat()
                .containsEntry("content-encoding", Arrays.asList("br"));
    }

    private TestUrlRequestCallback startAndWaitForComplete(String url) {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder builder =
                mCronetEngine.newUrlRequestBuilder(url, callback, callback.getExecutor());
        builder.build().start();
        callback.blockForDone();
        return callback;
    }
}
