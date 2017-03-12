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
var qml = Script.resolvePath('').replace('.js','.qml');
var window = new OverlayWindow({
    title: 'Debug Window',
    source: qml,
    width: 400, height: Math.min(Overlays.height()*.8, 900),
});
window.setPosition(25, 50);
function killit() { if (window) { window=null; Script.stop(); } }
window.closed.connect(killit);
function onMessageReceived(channel, message, sender, local) {
    if (local && channel === 'debugWindow.js' && window) {
        window.sendToQml(message);
    }
}

function onFromQml(message) {
    var msg = JSON.parse(message);
    if (msg.settingName) {
        window.sendToQml('css=' + Settings.getValue(msg.settingName, msg.defaultStyle));
    }
}
Messages.messageReceived.connect(onMessageReceived);
Messages.subscribe('debugWindow.js');
Script.scriptEnding.connect(function() {
    Messages.unsubscribe('debugWindow.js');
    Messages.messageReceived.disconnect(onMessageReceived);
    window && window.fromQml && window.fromQml.disconnect(onFromQml);
    window && window.closed && window.closed();
});

window.fromQml.connect(onFromQml);

var sendToLogWindow = function(type, message, scriptFileName) {
    // special exception to trim jasmine.js unit test lines
    if (~message.indexOf('jasmine/jasmine.js')) {
        var cull = false, lastAt = -1;
        message = message.split('\n').reduce(function(out, line, i) {
            cull = cull || ~line.indexOf('jasmine/jasmine.js');
            if (!cull) {
                if (~line.indexOf(') at ')) {
                    lastAt = i;
                }
                out.push(line);
            }
            return out;
        }, []).slice(0, lastAt).join('\n');
    }
    window.sendToQml({
        tstamp: new Date,
        fileName: scriptFileName,
        type: type,
        message: message,
    });
};
'printed,warning,error,info'.split(',').forEach(function(type) {
    var prefix = type === 'printed' ? 'log' : type === 'warning' ? 'warn' : type;
    var signal = type + 'Message';
    ScriptDiscoveryService[signal].connect(logit);
    Script.scriptEnding.connect(function() {
        ScriptDiscoveryService[signal].disconnect(logit);
    });
    function logit(message, scriptFileName) {
        sendToLogWindow(prefix, message, scriptFileName);
    }
});
print('//debugWindow.js')
}()); // END LOCAL_SCOPE
