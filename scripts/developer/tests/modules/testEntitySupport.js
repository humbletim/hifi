var uuids = [];
var sampleScripts = [
    Script.require.resolve('./a/entityConstructor.js'),
    Script.require.resolve('./a/entityConstructorNested.js'),
    Script.require.resolve('./a/entityPreloadModule.js')
];

Script.setTimeout(function() {
    for(var i=0; i < 6; i++) {
        var position = MyAvatar.position;
        position.y -= i/2;
        uuids.push( Entities.addEntity({
            text: "Entity #" + i,
            type: 'Text',
            position: position,
            script: sampleScripts[ i % sampleScripts.length ],
            scriptTimestamp: +new Date,
            lifetime: 120,
            lineHeight: 1/8,
            dimensions: { x: 1, y: .5, z: .01 },
            backgroundColor: { red: 0, green: 0, blue: 0 },
            color: { red: 0xff, green: 0xff, blue: 0xff },
        }, !Entities.serversExist() || !Entities.canRezTmp()));
    }
}, 3000);

Script.scriptEnding.connect(function() {
    uuids.forEach(function(uuid) { Entities.deleteEntity(uuid); });
});
