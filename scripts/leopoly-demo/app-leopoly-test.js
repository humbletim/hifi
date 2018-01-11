if (typeof LeopolyManager === 'object' && Settings.getValue('humbletim') && LeopolyManager.objectName === 'Q_OS_WIN') {
    __debug_mode__();
}

if (typeof Script === 'object') {
    var require = Script.require;
}

var hands = require('./virtual-hands.js'),
    ui = require('./virtual-ui.js'),
    cursor = require('./virtual-cursor.js'),
    helpers = require('./connection-helpers.js');

//ui.iconPath = 'http://10.10.10.102:8000/leopoly-icons';
ui.iconPath = Script.resolvePath('./icons');//'http://192.168.0.5:8000/leopoly-icons';

var aux = ui._getToolbar('leopoly.aux');

var context = {
    dirty: 0,
    atpURL: null,
    atpMesh: null,
    clientOnly: !Entities.serversExist(),
    demesh: demesh,
    resyncMesh: resyncMesh,
    uploadMesh: uploadMesh,
    selectEntity: selectEntity,
    updateMetadata: updateMetadata,
    createLocalMesh: createLocalMesh,
    initialize: function() {
        //Leopoly.toolName = 'sculpt';
        Leopoly.activeToolChanged(Leopoly.activeTool);
        Leopoly.colorChanged(Leopoly.color);
        Leopoly.radiusChanged(Leopoly.radius);
        Leopoly.strengthChanged(Leopoly.strength);
        Leopoly.mirrorChanged(Leopoly.mirror);
    },
};

if (typeof Leopoly !== 'object' || !Leopoly.supported) {
    throw new Error('!Leopoly.supported');
}

Object.defineProperty(context, 'webColor', {
    enumerable: true,
    configurable: true,
    get: function() {
        return '#'+[Leopoly.color.x,Leopoly.color.y,Leopoly.color.z].map(function(v) {
            return ('0'+(Math.round(v  *0xff ).toString(16))).substr(-2);
        }).join('');
    },
});

Script.setTimeout(function() { Assets.initializeCache(); }, 1000);

var hand = hands.dominant;

var STYLUS_DIMS = Vec3.multiply({ x: .25, y: 1, z: .25 }, .25);
var stylus = cursor.update({
    name: 'Leopoly.dominantHand',
    type: 'Shape',
    ignoreRayIntersection: true,
    registrationPoint: { x: .5, y: -.25, z: .5 },
    dimensions: STYLUS_DIMS,
    //type: 'shape', // to use overlay instead
    clientOnly: true,
    //lifetime: 3600,
});

var chooseMeshButton = ui.addAction({
    toolbar: aux,
    objectName: 'leopoly-chooseMeshButton',
    text: '(select)',
    icon: '#select',
    onToggled: function(isActive) {
        print('... select Mesh', isActive);
        this.editProperties({ text: isActive ? '(select)' : Leopoly.metadata.entityID });
    },
});
helpers.connect(Entities, 'mousePressOnEntity', null, function(uuid, evt) {
    if (chooseMeshButton.getProperties().isActive) {
        print('SELECTED', uuid);
        updateMetadata({ entityID: uuid });
        chooseMeshButton.editProperties({
            isActive: false,
            text: uuid || '(select)',
        });
    }
});

var sphereButton = ui.addAction({
    toolbar: aux,
    objectName: 'leopoly-sphereButton',
    icon: 'images/sixense-reticle.png',
    text: 'new sphere',
    onClicked: function() {
        if (!context.atpMesh || Window.confirm('start with new sphere?')) {
            updateMetadata({
                sphereID: Entities.addEntity(
                    { type:'Sphere', position:MyAvatar.position,dimensions:Vec3.ONE, lifetime: 3600 },
                    context.clientOnly),
            });
            Script.setTimeout(function() {
                selectEntity(Leopoly.metadata.sphereID);
            }, 1000);
        }
    },
});

