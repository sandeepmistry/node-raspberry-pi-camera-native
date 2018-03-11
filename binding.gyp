{
  "targets": [
    {
      "target_name": "raspberry-pi-camera",
      "sources": [ "src/RaspberryPiCamera.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "/opt/vc/include"
      ],
      'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
      "libraries": [
        "-L/opt/vc/lib",
        "-lbcm_host",
        "-lmmal",
        "-lmmal_core"
      ]
    }
  ]
}
