//
//  debugWindow.qml
//
//  Brad Hefta-Gaub, created on 12/19/2016.
//  Copyright 2016 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
// ================================================================================
//
// tim's changes:
//   * added support for "pop out / pop in" button
//   * added support for styling via various CSS-like rules
//
// Examples:
//   - White on black background, bolded 12pt DejaVu Sans Mono font:
//     var style = 'font: 12pt bold "DejaVu Sans Mono"; background-color: black; color: white;'
//
//   - Disable antialiasing, smoothing and word wrapping:
//     var style = 'white-space: nowrap; text-rendering: optimizeLegibility;'
//
//   - Display log entries as HTML (eg: print('<b>asdf</b> hi') will show in bold)
//     var style = '-hifi-format: html;'
//
//   NOTE: the style string needs to be set manually for now using this sorta thing:
//      Settings.setValue('debugWindow/style', style);
//   (which you can do in the Console... or Script Editor debug pane)
//
// List of supported CSS-like rules:
//     color: #abc;
//     background-color: #def;
//     font: NNpt bold italic 'Desired Font Family', FallbackFont, 'Comic Sans MS';
//       font-family: 'Desired Font Family', FallbackFont;
//       font-size: NNpt;
//       font-weight: bold;
//       font-style: italic;
//     white-space: nowrap;
//     text-rendering: optimizeLegibility;
//     -hifi-smooth: false; (same effect as text-rendering: optimizeLegibility)
//     -hifi-format: html;
//
// -- humbletim @ 2017.02.05

import QtQuick 2.5
import QtQuick.Controls 1.4
import QtQuick.Controls.Styles 1.4