if (0) var lightButton = ui.addAction({
    objectName: 'leopoly-lightButton',
    icon: '#light',
    text: '(light)',
    onClicked: function() {
        updateMetadata({
            lightID: Entities.addEntity(
                {
                    lifetime: 3600,
                "type": "Light",      "visible": 1,      "position": MyAvatar.position,
                "dimensions": {        "x": 25.086944580078125,        "y": 25.086944580078125,        "z": 25.086944580078125      },
                "color": {        "red": 255,        "green": 255,        "blue": 255      },
                "intensity": 50,
                "falloffRadius": 500.20000000298023224,
                "exponent": 0,
                "cutoff": 15,
            },
                context.clientOnly),
        });
    },
});

function createLocalMesh() {
    var eprops = Entities.getEntityProperties(Leopoly.metadata.entityID);
    if (!eprops.id) {
        throw new Error('invalid Leopoly.metadata.entityID:' + Leopoly.metadata.entityID);
    }
    updateMetadata({
        atpID: Entities.getEntityProperties(Leopoly.metadata.atpID).id || Entities.addEntity({
            type:'Model',
            position:eprops.position,// Vec3.sum(MyAvatar.position,Vec3.UNIT_Z),
            dimensions:Vec3.multiply(eprops.dimensions, 1.05), //Vec3.ONE,
            color:{red:0xff,green:0,blue:0},
            lifetime: 3600,
        }, context.clientOnly)
    });
    Entities.editEntity(Leopoly.metadata.atpID, {
        position:eprops.position,// Vec3.sum(MyAvatar.position,Vec3.UNIT_Z),
        dimensions:Vec3.multiply(eprops.dimensions, 1.05), //Vec3.ONE,
    });
    print('-------------------ATPID', Leopoly.metadata.atpID,
          JSON.stringify(Entities.getEntityProperties(Leopoly.metadata.atpID).position),
          JSON.stringify(Entities.getEntityProperties(Leopoly.metadata.atpID).dimensions)
         );
    Leopoly.update();
    //if (context.dirty) {
    print("RECACHING LOCAL MESH", context.dirty);
    recacheLocalMesh(Leopoly.metadata.atpID, null, publishButton.getProperties().isActive);
    // }
}

var resyncButton = ui.addAction({
    objectName: 'leopoly-resyncButton',
    icon: 'images/reload.svg',
    text: '(refresh)',
    onToggled: function() {},
    onClicked: function() {
        this.editProperties({ isActive: true });
        context.createLocalMesh();
    },
});

var activeToolButton = ui.addAction({
    objectName: 'leopoly-activeToolButton',
    icon: '#on',
    text: '(on)',
    onClicked: function() {
        var sculpting = Leopoly.inputMesh;//activeTool !== Leopoly.Tool.INVALID;
        print(
            'activeToolButton.clicked',
            sculpting ? 'stopAction' : 'startAction',
            Leopoly.activeToolName, Leopoly.activeTool
        );
        if (!sculpting) {
            context.selectEntity(Leopoly.metadata.entityID);
        } else {
            print('stop action result', Leopoly.stopAction());
            Leopoly.inputMesh = null;
        }
        print('//activeToolButton.clicked', 'toolName:'+Leopoly.toolName, 'activeToolName:'+Leopoly.activeToolName);
    },
});

    
var tip = { position: Vec3.ZERO, rotation: Quat.IDENTITY };

