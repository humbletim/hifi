function busywork() {
    for(var i=0; i < 3e7; i++);
}

var st = + new Date;

busywork();

Script.update.connect(function update() {
    print("#1 UPDATE!!!", +new Date - st);
    busywork();
    Script.update.disconnect(update);
});

Script.setImmediate(function() {
    print("#2 IMMEDIATE!!!", +new Date - st);
    busywork();
});

Script.setTimeout(function() {
    print("#3 TIMEOUT", +new Date - st);
    busywork();
}, 0);

var to = Script.setInterval(function() {
    print("#3.5 INTERVAL", +new Date - st);
    busywork();
    Script.clearInterval(to);
}, 0);

Script.setImmediate(function() {
  print("#4 IMMEDIATE!!!", +new Date - st);
    busywork();
});

Script.update.connect(function update() {
    print("#5 UPDATE!!!", +new Date - st);
    busywork();
    Script.update.disconnect(update);
});
Script.setImmediate(function() {
  print("#6 IMMEDIATE!!!", +new Date - st);
    busywork();
});

Script.setTimeout(function() {
  print("#7 TIMEOUT", +new Date - st);
    busywork();
}, 0);

Script.setImmediate(function() {
  print("#8 IMMEDIATE!!!", +new Date - st);
    busywork();
});

Script.clearImmediate(Script.setImmediate(function() {
    print("#9 SHOULD NOT SEE IMMEDIATE!!!", +new Date - st);
    busywork();
}));


Script.clearTimeout(Script.setTimeout(function() {
    print("#10 SHOULD NOT SEE TIMEOUT", +new Date - st);
    busywork();
}, 0))

Script.setTimeout(function() {
    print("#11 TIMEOUT", +new Date - st);
    busywork();
}, 0);

var to2 = Script.setInterval(function() {
    print("#12 INTERVAL", +new Date - st);
    busywork();
    Script.clearInterval(to2);
}, 0);
