/* globals MyAvatar, Vec3, Uuid, Graphics, Quat, Script, Overlays, Entities, OverlayWebWindow,
    ScriptDiscoveryService, ModelCache, Resource */
/* eslint-env jasmine */
instrument_testrunner(true);

var testUtils = require_test_utils();

var cleanups = [];

function cleanup() {
    cleanups.splice(0, cleanups.length).forEach(function(fn) {
        fn();
    });
}
Script.scriptEnding.connect(cleanup);

var FBX = {
  xyzCross: {
    url: 'https://cdn.rawgit.com/highfidelity/hifi_tests/db68be5f/assets/models/collisions/xyzCross-three-submeshes.fbx',
    numMeshes: 3,
    numVertices: 439 * 3,
  },
  icosphere2x: {
    url: 'http://192.241.189.145:8083/hifi/dump/icosphere2x.fbx',
    numMeshes: 1,
    numVertices: 10242,
    sampleIndex: 1000,
    samplePosition: {
      x: 0.8006935715675354,
      y: 0.164432093501091,
      z: 0.5760658383369446
    },
  },
};
var SHAPE_VERTEX_COUNT = {
    cube: 24,
    icosahedron: 60,
    hexagon: 36,
    cylinder: 36,
};

const SHAPE_PROXY_PLUGIN_URI = 'plugin://entity::Shape::MeshTest';