var updateThread = Script.setInterval(function update() {
    var position = hand.position;
    var rotation = hand.orientation;
    //DebugDraw.addMarker('dominantHand', rotation, position, Vec3.ONE);
    /*
    var pt = Reticle.position;
    var v = Controller.getViewportDimensions();
    var c = {x: v.x/2, y: v.y/2 };
    var cpt = {x: (pt.x-v.x/2), y: (pt.y-v.y/2) ,z: 0 };
    cpt = Vec3.multiply(cpt, 3/4);//2/3*1.111111111);
    cpt.x += c.x;
    cpt.y += c.y;
    var ray = Camera.computePickRay(cpt.x, cpt.y);
    
    DebugDraw.addMarker('pickRay', rotation, Entities.findRayIntersection(ray, true, hands.dominant.whitelist, hands.dominant.blacklist, true).intersection, Vec3.UNIT_Z);
*/
    tip.position = Vec3.mix(tip.position || position, position, .1);
    tip.rotation = Quat.mix(tip.rotation || rotation, rotation, .1);
    var dimensions = Vec3.multiply(STYLUS_DIMS, 1 + Leopoly.strength/.5 * .5);
    dimensions.x *= Math.max(.25, Leopoly.radius);
    dimensions.z *= Math.max(.25, Leopoly.radius);

    tip.newPosition = Vec3.sum(tip.position, Vec3.multiplyQbyV(tip.rotation, {x: 0, y:-dimensions.y/2, z: 0}));
    tip.last = tip.last || {};
    updateMetadata({
        rayIntersection: hand.rayIntersection,
        activeStrength: tip.lastTrigger * Leopoly.strength || undefined,
        activeRadius: tip.lastTrigger ? Leopoly.radius : undefined,
        activeMirror: tip.lastTrigger ? Leopoly.mirror : undefined,
        activeColor: tip.lastTrigger ? Leopoly.color : undefined,
    });
    if (tip.lastTrigger != hand.triggerValue ||
        tip.radius !== tip.last.radius ||
        tip.strength !== tip.last.strength ||
        !Vec3.withinEpsilon(tip.lastPosition, tip.newPosition, 0.005)) {
        tip.last.radius = Leopoly.radius;
        tip.last.strength = Leopoly.strength;
        tip.lastPosition = tip.newPosition;
        tip.lastTrigger = hand.triggerValue;
        //print(JSON.stringify(hand.rayIntersection,0,2));
        stylus.update({
            color: 1||Leopoly.activeTool === Leopoly.Tool.PAINT ? vecToRGB(Leopoly.color) : COLORS.black,
            position: tip.newPosition,
            registrationPoint: { x: .5, y: 0, z: .5 },
            rotation: tip.rotation,
            dimensions: dimensions,
            solid: hand.intersection && hand.intersection.intersects,
            alpha: hand.triggerPressed ? .25 : .75,
            text: hand.triggerPressed ? hand.triggerValue.toFixed(2) : Leopoly.activeToolName,
        });
    }

    //update.i = update.i || 0;
    //if (update.i++ % 10 === 0) {
    var props = Entities.getEntityProperties(Leopoly.metadata.entityID);
    var cursor = {
        position: Vec3.multiplyVbyV({x:-1,y:-1,z:-1}, Vec3.subtract(props.position||Vec3.ZERO, tip.position)),
        rotation: tip.rotation || Quat.multiply(Quat.inverse(props.rotation||Quat.IDENTITY), tip.rotation),
    };
    cursor.position = Vec3.sum(Vec3.multiplyQbyV(
        cursor.rotation,
        Vec3.sum({ x: 0, y: 0*Leopoly.radius, z: 0 }, Settings.getValue('app-leopoly-test/cursor.offset', {x:0,y:-.5,z:0}))
    ), cursor.position);
    Leopoly.cursor = cursor;
    (function() {
        var pose = Leopoly.actionPoses[0];
        if (pose) {
            var props = Entities.getEntityProperties(Leopoly.metadata.atpID);
            DebugDraw.addMarker(
                'actionPose',
                Quat.multiply(props.rotation, pose.rotation),
                Vec3.sum(Vec3.multiplyQbyV(
                    Quat.multiply(props.rotation, pose.rotation),
                    {x:0,y:-.01,z:0}
                ), Vec3.sum(props.position, pose.position)), 
                {x:1,y:0,z:1}
            );}
    })();
    Leopoly.update(true);
    0 && Settings.setValue('app-leopoly-test/dominantHand', {
        position: tip.position,
        rotation: tip.rotation,
    });
    //}
}, 1e6);
updateThread.stop();
updateThread.interval = 1000/60;

