// interim multi-toolbar virtual UI -- humbletim

var TOOLBAR_ID = 'com.highfidelity.interface.toolbar.leopoly2';
var ACTIONBAR_ID = 'com.highfidelity.interface.toolbar.leopoly3';

ToolbarButton.instances = {};
ToolbarButton.PROPERTIES = [
    'uuid','text','icon','isActive','objectName','visible','activeText','activeIcon',
    'hoverIcon','sortOrder','stableOrder',].filter(Boolean);

function ToolbarButton(parent, config) {
    var instances = ToolbarButton.instances[parent.objectName] =
        ToolbarButton.instances[parent.objectName] || [];
    config.uuid = config.objectName || Uuid.generate();
    config.objectName = config.objectName || config.uuid;
    config.sortOrder = config.sortOrder || instances.reduce(function(max, b) {
        // print('considering', b, b.config.sortOrder);
        return Math.max(max, b.config.sortOrder||0);
    }, 0)+1;
    print(config.text, '@'+config.sortOrder);
    config.stableOrder = config.sortOrder;//config.stableOrder || 0;
    for (var p in config) {
        if (/icon/i.test(p) && /^#/.test(config[p])) {
            var newValue = module.exports.iconPath + '/' + config[p].substr(1) + '.svg#'+Date.now();
            // print(config[p], '->', newValue);
            config[p] = newValue;
        }
    }
    //config.name = config.name || config.text || config.objectName;
    this.parent = parent;
    this.config = config;
    this.button = parent.addButton({ objectName: this.config.objectName });
    this.button.objectName = this.config.objectName;
    this.editProperties(this.config);
    // print('ToolbarButton created button:', this.config.objectName, this.button);
    instances.push(this);
    this.button.clicked.connect(this, 'onClicked');
    Script.scriptEnding.connect(this, 'dispose');
}
ToolbarButton.prototype = {
    getProperties: function() {
        var self = this;
        return ToolbarButton.PROPERTIES.reduce(function(out, p) {
            out[p] = self.button.readProperty(p);
            return out;
        }, { uuid: this.config.uuid });
    },
    editProperties: function(props) {
        for (var p in props) {
            if (~ToolbarButton.PROPERTIES.indexOf(p)) {
                // print('writeProperty', p, props[p]);
                this.button.writeProperty(p, props[p]);
            } else {
                // print('(writeProperty skipping)', p, typeof props[p] === 'function' ? '' : props[p]);
            }
                
        }
    },
    onClicked: function() {
        if (this.config.onClicked) {
            if (!this.config.onToggled) {
                this.editProperties({ isActive: true });
                var self = this;
                Script.setTimeout(function() {
                    self.editProperties({ isActive: false });
                }, 250);
            }
            this.config.onClicked.call(this, this.getProperties().isActive);
        } else {
            // if onClicked not specified then auto-toggle between active/inactive
            var isActive = !this.getProperties().isActive;
            this.editProperties({ isActive: !this.getProperties().isActive });
        }
        if (this.config.onToggled) {
            this.config.onToggled.call(this, this.getProperties().isActive);
        }
    },
    dispose: function() {
        if (!this.disposed) {
            // print('ToolbarButton dispose:', this.button);
            var instances = ToolbarButton.instances[this.parent.objectName] =
                ToolbarButton.instances[this.parent.objectName] || [];
            this.disposed = true;
            try { this.button.clicked.disconnect(this, 'onClicked'); } catch(e) { print('dispose error', e); }
            var idx = instances.indexOf(this);
            ~idx && instances.splice(idx, 1);
            this.parent && this.parent.removeButton(this.button.objectName);
            try { Script.scriptEnding.disconnect(this, 'dispose'); } catch(e) {}
        }
    },
};

function _getToolbar(id) {
    var toolbar = Toolbars.getToolbar(id);
    toolbar.objectName = id;
    return toolbar;
}

module.exports = {
    version: '0.0.0',
    _getToolbar: _getToolbar,
    toolbar: _getToolbar(TOOLBAR_ID),
    actions: _getToolbar(ACTIONBAR_ID),
    ToolbarButton: ToolbarButton,
    iconPath: '',
    instances: ToolbarButton.instances,
    addAction: function(config) { return new ToolbarButton(config.toolbar || this.toolbar, config); },
    cleanup: function() {
        for (var p in ToolbarButton.instances) {
            var buttons = ToolbarButton.instances[p].splice(0, ToolbarButton.instances[p].length);
            buttons.map(function(button) { return button.dispose(); });
        }
    },
};
    
Script.scriptEnding.connect(module.exports, 'cleanup');
