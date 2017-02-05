//
//  debugWindow.js
//
//  Brad Hefta-Gaub, created on 12/19/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() { // BEGIN LOCAL_SCOPE

// Set up the qml ui
var qml = Script.resolvePath('debugWindow.qml');
var window = new OverlayWindow({
    title: 'Debug Window',
    source: qml,
    width: 400, height: 900,
});
window.setPosition(25, 50);
function killit() { if (window) { window=null; Script.stop(); } }
window.closed.connect(killit);
Script.scriptEnding.connect(function() { window && window.windowClosed &&window.windowClosed(); });

var getFormattedDate = function() {
    var date = new Date();
    return date.getMonth() + "/" + date.getDate() + " " + date.getHours() + ":" + date.getMinutes() + ":" + date.getSeconds();
};

var sendToLogWindow = function(type, message, scriptFileName) {
    var typeFormatted = "";
    if (type) {
        typeFormatted = type + " - ";
    }
    window.sendToQml("[" + getFormattedDate() + "] " + "[" + scriptFileName + "] " + typeFormatted + message);
};
'printedMessage,warningMessage,errorMessage,infoMessage'.split(',').forEach(function(signal) {
    var prefix = signal === 'printedMessage' ? '' : signal.toUpperCase().replace(/message/i,'');
    ScriptDiscoveryService[signal].connect(logit);
    Script.scriptEnding.connect(function() {
        ScriptDiscoveryService[signal].disconnect(logit);
    });
        function logit(message, scriptFileName) {
            sendToLogWindow(prefix, message, scriptFileName);
        }
});
}()); // END LOCAL_SCOPE