describe("Graphics", function() {
    var ref = Camera; // MyAvatar
    var front = Vec3.sum(ref.position, Vec3.multiply(3, Quat.getForward(ref.orientation)));
    var position = Vec3.sum(front, {
        x: -5,
        y: 0,
        z: 0
    });

    function nextPosition() {
        return position = Vec3.sum(position, Vec3.UNIT_X);
    }

    it('Avatar', function() {
        var model = Graphics.getModel(MyAvatar.SELF_ID);
        expect(model.objectID).toEqual(MyAvatar.SELF_ID)
        expect(model.meshes.length).toBeGreaterThan(0);
    });
    it('Overlay', function() {
        var overlayID = Overlays.addOverlay('shape', {
            solid: true,
            shape: 'cylinder',
            position: nextPosition(),
            dimensions: Vec3.ONE,
            lifetime: 60
        });
        cleanups.push(function() {
            Overlays.deleteOverlay(overlayID);
        });
        //Script.setTimeout(function() {
        var model = Graphics.getModel(overlayID);
        expect(model.objectID).toEqual(overlayID)
        expect(model.meshes.length).toEqual(1);
        expect(model.meshes[0].numVertices).toEqual(SHAPE_VERTEX_COUNT.cylinder);
        //}, 1000);
    });
    it('Entity', function(done) {
        var entityID = Entities.addEntity({
            type: 'Box',
            position: nextPosition(),
            dimensions: Vec3.HALF,
            lifetime: 60
        }, true);
        cleanups.push(function() {
            Entities.deleteEntity(entityID);
        });
        Script.setTimeout(function() {
            var model = Graphics.getModel(entityID);
            expect(model.objectID).toEqual(entityID)
            expect(model.meshes.length).toEqual(1);
            expect(model.meshes[0].numVertices).toEqual(SHAPE_VERTEX_COUNT.cube);
            done();
        }, 1000);
    });
    it('Entity replace mesh', function(done) {
        testUtils.precacheThen(FBX.xyzCross.url, function(res) {
            var overlayID = Overlays.addOverlay('model', {
                url: res.url,
                position: nextPosition(),
                dimensions: Vec3.ONE,
            });
            var modelID = Entities.addEntity({
                type: 'Model',
                modelURL: res.url,
                position: nextPosition(),
                dimensions: Vec3.multiply(3, Vec3.ONE),
                lifetime: 600,
            }, true);
            var icosahedronID = Entities.addEntity({
                type: 'Shape',
                shape: 'Icosahedron',
                position: nextPosition(),
                dimensions: Vec3.HALF,
                lifetime: 60,
                color: { red: 100, green: 100, blue: 100 },
            }, true);
            var proxyIDs = [0,1,0].map(function(n) {
                var id = Entities.addEntity({
                    name: 'submesh-'+n,
                    type: 'Shape',
                    position: nextPosition(),
                    dimensions: Vec3.ONE,
                    color: { red: 255, green: 0, blue: 0 },
                    lifetime: 60,
                    angularVelocity: Vec3.UNIT_X,
                    angularDamping: 0,
                    dynamic: 1,
                }, true);
                expect(Graphics.setRenderPlugin(id, SHAPE_PROXY_PLUGIN_URI)).toEqual(true);
                return id;
            });
            cleanups.push(function() {
                Entities.deleteEntity(icosahedronID);
                Entities.deleteEntity(modelID);
                proxyIDs.forEach(Entities.deleteEntity);
                Overlays.deleteOverlay(overlayID);
            });
            Script.setTimeout(function() {
                expect(Graphics.canUpdateModel(overlayID)).toEqual(false);
                expect(Graphics.canUpdateModel(modelID)).toEqual(false);
                expect(Graphics.canUpdateModel(icosahedronID)).toEqual(false);
                proxyIDs.forEach(function(proxyID) {
                    expect(Graphics.canUpdateModel(proxyID)).toEqual(true);
                });
                
                var icosahedronModel = Graphics.getModel(icosahedronID);
                expect(icosahedronModel.objectID).toEqual(icosahedronID)
                expect(icosahedronModel.meshes.length).toEqual(1);
                expect(icosahedronModel.meshes[0].numVertices).toEqual(SHAPE_VERTEX_COUNT.icosahedron);

                var model = Graphics.getModel(modelID);
                expect(model.numMeshes).toEqual(FBX.xyzCross.numMeshes);
              
                // modify "red" axis submesh (changes should be reflected live)
                var mesh = model.meshes[2];
                mesh.fillAttribute('color', Vec3.HALF);
                mesh.scale({x:-1,y:-1,z:-1});
                mesh.translate({x:0, y:-120, z:0});

                model = model.cloneModel().scaleToFit(1);

                var newModel = Graphics.newModel([60, 120, 180].map(function(deg) {
                  return cloneRotateColor(model.meshes[0], { x: deg, y: 0,  z: 0 }, Vec3.multiply(deg/360, Vec3.ONE));
                }));
                print('newModel:' + newModel);
                Settings.setValue('modelID', modelID); /* globals Settings*/
              
                // note: can't update typical Shape entities
                expect(function() {
                    Graphics.updateModel(icosahedronID, newModel);
                }).toThrowError(/provider does not support updating/);

                // note: can update proxy Shape entities
                if (0)proxyIDs.forEach(function(proxyID) {
                    expect(function() {
                        Graphics.updateModel(proxyID, newModel);
                    }).not.toThrowError(/provider does not support updating/);
                });
                // individually update each submesh
                newModel.meshes.forEach(function(mesh, meshIndex) { 
                    //expect(Graphics.updateModel(modelID, newModel, meshIndex, 0)).toEqual(true);
                    expect(Graphics.updateModel(proxyIDs[meshIndex], newModel, meshIndex, 0)).toEqual(true);
                    expect(Graphics.overrideRenderFlags(proxyIDs[meshIndex], Graphics.RenderFlag.DIRTY)).toEqual(true);
                    print('meshes', meshIndex, proxyIDs[meshIndex]);
                });
                
              var sphere = Graphics.getModel(modelID).meshes[2].cloneMesh();
              sphere.rotateVec3Degrees({x: -45, y: -45, z: -45 });
              sphere.scale({x:-2,y:-2,z:-2});
              //expect(Graphics.updateModel(overlayID, Graphics.newModel([sphere]))).toEqual(true);
              
              newModel.meshes.forEach(function(mesh, meshIndex) {
                //mesh.scaleToFit(Vec3.ONE);
                //mesh.fillAttribute('color', Vec3.ZERO);//Vec3.multiply(Vec3.ONE, meshIndex/3)); // make sure color attribute exists
                var numUpdated = mesh.updateVertexAttributes(testUtils.colorizer);
                expect(numUpdated).toEqual(FBX.xyzCross.numVertices / 3);
              });
              expect(
                Graphics.getModel(modelID).updateVertexAttributes(testUtils.colorizer)
              ).toEqual(FBX.xyzCross.numVertices);
              done();
            }, 250);
        });
    });
    xit('Entity replace mesh II', function(done) {
        precacheThen(FBX.icosphere2x.url, function(res) {
            var modelID = Entities.addEntity({
                type: 'Model',
                modelURL: res.url,
                position: nextPosition(),
                dimensions: Vec3.multiply(.5, Vec3.ONE),
                lifetime: 600,
            }, true);
            cleanups.push(function() { Entities.deleteEntity(modelID); });
            Script.setTimeout(function() {
                expect(Graphics.canUpdateModel(modelID)).toEqual(true);
                var model = Graphics.getModel(modelID);
                expect(model.numMeshes).toEqual(FBX.icosphere2x.numMeshes);
                //model.meshes[0].fillattribute('color'
                function ripple(varying) {
                  return {
                    color: {
                      x: Math.sin(varying.normal.x),
                      y: varying.normal.y,
                      z: Math.cos(varying.normal.z),
                    },
                  };
                }
                expect(
                  Graphics.getModel(modelID).updateVertexAttributes(ripple)
                ).toEqual(FBX.icosphere2x.numVertices);
                done();
            }, 250);
        });
    });
    xit('Entity replace mesh III', function(done) {
        testUtils.precacheThen(FBX.icosphere2x.url, function(res) {
            var modelID = Entities.addEntity({
                type: 'Model',
                modelURL: res.url,
                position: nextPosition(),
                dimensions: Vec3.multiply(.5, Vec3.ONE),
                lifetime: 600,
            }, true);
            cleanups.push(function() { Entities.deleteEntity(modelID); });
            Script.setTimeout(function() {
                var model = Graphics.getModel(modelID);
                expect(model.numMeshes).toEqual(1);
                var positions = model.meshes[0].attributeToTypedArray('position');
                expect(positions.length).toEqual(FBX.icosphere2x.numVertices * 3);
                expect(positions[FBX.icosphere2x.sampleIndex * 3 + 0]).toEqual(FBX.icosphere2x.samplePosition.x);
                expect(positions[FBX.icosphere2x.sampleIndex * 3 + 1]).toEqual(FBX.icosphere2x.samplePosition.y);
                expect(positions[FBX.icosphere2x.sampleIndex * 3 + 2]).toEqual(FBX.icosphere2x.samplePosition.z);
                for (var i = 0; i < positions.length; i+=3) {
                  positions[i+0] += Math.sin(i) / 2.0;
                }
                return done();
              expect(model.meshes[0].attributeFromTypedArray('position', positions)).toEqual(true);
                done();
            }, 250);
        });
    });
});
 
