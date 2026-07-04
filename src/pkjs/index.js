var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig, null, { autoHandleEvents: false });

Pebble.addEventListener('showConfiguration', function() {
  Pebble.openURL(clay.generateUrl());
});

function valueOf(setting) {
  return typeof setting === 'object' ? setting.value : setting;
}

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response) return;

  var settings = clay.getSettings(e.response, false);
  var dict = {};

  if (settings.HAND_HEX !== undefined) {
    dict.HAND_HEX = valueOf(settings.HAND_HEX);
  }
  if (settings.MUTE !== undefined) {
    dict.MUTE = valueOf(settings.MUTE) ? 1 : 0;
  }

  Pebble.sendAppMessage(dict);
});
