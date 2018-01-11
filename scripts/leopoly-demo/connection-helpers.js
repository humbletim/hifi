// QObject::connect like helper w/autodisconnect at script ending -- humbletim
//
// usage:
//    var con = connect(source, signal, target, slot);
//    disconnect(con); // or con.disconnect()

// example:
//    connect(Controller, 'mousePressEvent', null, function(event) { ... });

module.exports = {
    resolve: function(source, signal, target, slot) {
        return {
            source: source,
            signal: (source && source[signal]) || signal,
            target: target,
            slot: (target && target[slot]) || slot || target,
            toString: function() {
                return '[connection '+
                    'source='+(this.source.objectName||this.source.constructor.name)+'.'+(this.signal.name)+' '+
                    'target='+(this.dest&&(this.dest.objectName||this.dest.constructor.name))+'.'+(this.slot.name)+
                    ']';
            },
        };
    },
            
    wire: function(operation, source, signal, target, slot) {
        var context = arguments.length === 2 ? source : this.resolve(source, signal, target, slot);
        try {
            context.signal[operation](context.target, context.slot);
        } catch(e) {
            console.error('ERROR helpers.wire', operation, context);
        }
        return context;
    },
    connect: function(source, signal, target, slot) {
        var context = this.wire('connect', source, signal, target, slot);
        var self = this; // helper
        context.disconnect = function() {
            print('autodisconnect', this);
            self.wire('disconnect', context);
            try { Script.scriptEnding.disconnect(context, 'disconnect'); } catch(e) {}
        };
        Script.scriptEnding.connect(context, 'disconnect');
        return context;
    },
    disconnect: function(metaConnection) { return metaConnection.disconnect(); },
};