var COLORS = {
    white: { red: 0xff, green: 0xff, blue: 0xff },
    black: { red: 0x00, green: 0x00, blue: 0x00 },
    grey: { red: 0x99, green: 0x99, blue: 0x99 },
};

helpers.connect(Messages, 'messageReceived', this, function(c,m,s,l) {
    if (c === 'app-leopoly-test') {
        var obj = JSON.parse(m);
        print('messageReceived', JSON.stringify(obj,0,2));
        for (var p in obj) {
            Leopoly[p] = obj[p];
        }
    }
});

var sculpting = false;
function onActiveToolChanged(activeTool) {
    sculpting = !!Leopoly.inputMesh;//;
    print('onActiveToolChanged', Leopoly.activeToolName, 'sculpting?:'+sculpting);
    hand.blacklist = [ stylus.id ]; // prevent intersection with the stylus
    activeToolButton.editProperties({
        isActive: sculpting,
        text: sculpting ? '(on)' : '(off)',
    });
    sculpting  ? updateThread.start() : updateThread.stop();
    stylus.update({
        visible: sculpting,
        color: activeTool === Leopoly.Tool.PAINT ? vecToRGB(Leopoly.color) : COLORS.black,
    });
    if (!sculpting) {
        DebugDraw.removeMarker('dominantHand');
        DebugDraw.removeMarker('actionPose');
    }
    toolButtons.forEach(function(button) {
        button.editProperties({
            name: activeTool === button.tool ? '*'+button.name+'*' : button.name,
            isActive: Leopoly.tool === button.tool,
        });
    });
    if (sculpting && context.dirty && Leopoly.activeTool === Leopoly.Tool.INVALID) {
        Leopoly.update();
        var publish = publishButton.getProperties().isActive &&
            !resyncButton.getProperties().isActive;
        if (!context.atpMesh || publish) {
            recacheLocalMesh(Leopoly.metadata.atpID, null, publish);
        }
        var output = context.lastOutput;
        if (output) chooseMeshButton.editProperties({
            text: (output.numVertices/1024).toFixed(2)+'kv',
        });
    }
};

function onInputMeshChanged(inputMesh) {
    print('leopoly.inputMeshChanged', inputMesh);
    if (!Leopoly.inputMesh) {
        context.atpMesh = null;
    }
    onActiveToolChanged(Leopoly.activeTool);
}

helpers.connect(Leopoly, 'packetCountChanged', null, function(count) {
    print('leopoly.packetCountChanged', count, context.dirty);
    context.dirty++;
});
helpers.connect(Leopoly, 'geometryPacketChanged', null, function onGeometryPacketChanged(index, packet) {
    context.dirty++;
    onGeometryPacketChanged._changeto && Script.clearTimeout(onGeometryPacketChanged._changeto);
    onGeometryPacketChanged._changeto = Script.setTimeout(function() {
        onGeometryPacketChanged._changeto = 0;
        resyncMesh();
    }, 250);
});

helpers.connect(Leopoly, 'inputMeshChanged', null, onInputMeshChanged);
helpers.connect(Leopoly, 'activeToolChanged', null, onActiveToolChanged);
helpers.connect(Leopoly, 'colorChanged', null, function() {
    var web = context.webColor;
    colorButton.editProperties({
        text: web.substr(1).toUpperCase() || '(no color)',
        icon: 'data:image/svg+xml;xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128"><circle cx="64" cy="64" r="64" fill="white"/></svg>'.replace('white', web),
    });
    //if (Leopoly.tool === Lepooly.Tool.PAINT) {
    //   onActiveToolChanged(Leopoly.activeTool);
    //}
});

