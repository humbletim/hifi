//
//  WebView.qml
//
//  Created by Bradley Austin Davis on 12 Jan 2016
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.5
import QtWebEngine 1.2
import HFWebEngineProfile 1.0

WebEngineView {
    id: root

    profile: desktop.browserProfile

    // Ensure the JS from the web-engine makes it to our logging
    Connections {
        target: root
        onJavaScriptConsoleMessage: {
            var category = [' info',' warn','error'][level] || level,
                src = (sourceID+''),
                parts = src.split(/[?#]/)[0].split(/[/]+/),
                basename = parts.pop(),
                scheme = parts.shift(),
                domainOrScheme = /^http/.test(scheme) ? parts.shift() : scheme;
            console.log(
                "Web Window [%1] | (%2) %3:%4 | %5"
                    .arg(category)
                    .arg(domainOrScheme)
                    .arg(basename)
                    .arg(lineNumber)
                    .arg(message)
            );
        }
    }

    onLoadingChanged: {
        // Required to support clicking on "hifi://" links
        if (WebEngineView.LoadStartedStatus == loadRequest.status) {
            var url = loadRequest.url.toString();
            if (urlHandler.canHandleUrl(url)) {
                if (urlHandler.handleUrl(url)) {
                    root.stop();
                }
            }
        }
    }
}
