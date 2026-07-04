module.exports = [
  {
    "type": "heading",
    "defaultValue": "Texas Songbird"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Settings"
      },
      {
        "type": "color",
        "messageKey": "HAND_HEX",
        "label": "Hand color",
        "defaultValue": "0xFF5500",
        "sunlight": true
      },
      {
        "type": "toggle",
        "messageKey": "MUTE",
        "label": "Mute bird calls",
        "description": "When on, calls never play (on the hour or on tap).",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save"
  }
];