var strengthButton = ui.addAction({
    //toolbar: ui.actions,
    objectName: 'leopoly-strengthButton',
    text: 'strength',
    icon: '#strength',
    onClicked: function() {
        Leopoly.strength = Leopoly.strength === .1 ? .5 :
            Leopoly.strength === .5 ? 1 : .1;
    },
});
helpers.connect(Leopoly, 'strengthChanged', strengthButton, function(strength) {
    this.editProperties({ text: strength.toFixed(2) });
});

var radii = [ 0.03, 0.1, 0.25, 0.5, 1 ];
radii.current = 0;
var radiusButton = ui.addAction({
    objectName: 'leopoly-radiusButton',
    text: 'radius',
    icon: '#radius',
    onClicked: function() {
        Leopoly.radius = radii[radii.current++ % radii.length] || 1;
    },
});
helpers.connect(Leopoly, 'radiusChanged', radiusButton, function(radius) {
    this.editProperties({ text: radius.toFixed(2) });
});

var mirrorXButton = ui.addAction({
    objectName: 'leopoly-mirrorY',
    text: 'mirror\nY-axis',
    icon: '#mirror',
    onToggled: function(isActive) {
        var tmp = Leopoly.mirror;
        tmp.x = isActive;
        Leopoly.mirror = tmp;
    },
});
helpers.connect(Leopoly, 'mirrorChanged', mirrorXButton, function(mirror) {
    print('mirrorChanged', JSON.stringify(mirror));
    var mirrored = [mirror.x&&'X',mirror.y&&'Y',mirror.z&&'Z'].filter(Boolean).join('|');
    this.editProperties({ text: mirrored ? 'mirror: ' + mirrored : '(mirror)' });
});

helpers.connect(Controller, 'mousePressEvent', null, function onMousePressEvent(event) {
    if (event.isLeftButton) {
        Leopoly.startAction();
    }
});
helpers.connect(Controller, 'mouseReleaseEvent', null, function onMouseReleaseEvent(event) {
    if (event.isLeftButton) {
        Leopoly.stopAction();
    }
});

if (0) helpers.connect(Controller, 'mouseMoveEvent', null, function onMouseMoveEvent(event) {
    lastMouseEvent = event;
    if (event.isLeftButton) {
        if (Leopoly.tool !== Leopoly.Tool.INVALID && Leopoly.activeTool === Leopoly.Tool.INVALID) {
            Leopoly.startAction();//activeTool = Leopoly.tool;
        }
    } else if (!event.isLeftButton) {
        if (Leopoly.activeTool !== Leopoly.Tool.INVALID) {
            print('!leftbutton && activeTool', Leopoly.activeToolName);
            Leopoly.stopAction();//activeTool = Leopoly.Tool.INVALID;
        }
    }
});

var toolButtons = [0,1,2,3,4,5,6].map(function(id) {
    var name = Leopoly.Tool[id];
    var self = ui.addAction({
        toolbar: ui.actions,
        objectName: 'leopoly-toolButton-'+name,
        icon: '#'+name,
        text: name || 'name?',
        stableOrder: 100+id,
        onToggled: function(isActive) {
            print('onToggled', 'tool:'+name);
            if (Leopoly.tool === id) {
                Leopoly.tool = Leopoly.Tool.INVALID;
            } else {
                Leopoly.tool = id;
            }
        },
    });
    self.tool = id;
    self.name = name;
    return self;
});

var colorButton = ui.addAction({
    toolbar: ui.actions,
    objectName: 'leopoly-colorButton',
    text: 'color',
    //hoverIcon: 'https://upload.wikimedia.org/wikipedia/commons/c/c7/Blended_colour_wheel.svg',
    onClicked: function(isActive) {
        Leopoly.color = {x:Math.random(),y:Math.random(),z:Math.random()};
        print(JSON.stringify(Leopoly.color));
    },
});

