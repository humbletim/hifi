// glue to automatically bootstrap Jasmine for either Node.js or Interface testing scenarios

try { console.info(process.title); } catch(e){}
// Node.js
if (typeof process === 'object' && process.title === 'node') {
    MODE = 'node';
    // for consistency this still uses the same local jasmine.js library
    var jasmineRequire = require('../../libraries/jasmine/jasmine.js');
    var jasmine = jasmineRequire.core(jasmineRequire);
    var env = jasmine.getEnv();
    var jasmineInterface = jasmineRequire.interface(jasmine, env);
    // expose describe(), it(), etc.
    for (var p in jasmineInterface)
        global[p] = jasmineInterface[p];
    try {
        // note: you can install the pretty-pretted reporter using:
        // $ cd scripts/developer/tests/modules
        // $ npm install
        env.addReporter(new (require('jasmine-console-reporter')));
    } catch(e) {
        // fallback to a boring reporter
        var stats = {
            status: {},
            specs: {},
        };
        env.addReporter({
            jasmineStarted: function(suiteInfo) { console.log('Running suite with ' + suiteInfo.totalSpecsDefined); },
            suiteStarted: function(spec) { console.log('Suite started: ' + spec.description + ' whose full description is: ' + spec.fullName); },
            specStarted: function(spec) { console.log('Spec started: ' + spec.description + ' whose full description is: ' + spec.fullName); },
            specDone: function(spec) {
                var statuses = stats.status[spec.status] = stats.status[spec.status] || [];
                statuses.push(spec.description);
                //console.log(spec);
            },
            suiteDone: function(spec) { console.log(stats); },
            jasmineDone: function() { console.log('Finished suite'); }
        });
    }
    // note: the next version of jasmine supports this sorta thing built-in
    // jasmine.configureDefaultReporter({
    //     timer: new jasmine.Timer(),
    //     print: function() {
    //         console.log(util.format.apply(this, arguments));
    //     },
    //     showColors: true,
    //     jasmineCorePath: jasmine.jasmineCorePath
    // });
    console.info('typeof expect: ' + typeof expect);

    // testing mocks
    Script = {
        setTimeout: setTimeout,
        clearTimeout: clearTimeout,
        resolvePath: function(id) {
            // this attempts to accurately emulate how Script.resolvePath works
            var trace = {}; Error.captureStackTrace(trace);
            var base = trace.stack.split('\n')[2].replace(/^.*[(]|[)].*$/g,'').replace(/:[0-9]+:[0-9]+.*$/,'');
            if (!id)
                return base;
            var rel = base.replace(/[^\/]+$/, id);
            console.info('rel', rel);
            return require.resolve(rel);
        },
        require: function(mod) {
            return require(Script.require.resolve(mod));
        }
    };
    Script.require.cache = require.cache;
    Script.require.resolve = function(mod) {
        if (mod === '.' || /^\.\.($|\/)/.test(mod))
            throw new Error("Cannot find module '"+mod+"' (is dir)");
        var path = require.resolve(mod);
        //console.info('node-require-reoslved', mod, path);
        try {
            if (require('fs').lstatSync(path).isDirectory()) {
                throw new Error("Cannot find module '"+path+"' (is directory)");
            }
            //console.info('!path', path);
        } catch(e) { console.info(e) }
        return path;
    };
    print = console.info.bind(console, '[print]');
} else {
    MODE = 'interface';
    // Interface Test mode
    Script.require('../../../system/libraries/utils.js');
    this.jasmineRequire = Script.require('../../libraries/jasmine/jasmine.js');
    Script.require('../../libraries/jasmine/hifi-boot.js')
    print('via ----------', typeof Function.prototype.bind);
    // polyfill console
    console = {
        log: print,
        info: print.bind(this, '[info]'),
        warn: print.bind(this, '[warn]'),
        error: print.bind(this, '[error]'),
        debug: print.bind(this, '[debug]'),
    };
}

