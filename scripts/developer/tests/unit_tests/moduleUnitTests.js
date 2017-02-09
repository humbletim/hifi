// Script.require / JS Module tests

var isNode = typeof process === 'object' && process.title === 'node';

jasmin_polyfill();

if (isNode) {
    process.env.NODE_PATH = __dirname + '../modules';
    require('module').Module._initPaths();
} else {
    require = Script.require
}

// note: set these specializations to `xdescribe` (instead of `describe`) to temporarily disable the related specs
// network-enabled (slow) specs
describe.network = describe;
// node only specs
describe.node = isNode ? describe : xdescribe;
// interface only specs
describe.interface = isNode ? xdescribe : describe;

describe('require', function() {
    describe('resolve', function() {
        it('should resolve relative filenames', function() {
            var expected = Script.resolvePath('../modules/example.json');
            expect(require.resolve('../modules/example.json')).toEqual(expected);
        });
    });

    describe('JSON', function() {
        it('should import .json modules', function() {
            var example = require('../modules/example.json');
            expect(example.name).toEqual('Example JSON Module');
        });
        describe.interface('inteface', function() {
            describe.network('network', function() {
                it('should import #content-type=application/json modules', function() {
                    var results = require('https://jsonip.com#content-type=application/json');
                    expect(results.ip).toMatch(/^[.0-9]+$/);
                });
            });
        });

    });

    describe.interface('system', function() {
        it('require(id)', function() {
            expect(require('vec3')).toEqual(jasmine.any(Function));
        });
        it('require(id).function', function() {
            expect(require('vec3')().isValid).toEqual(jasmine.any(Function));
        });
    });

    describe('exceptions', function() {
        it('should reject blank "" module identifiers', function() {
            expect(function() {
                require.resolve('');
            }).toThrowError(/Cannot find/);
        });
        it('should reject excessive identifier sizes', function() {
            expect(function() {
                require.resolve(new Array(8193).toString());
            }).toThrowError(/Cannot find/);
        });
        it('should reject implicitly-relative filenames', function() {
            expect(function() {
                var mod = require.resolve('example.js');
            }).toThrowError(/Cannot find/);
        });
        it('should reject non-existent filenames', function() {
            expect(function() {
                var mod = require.resolve('./404error.js');
            }).toThrowError(/Cannot find/);
        });
        it('should reject identifiers resolving to a directory', function() {
            expect(function() {
                var mod = require.resolve('.');
                //console.warn('resolved(.)', mod);
            }).toThrowError(/Cannot find/);
            expect(function() {
                var mod = require.resolve('..');
                //console.warn('resolved(..)', mod);
            }).toThrowError(/Cannot find/);
            expect(function() {
                var mod = require.resolve('../');
                //console.warn('resolved(../)', mod);
            }).toThrowError(/Cannot find/);
        });
        if (typeof MODE !== 'undefined' && MODE !== 'node') {
            it('should reject non-system, extensionless identifiers', function() {
                expect(function() {
                    require.resolve('./example');
                }).toThrowError(/Cannot find/);
            });
        }
    });

    describe('cache', function() {
        it('should cache modules by resolved module id', function() {
            var value = new Date;
            var example = require('../modules/example.json');
            example['.test'] = value;
            var example2 = require('../../tests/modules/example.json');
            expect(example2).toBe(example);
            expect(example2['.test']).toBe(example['.test']);
        });
        it('should reload cached modules set to null', function() {
            var value = new Date;
            var example = require('../modules/example.json');
            example['.test'] = value;
            require.cache[require.resolve('../../tests/modules/example.json')] = null;
            var example2 = require('../../tests/modules/example.json');
            expect(example2).not.toBe(example);
            expect(example2['.test']).not.toBe(example['.test']);
        });
    });

    describe('cyclic dependencies', function() {
        describe('should allow lazy-ref require cycles', function() {
            const MODULE_PATH = '../modules/cycles/main.js';
            var main;
            beforeAll(function() {
                try { this._console = console; } catch(e) {}
                // for this test console.log is no-op'd so it doesn't disrupt the reporter output
                console = typeof console === 'object' ? console : { log: function() {} };
                Script.resetModuleCache();
            });
            afterAll(function() {
                console = this._console;
            });
            it('main requirable', function() {
                main = require(MODULE_PATH);
                expect(main).toEqual(jasmine.any(Object));
            });
            it('main with both a and b', function() {
                expect(main.a['b.done?']).toBe(true);
                expect(main.b['a.done?']).toBe(false);
            });
            it('a.done?', function() {
                expect(main['a.done?']).toBe(true);
            });
            it('b.done', function() {
                expect(main['b.done?']).toBe(true);
            });
        });
    });

    describe('JS', function() {
        it('should throw catchable, local file errors', function() {
            expect(function() {
                require('file:///dev/null/non-existent-file.js');
            }).toThrowError(/path not found|Cannot find.*non-existent-file/);
        });
        it('should throw catchable, invalid moduleId errors', function() {
            expect(function() {
                require(new Array(4096 * 2).toString());
            }).toThrowError(/invalid.*size|Cannot find.*,{30}/);
        });
        it('should throw catchable, unresolvable moduleId errors', function() {
            expect(function() {
                require('foobar:/baz.js');
            }).toThrowError(/could not resolve|Cannot find.*foobar:/);
        });

        describe.network('network', function() {
            // note: with retries these tests take at least 60 seconds each to timeout
            var timeout = 75 * 1000;
            it('should throw catchable host errors', function() {
                expect(function() {
                    var mod = require('http://non.existent.highfidelity.io/moduleUnitTest.js');
                    print("mod", Object.keys(mod));
                }).toThrowError(/error retrieving script .ServerUnavailable.|Cannot find.*non.existent/);
            }, timeout);
            it('should throw catchable network timeouts', function() {
                expect(function() {
                    require('http://ping.highfidelity.io:1024');
                }).toThrowError(/error retrieving script .Timeout.|Cannot find.*ping.highfidelity/);
            }, timeout);
        });
    });

    describe('entity', function() {
        xit('TODO: incorporate Entity scripting tests for modules', function() {});
    });
});

// allow for dual use/testing across Interface and Node.js
function atexit() {}
function jasmin_polyfill() {
    if (typeof describe !== 'function') {
        // allow direct invocation (via Node.js or as Client script)
        (typeof Script === 'object' ? Script : module).require('../modules/jasmine-isomorphic.js');
        atexit = function() { jasmine.getEnv().execute(); };
    }
    if (isNode) {
        Script.resetModuleCache = function() { module.require.cache = {}; };
    }
}
atexit();
//try { Test.quit(); } catch(e) {}