var undoButton = ui.addAction({
    objectName: 'leopoly-undoButton',
    text: 'undo',
    icon: '#undo',
    onClicked: function() {
        print('... undo');
        Messages.sendLocalMessage('app-leopoly-test', JSON.stringify({ command: 'undo' }));
},
});
var redoButton = ui.addAction({
    objectName: 'leopoly-redoButton',
    text: 'redo',
    icon: '#redo',
    onClicked: function() {
        print('... redo');
        Messages.sendLocalMessage('app-leopoly-test', JSON.stringify({ command: 'redo' }));
    },
});
var publishButton = ui.addAction({
    objectName: 'leopoly-publishButton',
    text: 'auto-ATP',
    icon: '#atp',
    onToggled: function(isActive) {
        print('...auto-ATP', isActive);
    },
});
var uploadButton = ui.addAction({
    objectName: 'leopoly-uploadButton',
    text: 'putAsset',
    icon: '#upload',
    onClicked: function() {
        recacheLocalMesh(Leopoly.metadata.atpID, null, true);
    },
});

var wireframe = ui.addAction({
    toolbar: aux,
    objectName: 'leopoly-wireframeButton',
    text: 'wireframe',
    icon: '#wireframe',
    onToggled: function(isActive) {
        Render.getConfig("RenderMainView").getConfig('LightingModel').enableWireframe = isActive;
    },
});

// var normalsButton = ui.addAction({
//     toolbar: aux,
//     objectName: 'leopoly-normalsButton',
//     text: 'normals',
//     icon: '#normals',
//     onClicked: function() {
//         print('... normals');
//         Messages.sendLocalMessage('app-leopoly-test', JSON.stringify({ command: 'normals' }));
//     },
// });

helpers.connect(Leopoly, 'toolChanged', null, function(tool) {
    print('TOOL CHANGED', tool, Leopoly.toolName);
    if (0) chooseMeshButton.editProperties({
        text: Leopoly.toolName.toLowerCase() || '(no tool)',
    });
});

if (0)helpers.connect(Leopoly, 'cursorChanged', function(position, rotation) {
    //print('leopoly.cursorChanged -- stylus moved to:', JSON.stringify({ position: position, rotation: rotation }));
    Settings.setValue('app-leopoly-test/dominantHand', {
        position: position,
        rotation: rotation,
    });
});

function cleanup() {
    Leopoly.inputMesh = null;
    DebugDraw.removeMarker('dominantHand');
    DebugDraw.removeMarker('actionPose');
    Entities.editEntity(Leopoly.metadata.entityID, { visible: true });
    Entities.deleteEntity(Leopoly.metadata.atpID);
    Entities.deleteEntity(Leopoly.metadata.lightID);
    Entities.deleteEntity(Leopoly.metadata.sphereID);
}
Script.scriptEnding.connect(cleanup);

context.initialize();

helpers.connect(Entities, 'deletingEntity', null, function onDeletingEntity(uuid) {
    if (uuid === Leopoly.metadata.entityID) {
        print('Leopoly.metadata.entityID deleted', uuid);
        Leopoly.activeTool = Leopoly.Tool.INVALID;
        Leopoly.inputMesh = null;
        updateMetadata({ entityID: null });
        cleanup();
    }
});

helpers.connect(Leopoly, 'metadataChanged', null, function metadataChanged(metadata) {
    if (metadataChanged.last === metadata.entityID)
        return;
    metadataChanged.last = metadata.entityID;
    hand.whitelist = [ metadata.entityID ].filter(Boolean);
    print('hand.whitelist', hand.whitelist);
});

///////////////////////////////////////////////////////////////////////////

function vecToRGB(c) {
    return { red: c.x*0xff, green: c.y*0xff, blue: c.z*0xff, alpha: c.w };
}
function updateMetadata(metadata) {
    var tmp = Leopoly.metadata;
    var changed = false;
    for (var p in metadata) {
        if (metadata[p] !== tmp[p]) {
            tmp[p] = metadata[p];
            changed = true;
        }
    }
    if (changed) {
        Leopoly.metadata = tmp;
    }
    return tmp;
}

