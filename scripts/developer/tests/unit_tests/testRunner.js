// Include testing library
Script.include('../../libraries/jasmine/jasmine.js');
Script.include('../../libraries/jasmine/hifi-boot.js')

// Include unit tests
Script.include('avatarUnitTests.js');
Script.include('bindUnitTest.js');
Script.include('entityUnitTests.js');
Script.include('../modules/testModuleSupport.js');

// invoke Script.stop (after any async tests complete)
jasmine.getEnv().addReporter({ jasmineDone: Script.stop });

// Run the tests
jasmine.getEnv().execute();