Item {
    id: root
    width: parent ? parent.width : 100
    height: parent ? parent.height : 100

    property var textStyle: null
    property var iconFont: ''

    property var debug: false
    readonly property var settingName: "debugWindow/style"
    readonly property var defaultStyle: ('
        color: white;
        background-color: black;
        font: 12px "monospace";
        -hifi-format: html;
    ')

    Rectangle {
        id: logArea
        anchors.fill: parent
        property alias textArea: textArea
        Component.onCompleted: pollFont()
        Button {
            id: popoutButton
            visible: false
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: textArea.viewport.anchors.rightMargin
            anchors.bottomMargin: textArea.viewport.anchors.bottomMargin
            z: textArea.z + 1
            property var expand:   iconFont === 'FontAwesome' ? '\uf065' : '\u21f1'
            property var compress: iconFont === 'FontAwesome' ? '\uf066' : '\u21f2'
            text: popoutwin && popoutwin.visible ? compress : expand
            onClicked: popoutwin && popoutwin.visible ? popWindowIn() : popWindowOut()
            width: height
            style: ButtonStyle {
                label: Text {
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    anchors.fill: parent
                    font.family: root.iconFont
                    text: control.text
                    color: 'black'
                }
            }
        }

        TextArea {
            id: textArea
            text: "\n"
            anchors.fill: parent
            readOnly: true
            selectByMouse: true
            selectByKeyboard: true
            frameVisible: false
            antialiasing: !!smooth
            smooth: !!smooth
            style: textAreaStyle
            property var to
            onTextChanged: {
                if (wrapMode === Text.NoWrap)
                    flickableItem.contentX = 0;
            }
        }
    }

    readonly property var colors: ({
        debug: 'darkgray',
        log: 'gray',
        info: 'steelblue',
        warn: 'yellow',
        error: 'red',
    })

    property var bottomY: Math.floor(textArea.flickableItem.contentHeight - textArea.viewport.height);
    property var currentY: Math.floor(textArea.flickableItem.contentY)
    property var atBottom: bottomY <= 0 || Math.abs(currentY - bottomY) <= 16
    property var autoscroll: (textArea.flickableItem.atYBeginning || atBottom) && !textArea.selectedText.length
    property var buffered: ([])

    function fromScript(message) {
        //console.info('fromScript: ' + message);
        if (message === 'popout') {
            if (popoutwin && popoutwin.visible) {
                return;
            }
            popWindowOut()
            return;
        } else if (message === 'popin') {
            if (popoutwin && popoutwin.visible) {
                popWindowIn();
            }
            return;
        } else if (message === 'clear') {
            textArea.text = '\n';
        } else if (/^css=/.test(message)) {
            css = myDecodeStyles(defaultStyle+';'+message.substr("css=".length));
            desktopMode && debugDump(debug);
        }

        if (message && typeof message === 'object') {
            var tstamp = message.tstamp.toJSON().replace('T', ' ').replace(/Z$/,'')
            if (!desktopMode)
                tstamp = tstamp.replace(/^[^ ]+ /,'');
            var type = textArea.textFormat === Text.RichText ?
                "<font color=%1>%2</font>".arg(colors[message.type]).arg(message.type)
                : message.type;
            message = ("[%1] [%2] [%3] %4"
                       .arg(tstamp)
                       .arg(message.fileName)
                       .arg(type)
                       .arg(message.message));
        }
        var MAX_LINE_COUNT = 2000;
        var TRIM_LINES = 500;
        if (textArea.lineCount > MAX_LINE_COUNT) {
            var lines = textArea.text.split('\n');
            lines.splice(0, TRIM_LINES);
            textArea.text = lines.join('\n');
        }
        message = message + '';
        if (textArea.textFormat === Text.RichText) {
            function replaceIndent(line) {
                return line.replace(/^ +/, function(s) {
                    return s.replace(/ /g, '&nbsp;');
                });
            }
            message = message.split('\n').map(replaceIndent).join('<br />');
        }
        message = message.replace(/\n$/,'');
        buffered.push(message);
        if (autoscroll || buffered.length > 250 || bottomY <= 0 || (!desktopMode && bottomY <= 50)) {
            buffered.splice(0, buffered.length).forEach(function(message) {
                textArea.append(message);
            });
        } else console.info('>>>>' + JSON.stringify({bottomY:bottomY, atBottom: atBottom, currentY: currentY }));
    }

    // make textStyle.renderType available for debug printing..
    property Component textAreaStyle: TextAreaStyle {
        frame: Item { visible: false }
        Component.onCompleted: textStyle = this
    }

    // try to pull FontAwesome in from the local resources
    FontLoader { id: iconFontA; source: "../../../resources/fonts/fontawesome-webfont.ttf" }
    FontLoader { id: iconFontB; source: "../../../interface/resources/fonts/fontawesome-webfont.ttf" }
    property var fonts: [ iconFontA, iconFontB ]
    function pollFont() {
        pollFont.i = (pollFont.i||0) + 1;
        iconFont = fonts
            .filter(function(f) { return f.status === FontLoader.Ready })
            .map(function(f) { return f.name })[0];
        if (!iconFont && pollFont.i++ < 5)
            setTimeout(pollFont, 1000);
    }
    onIconFontChanged: console.info('iconFont', iconFont)

    // dynamic popout window support
    default property alias contents: root.children
    property var window: null
    property var popoutwin: null
    property var backupProperties: 'width,height,closable,destroyOnHidden,x,y,shown,scale'.split(',')
    property var backup: ({})
    Binding { target: root; property: 'window'; value: parent.parent; when: Boolean(parent.parent) }

    function popWindowIn() {
        console.info('popWindowIn', popoutwin.contents, window);
        root.contents = popoutwin.contents;
        popoutwin.window = null;
        window.visible = true;
        backupProperties.forEach(function(p) {
            try {
                window[p] = backup[p];
            } catch(e) {}
        });
        window.opacity = .9
        window.fadeIn();
        //textArea.flickableItem.contentY = textArea.flickableItem.contentHeight - textArea.viewport.height;
    }

    Keys.onPressed: {
        switch(event.key) {
        case Qt.Key_W:
            if (window && window.closable && (event.modifiers === Qt.ControlModifier)) {
                event.accepted = true;
                window && window.close && window.close();
                window && window.windowClosed && window.windowClosed();
            }
        }
    }

    Connections {
        target: popoutwin ? popoutwin : null
        ignoreUnknownSignals: true
        onClosing: {
            console.info('window.popoutwin.onClosing', popoutwin, popoutwin && popoutwin.window);
        }
        onClosed: {
            console.info('window.popoutwin.onClosed', popoutwin, popoutwin && popoutwin.window);
        }
        Component.onDestruction: {
            console.info('window.popoutwin.Component.onDestruction', popoutwin, popoutwin && popoutwin.window)
            if (popoutwin && popoutwin.window)
                popWindowIn();
        }
    }

    function popWindowOut() {

        backupProperties.forEach(function(p) {
                backup[p] = window[p];
            });

        try {
            window.x = -9999;
            window.y = -9999;
            window.scale = 0.01;
            window.width = window.height = 1;

            window.closable = false;
            window.destroyOnHidden = false;
        } catch(e) {
        }
        popoutwin = windowMaker.createObject(null, {
            window: window,
            contents: logArea
        });
        popoutwin.width = backup.width
        popoutwin.height = backup.height
    }
    property var css

    Binding { target: window;     property: 'width';          value: parseInt(css['width']); when: 'width' in css; }
    Binding { target: window;     property: 'height';         value: parseInt(css['height']); when: 'height' in css; }
    Binding { target: textStyle;  property: 'backgroundColor';value: css['background-color'] || hifi.colors.white; }
    Binding { target: textArea;   property: 'textColor';      value: css['color'] || hifi.colors.black; }
    Binding { target: textArea;   property: 'wrapMode';       value: css['white-space'] === 'nowrap' ? Text.NoWrap : Text.WordWrap; }
    Binding { target: textArea;   property: 'textFormat';     value: /html|rich/i.test(css['-hifi-format']) ? Text.RichText : Text.PlainText; }

    Binding { target: textArea;   property: 'font.bold';      value: /bold/.test([css['font-weight'],css['font']]); }
    Binding { target: textArea;   property: 'font.italic';    value: /italic/.test([css['font-weight'],css['font']]); }
    Binding { target: textArea;   property: 'font.family';    value: css['font-family']; when: css['font-family']; }
    Binding { target: textArea;   property: 'font.pixelSize'; value: parseFloat(css['font-size']); when: /px$/.test(css['font-size']) }
    Binding { target: textArea;   property: 'font.pointSize'; value: parseFloat(css['font-size']); when: /pt$/.test(css['font-size']) }

    Binding { target: textStyle;  property: 'renderType';     value: css['-hifi-smooth'] ? Text.QtRendering : Text.NativeRendering }
    Binding { target: textArea;   property: 'smooth';         value: css['-hifi-smooth'] }
    Binding { target: textArea;   property: 'antialiasing';   value: css['-hifi-smooth'] }

    function debugDump(debug) {
        if (debug) {
            //fromScript(window);
            if (window) {
                'width,height'.split(',').forEach(function(p) {
                    fromScript('window.'+ p + ': ' + window[p]);
                });
            }
            'textFormat,antialiasing,smooth,width,height'.split(',').forEach(function(p) {
                fromScript(p + ': ' + textArea[p]);
            });
            fromScript('renderType: '+ (textStyle.renderType === Text.NativeRendering ? 'NativeRendering' : 'QtRendering'));
            'family,pointSize,pixelSize,italic,bold'.split(',').forEach(function(p) {
                fromScript(p + ': ' + textArea.font[p]);
            });
        } else {
            fromScript('textArea.font.family: ' + textArea.font.family);
        }
        if (desktopMode)
            fromScript('css: ' + JSON.stringify(css,0,2));
    }
    property var desktopMode: true

    function _desktopMode() {
        return (!window || !/tabletroot/i.test(window));
    }
    Connections {
        target: root
        Component.onCompleted: {
            if (typeof Settings === 'object' && Settings.getValue)
                css = myDecodeStyles(Settings.getValue(settingName, defaultStyle));
            else
                sendToScript(JSON.stringify({ settingName: settingName, defaultStyle: defaultStyle }));
            desktopMode = _desktopMode();
            popoutButton.visible = !HMD.active && desktopMode;
            debugDump(debug);
        }
    }
    Component {
        id: windowMaker
        ApplicationWindow {
            id: win
            visible: true
            //flags: Qt.Tool | Qt.WindowDoesNotAcceptFocus
            title: 'Debug Log (external)'
            property var window: null
            onWindowChanged: {
                if (window)
                    console.info('poppping out window', window);
                else {
                    console.info('popping in window');
                    win.close();
                }
            }
            onClosing: {
                console.info('popoutwin.onClosing', window);
                window && window.close && window.close();
                window && window.windowClosed && window.windowClosed();
            }
            default property alias contents: placeholder.children
            Rectangle {
                id: placeholder
                anchors.fill: parent
                color: '#000'
            }
        }
    }

    Connections {
        target: window
        ignoreUnknownSignals: true
        onDestroyed: console.info('window.onDestroyed')
        onShownChanged: console.info('window.onShownChanged')
        onParentChanged: console.info('window.onParentChanged')
        onWindowClosing: console.info('window.windowClosing')
        onWindowClosed: console.info('window.windowClosed')
        onClosed: console.info('window.closed')
        Component.onDestruction: {
            console.info('window.onDestruction');
            if (popoutwin) popoutwin.window = null;
        }
    }

    onWindowChanged: {
        if (window) {
            if (window.x < 0) window.x = 0;
            if (window.y < 0) window.y = 0;
        }
        desktopMode = _desktopMode();
    }

    // helper functions

    // decode stuff like 'font-size: 12pt; color: #ff0; foo: bar;' into a POJO object
    function decodeStyles(css) {
        return (css+'').split(';').reduce(function(out, rule) {
            var kv = rule.split(':'),
                name = kv.shift().trim(),
                value = kv.join(':').trim();
            if (name) {
                out[name] = value === 'false' || value === 'none' ? false : value;
            }
            return out;
        }, {});
    }
    // apply special roll-ups for font and -hifi-smooth pseudo-style
    function myDecodeStyles(css) {
        var style = decodeStyles(css);
        style['font-family'] = style['font-family'] ||
            (style['font']+'').replace(/bold|italic|\b[.0-9]+(pt|px)/g,'').trim();
        style['font-size'] = style['font-size'] || ((style['font']+'').match(/[.0-9]+p[xt]\b/)||[])[0];

        if (!('-hifi-smooth' in style))
            style['-hifi-smooth'] = !/legibil/i.test(style['text-rendering']); // !== 'optimizeLegibility'
        if (!('-hifi-format' in style) && typeof Settings === 'object')
            style['-hifi-format'] = Settings.getValue('debugWindow/format', '');

        if (!desktopMode) {
            console.info('CLEARING WIDTH/HEIGHT bc TABLET ' + window);
            delete style['width']; delete style['height'];
            root.height = window.height;
            root.width = window.width;
            //css['height'] = window.height;
        }
        return style;
    }

    // humbletim's setTimeout implementation for QML!
    function setTimeout(fn, ms) {
        return setTimeoutish.createObject(null, {
            running: true,
            repeat: false,
            interval: ms,
            callback: fn
        });
    }
    Component {
        id: setTimeoutish
        Timer {
            id: delay
            interval: 1000
            running: false
            property var callback: null
            onTriggered: {
                //console.info('timer.timeout', typeof callback)
                stop();
                try {
                    callback && callback();
                } finally {
                    destroy();
                }
            }
        }
    }
}