function demesh(delay) {
    delay = isFinite(delay) ? delay : 2000;//publishButton.getProperties().isActive ? 5000 : 1000;
    function reset() {
        demesh.timeout && Script.clearTimeout(demesh.timeout);
        demesh.timeout = 0;
        demesh._to && Script.clearTimeout(demesh._to);
        demesh._to = 0;
    }
    reset();
    if (Leopoly.metadata.atpID) {
        resyncButton.editProperties({ isActive: true });
        demesh.timeout = Script.setTimeout(function() {
            demesh.timeout = 0;
            done('Error: demesh -- timeout after ' + delay + 'ms');
        }, delay);
        return retry();
    }
    function done(err, result) {
        reset();
        resyncButton.editProperties({ isActive: false });
        if (err) throw new Error(err);
        context.atpMesh = result.meshes[0];
        print('-- context.atpMesh', context.atpMesh);
    }
    function retry() {
        demesh._to && Script.clearTimeout(demesh._to);
        demesh._to = Script.setTimeout(function() {
            demesh._to = 0;
            var props = Entities.getEntityProperties(Leopoly.metadata.atpID);
            print('FETCHING MESHES ENTITY...', props.id);
            if (!props.id) {
                return done('!props.id');
            }
            Model.getMeshes(Leopoly.metadata.atpID, function(a,b) {
                if (!a && b) { return done(a,b); }
                print('FETCH FAILED -- scheduling retry...',a,b);
                demesh._to = Script.setTimeout(retry, 1000);
            });
        }, 500);
    }
}

function recacheLocalMesh(id, mesh, publish){
    context.lastOutput = mesh = mesh || (LeopolyManager.objectName === 'Q_OS_WIN' ? Leopoly.outputMesh : Leopoly.inputMesh);
    var data = Model.meshToOBJ({meshes:[mesh]});
    Assets.compressData({
        data: data,
        level: -1,
    }, function(error, result) {
        if (error) throw new Error(error);
        context.atpOBJ = result.data;
        //context.atpOBJ = data;
        var url = 'atp:'+ Assets.hashDataHex(result.data);
        var atpURL = url + '.obj.gz';
        context.atpURL = atpURL+'#'+Date.now();//updateMetadata({ atpURL:  atpURL });
        print('SAVING TO CACHE AS ATP ASSEST...', context.atpURL);
        Script.setTimeout(function() {
            Assets.saveToCache(url, result.data);
        }, 1);
        Script.setTimeout(function() {
            //print('-------recacheLocalMesh', Leopoly.metadata.atpID, output, url);
            var resource = ModelCache.prefetch(context.atpURL);
            resource.timeout = Script.setTimeout(function() {
                print('timeout...', resource.state);
                resource.timeout = 0;
                resource.state === Resource.State.LOADED && resource.stateChanged(Resource.State.FAILED);
            }, 5000);
            function onStateChanged(state) {
                print('onStateChanged', state, resource.timeout, resource.state, [Resource.State.LOADED,Resource.State.FINISHED,Resource.State.FAILED]);
                if (state === Resource.State.FINISHED || state === Resource.State.FAILED) {
                    resource.timeout && Script.clearTimeout(resource.timeout);
                    resource.timeout = 0;
                    try { resource.stateChanged.disconnect(onStateChanged); } catch(e) {}
                    Script.setTimeout(function() {
                        if (publish) {
                            print('PUBLISHING TO ATP...', Leopoly.metadata.atpID, context.atpURL);
                            uploadMesh(demesh);
                        } else {
                            print('ASSIGNING CACHED ATP URL TO ENTITY...', Leopoly.metadata.atpID, context.atpURL);
                            Entities.editEntity(Leopoly.metadata.atpID, { modelURL: context.atpURL });
                            demesh();
                        }
                    }, 100);
                }
            }
            resource.stateChanged.connect(onStateChanged);
            // print('calling stateChanged with', resource.state);
            Script.setTimeout(function() {
                resource.stateChanged(resource.state);
            }, 500);
        }, 500);
        return atpURL;
    });
}


