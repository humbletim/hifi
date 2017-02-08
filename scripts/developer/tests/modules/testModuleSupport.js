//WIP

jasmin_polyfill();

var require = Script.require;

describe('require.resolve tests', function() {
    it('should reject blank "" module identifiers', function() {
        expect(function() {
            require.resolve("");
        }).toThrowError(/Cannot find/);
    });
    if (typeof MODE !== 'undefined' && MODE !== 'node') {
        it('should reject non-system, extensionless identifiers', function() {
            expect(function() {
                require.resolve('./example');
            }).toThrowError(/Cannot find/);
        });
    }
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
            var mod = require.resolve(".");
            console.warn('resolved(.)', mod);
        }).toThrowError(/Cannot find/);
        expect(function() {
            var mod = require.resolve("..");
            console.warn('resolved(..)', mod);
        }).toThrowError(/Cannot find/);
        expect(function() {
            var mod = require.resolve("../");
            console.warn('resolved(../)', mod);
        }).toThrowError(/Cannot find/);
    });
    it('should resolve relative filenames', function() {
        expect(require.resolve('./example.json')).toEqual(Script.resolvePath('').replace(/[^\/]*$/,'example.json'));
    });
});

function atexit() {}
function jasmin_polyfill() {
    if (typeof describe !== 'function') {
        // allow direct invocation (via Node.js or as Client script)
        (typeof Script === 'object' ? Script : module).require('./jasmine-isomorphic.js');
        atexit = function() { jasmine.getEnv().execute(); };
    }
}
atexit();
//try { Test.quit(); } catch(e) {}
