// Copyright 2018 The Chromium Authors
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

import org.chromium.net.CronetTestRule.OnlyRunNativeCronet;

/**
 * Tests requests that generate Network Error Logging reports.
 */
@RunWith(AndroidJUnit4.class)
public class NetworkErrorLoggingTest {
    @Rule
    public final CronetTestRule mTestRule = CronetTestRule.withManualEngineStartup();

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
    public void testManualReportUpload() throws Exception {
        mTestRule.getTestFramework().applyEngineBuilderPatch(
                (builder)
                        -> CronetTestUtil.setMockCertVerifierForTesting(
                                builder, QuicTestServer.createMockCertVerifier()));

        String url = Http2TestServer.getReportingCollectorUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mTestRule.getTestFramework().startEngine().newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        TestUploadDataProvider dataProvider = new TestUploadDataProvider(
                TestUploadDataProvider.SuccessCallbackMode.SYNC, callback.getExecutor());
        dataProvider.addRead("[{\"type\": \"test_report\"}]".getBytes());
        requestBuilder.setUploadDataProvider(dataProvider, callback.getExecutor());
        requestBuilder.addHeader("Content-Type", "application/reports+json");
        requestBuilder.build().start();
        callback.blockForDone();
        dataProvider.assertClosed();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        assertThat(Http2TestServer.getReportingCollector().containsReport(
                           "{\"type\": \"test_report\"}"))
                .isTrue();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testUploadNELReportsFromHeaders() throws Exception {
        mTestRule.getTestFramework().applyEngineBuilderPatch((builder) -> {
            builder.setExperimentalOptions("{\"NetworkErrorLogging\": {\"enable\": true}}");
            CronetTestUtil.setMockCertVerifierForTesting(
                    builder, QuicTestServer.createMockCertVerifier());
        });
        String url = Http2TestServer.getSuccessWithNELHeadersUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mTestRule.getTestFramework().startEngine().newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        Http2TestServer.getReportingCollector().waitForReports(1);
        assertThat(Http2TestServer.getReportingCollector().containsReport(""
                           + "{"
                           + "  \"type\": \"network-error\","
                           + "  \"url\": \"" + url + "\","
                           + "  \"body\": {"
                           + "    \"method\": \"GET\","
                           + "    \"phase\": \"application\","
                           + "    \"protocol\": \"h2\","
                           + "    \"referrer\": \"\","
                           + "    \"sampling_fraction\": 1.0,"
                           + "    \"status_code\": 200,"
                           + "    \"type\": \"ok\""
                           + "  }"
                           + "}"))
                .isTrue();
    }

    @Test
    @SmallTest
    @OnlyRunNativeCronet
    public void testUploadNELReportsFromPreloadedPolicy() throws Exception {
        mTestRule.getTestFramework().applyEngineBuilderPatch((builder) -> {
            String serverOrigin = Http2TestServer.getServerUrl();
            String collectorUrl = Http2TestServer.getReportingCollectorUrl();
            builder.setExperimentalOptions(""
                    + "{\"NetworkErrorLogging\": {"
                    + "  \"enable\": true,"
                    + "  \"preloaded_report_to_headers\": ["
                    + "    {"
                    + "      \"origin\": \"" + serverOrigin + "\","
                    + "      \"value\": {"
                    + "        \"group\": \"nel\","
                    + "        \"max_age\": 86400,"
                    + "        \"endpoints\": ["
                    + "          {\"url\": \"" + collectorUrl + "\"}"
                    + "        ]"
                    + "      }"
                    + "    }"
                    + "  ],"
                    + "  \"preloaded_nel_headers\": ["
                    + "    {"
                    + "      \"origin\": \"" + serverOrigin + "\","
                    + "      \"value\": {"
                    + "        \"report_to\": \"nel\","
                    + "        \"max_age\": 86400,"
                    + "        \"success_fraction\": 1.0"
                    + "      }"
                    + "    }"
                    + "  ]"
                    + "}}");
            CronetTestUtil.setMockCertVerifierForTesting(
                    builder, QuicTestServer.createMockCertVerifier());
        });

        String url = Http2TestServer.getEchoMethodUrl();
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        UrlRequest.Builder requestBuilder =
                mTestRule.getTestFramework().startEngine().newUrlRequestBuilder(
                        url, callback, callback.getExecutor());
        requestBuilder.build().start();
        callback.blockForDone();
        assertThat(callback.getResponseInfoWithChecks()).hasHttpStatusCodeThat().isEqualTo(200);
        Http2TestServer.getReportingCollector().waitForReports(1);
        // Note that because we don't know in advance what the server IP address is for preloaded
        // origins, we'll always get a "downgraded" dns.address_changed NEL report if we don't
        // receive a replacement NEL policy with the request.
        assertThat(Http2TestServer.getReportingCollector().containsReport(""
                           + "{"
                           + "  \"type\": \"network-error\","
                           + "  \"url\": \"" + url + "\","
                           + "  \"body\": {"
                           + "    \"method\": \"GET\","
                           + "    \"phase\": \"dns\","
                           + "    \"protocol\": \"h2\","
                           + "    \"referrer\": \"\","
                           + "    \"sampling_fraction\": 1.0,"
                           + "    \"status_code\": 0,"
                           + "    \"type\": \"dns.address_changed\""
                           + "  }"
                           + "}"))
                .isTrue();
    }
}