function require_test_utils() {
  return {
    cloneRotateColor: cloneRotateColor,
    colorizer: colorizer,
    precache: precache,
    precacheThen: precacheThen,
  };
}

function colorizer(varying, index) {
  var position = Vec3.multiply(varying.position, .5);
  var color = Vec3.multiply(.1 + Math.sin(position.y*20) + Math.cos(position.z*20), Vec3.ONE);
  color.x *= Math.sin(position.x);
  position = Vec3.sum(Vec3.multiply(1, Vec3.multiply(.15 + color.x * 1, varying.normal)), position);
  return {
    color: color,
    normal: Vec3.normalize(position),
    position: position,
  };
}
      
function cloneRotateColor(mesh, rot, color) {
  mesh = mesh.cloneMesh();
  mesh.rotateVec3Degrees(rot);
  mesh.fillAttribute('color', color);
  return mesh;
}

function precacheThen(url, callback) {
  return precache(url + '#' + Date.now(), function(error, result) {
    if (error) throw error;
    callback(result);
  });
}

function precache(url, callback) {
    var res = ModelCache.prefetch(url);
    function once(state) {
        print(res.url, state);
        if (state === Resource.State.FAILED) {
            res.stateChanged.disconnect(once);
            callback(new Error('resource failed: ' + res.url), null);
        }
        if (state === Resource.State.FINISHED) {
            res.stateChanged.disconnect(once);
            callback(null, res);
        }
    }
    res.stateChanged.connect(once);
    res.stateChanged(res.state);
    return res;
}


