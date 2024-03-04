// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef YUZU_USE_QT_WEB_ENGINE

#include "yuzu/util/url_request_interceptor.h"

UrlRequestInterceptor::UrlRequestInterceptor(QObject* p) : QWebEngineUrlRequestInterceptor(p) {}

UrlRequestInterceptor::~UrlRequestInterceptor() = default;

void UrlRequestInterceptor::interceptRequest(QWebEngineUrlRequestInfo& info) {
    const auto resource_type = info.resourceType();

    switch (resource_type) {
    case QWebEngineUrlRequestInfo::ResourceTypeMainFrame:
        requested_url = info.requestUrl();
        emit FrameChanged();
        break;
    case QWebEngineUrlRequestInfo::ResourceTypeSubFrame:
    case QWebEngineUrlRequestInfo::ResourceTypeXhr:
        emit FrameChanged();
        break;
    default:
        break;
    }
}

QUrl UrlRequestInterceptor::GetRequestedURL() const {
    return requested_url;
}

#endif