function selectEntity(entityID, cb) {
    Entities.editEntity(Leopoly.metadata.entityID, { visible: true });
    Leopoly.inputMesh = null;
    context.updateMetadata({ entityID: entityID });
    Model.getMeshes(entityID, function(err, result) {
        print('selectEntity getMeshes...', err, result);
        if (result) {
            Leopoly.inputMesh = result.meshes[0];
            Leopoly.importMesh(Leopoly.inputMesh);
            context.createLocalMesh();
            Entities.editEntity(Leopoly.metadata.entityID, { visible: false });
            //Leopoly.update(true);
        }
        cb && cb(err, result);
    });
}

var NOTHALF = Vec3.multiply(1-1e-6, Vec3.HALF); 
function resyncMesh(force) {
    if (!context.dirty && !force) {
        return false;
    }
    print('resyncMesh -- #dirty', context.dirty);//index, packet);
    context.dirty = 0;
    if (context.atpMesh) {
        resyncMesh.waiting && Script.clearTimeout(resyncMesh.waiting);
        var props = resyncMesh.props = resyncMesh.props || Entities.getEntityProperties(Leopoly.metadata.atpID, ['shapeType','dimensions']);
        Entities.editEntity(Leopoly.metadata.atpID, {
            //visible: false,
            shapeType: props.shapeType === 'box' ? 'sphere' : 'box',
            collisionless: true,
            //registrationPoint: Vec3.multiply(0.99, Vec3.HALF),
            dimensions: Vec3.multiply(.998, props.dimensions),
        });
        Model.replaceMeshData(context.atpMesh, context.lastOutput = Leopoly.outputMesh);
        resyncMesh.waiting = Script.setTimeout(function() {
            resyncMesh.waiting = 0;
            resyncMesh.props = null;
            //Script.setTimeout(function() {
            Entities.editEntity(Leopoly.metadata.atpID, {
                //visible: true,
                shapeType: props.shapeType === 'sphere' ? 'box' : 'sphere',
                //registrationPoint: Vec3.HALF,
                dimensions: props.dimensions,
            });
            //}, 10);
        }, 100);
        //Model.mapAttributeValues(context.atpMesh, function(obj) { return { normal: Vec3.UNIT_Z}; })
        return true;
    }
}

function uploadMesh(demesh) {
    resyncButton.editProperties({ isActive: true });
    print('... upload', typeof demesh);
    //Messages.sendLocalMessage('app-leopoly-test', JSON.stringify({ command: 'upload' }));
    Assets.putAsset({
        data: context.atpOBJ,
        //compress: true,
        path: '/test/'+Leopoly.metadata.entityID.replace(/[^A-Za-z0-9]/,'').slice(0,16)+'-leopoly.obj.gz',
    }, this, function(error, result) {
        print('/////uploadBytes .obj:'+context.atpOBJ.length+'/'+result.byteLength, error, result.url, result.hash);//JSON.stringify(result,0,2));
        if (error) {
            resyncButton.editProperties({ isActive: false });
            throw new Error(error);
        }
        print('ASSIGNING RESULT URL TO ENTITY', result.url, '('+context.atpURL+')');
        //Leopoly.metadata.atpID, context.atpURL, 'result.url ====== ' + result.url);
        Entities.editEntity(Leopoly.metadata.atpID, { modelURL: result.url+'#'+Date.now() });//context.atpURL });
        demesh(4000);
    });
}


function __debug_mode__() {
    Script.require.debug = true;
    LODManager.setAutomaticLODAdjust(false);
    with(Render.getConfig("RenderMainView").getConfig('LightingModel')) {
        enableAmbientLight = true;
        enableDirectionalLight = true;
        enablePointLight = true;
        enableSpotLight = true;
        enableWireframe = false;
        enableScattering = false;
        enableSpecular = false;
        enableMaterialTexturing = false;
        enableLightmap = false;
    }
}