// ----------------------------------------------------------------------------
// this stuff allows the unit tests to be loaded indepenently and/or as part of testRunner.js execution
function run() {}

function instrument_testrunner(force) {
    if (force || typeof describe === 'undefined') {
        var oldPrint = print;
        window = new OverlayWebWindow({
            title: 'graphicsUnitTests.js',
            width: 640,
            height: 128,
            source: 'about:blank',
        });
        window.setPosition(Window.innerWidth - window.size.x, 0);
        Script.scriptEnding.connect(window, 'close');
        ['printed','warning','error','info'].forEach(function(type) {
          Script[type+'Message'].connect(function(msg) {
             window.emitScriptEvent(['<div class='+type+'>'].concat(msg).concat('</div>').join(' ') + '');
          });
        });
        Script.unhandledException.connect(function onUnhandledException(err) {
          Script.unhandledException.disconnect(onUnhandledException);
          print('Unhandled Exception:' + err);
        });
        console.info('logging to test window...');
        window.closed.connect(Script, 'stop');
        // wait for window (ready) and test runner (ran) both to initialize
        var ready = false;
        var ran = false;
        window.webEventReceived.connect(function once(message) {
            // window.webEventReceived.disconnect(once);
            cleanup();
            if (message === 'ready') {
                ready = true;
                maybeRun();
            } else if (message === 'reload') {
                Script.load(Script.resolvePath('').split(/[?#]/)[0] + '#' + Date.now());
                Script.stop();
            }
        });
        run = function() {
            ran = true;
            maybeRun();
        };

        window.setURL([
            'data:text/html;text,',
            '<style>.error {color:red}</style>',
            '<body style="height:100%;width:100%;margin:0;background:#eee;whitespace:pre;font-size:12px;">',
            '<pre id=output></pre><div style="height:2px;"></div>',
            '<body>',
            '<script>(' + function() { /* globals window, EventBridge, output */
                window.addEventListener("DOMContentLoaded", function() {
                    setTimeout(function() {
                        EventBridge.scriptEventReceived.connect(function(msg) {
                            output.innerHTML += msg;
                            window.scrollTo(0, 1e10);
                            document.body.scrollTop = 1e10;
                        });
                        EventBridge.emitWebEvent('ready');
                    }, 1000);
                });
            } + ')();</script>',
            '<button style="position:fixed;top:0;right:0;" onclick="EventBridge.emitWebEvent(\'reload\')">rerun test...</button>'
        ].join('\n'));

        // Include testing library
        Script.include('/~/developer/libraries/jasmine/jasmine.js');
        Script.include('/~/developer/libraries/jasmine/hifi-boot.js');

        function maybeRun() {
            if (!ran || !ready) {
                return oldPrint('doRun -- waiting for both script and web window to become available');
            }
            if (!force) {
                // invoke Script.stop (after any async tests complete)
                jasmine.getEnv().addReporter({
                    jasmineDone: Script.stop
                });
            } else if (!maybeRun.first) {
                maybeRun.first = true;
                jasmine.getEnv().addReporter({
                    jasmineDone: function() {
                        print("JASMINE DONE");
                    }
                });
            }

            // Run the tests
            jasmine.getEnv().execute();
        };
    }
}
run();
