// Prepended to the generated strata.js (see CMakeLists.txt). Runs before the
// game starts: mounts the save directory on IndexedDB and provides the small
// browser services the C++ side calls (src/platform/window_web.cpp).
var Module = typeof Module !== 'undefined' ? Module : {};

Module.preRun = Module.preRun || [];
Module.preRun.push(function () {
  // /strata holds the settings and every world. IDBFS keeps a copy in
  // IndexedDB; load it before main() runs.
  FS.mkdir('/strata');
  FS.mount(IDBFS, {}, '/strata');
  addRunDependency('strata-storage');
  FS.syncfs(true, function (err) {
    if (err) console.warn('Strata: could not load saved data:', err);
    removeRunDependency('strata-storage');
  });
});

// Writes /strata back to IndexedDB. Calls made while a sync is running are
// merged into one more sync afterwards.
Module.strataSync = (function () {
  var busy = false;
  var again = false;
  return function sync() {
    if (busy) {
      again = true;
      return;
    }
    busy = true;
    FS.syncfs(false, function (err) {
      busy = false;
      if (err) console.warn('Strata: could not save to browser storage:', err);
      if (again) {
        again = false;
        sync();
      }
    });
  };
})();

// Fullscreen may only start during a user gesture. The game asks from its
// frame callback; if the browser refuses, retry on the next tap, click or key.
if (typeof window !== 'undefined') {
  var strataPendingFullscreen = false;
  Module.strataFullscreen = function (on) {
    var d = document;
    var el = d.documentElement;
    if (on) {
      if (d.fullscreenElement || d.webkitFullscreenElement) return;
      var request = el.requestFullscreen || el.webkitRequestFullscreen;
      if (!request) return;  // e.g. iPhone Safari: add the page to the home screen instead
      try {
        var p = request.call(el, { navigationUI: 'hide' });
        if (p && p.catch) p.catch(function () { strataPendingFullscreen = true; });
      } catch (e) {
        strataPendingFullscreen = true;
      }
    } else {
      strataPendingFullscreen = false;
      var exit = d.exitFullscreen || d.webkitExitFullscreen;
      if ((d.fullscreenElement || d.webkitFullscreenElement) && exit) exit.call(d);
    }
  };
  ['pointerup', 'touchend', 'keyup'].forEach(function (type) {
    window.addEventListener(type, function () {
      if (!strataPendingFullscreen) return;
      strataPendingFullscreen = false;
      Module.strataFullscreen(true);
    }, true);
  });
}
